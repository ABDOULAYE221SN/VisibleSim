/**
 * MonApplication_MultiKeyCode.cpp
 * Implémentation du protocole AKC-PM (Multi-Key) + Reconfiguration
 * Conforme à l'article : "A Lightweight Distributed Security Protocol with
 * Adaptive Key Management for Programmable Matter Based on Modular Robots"
 * Faye, Makhoul, Diene, Ouzzif — INISTA 2025
 */
#include "MonApplication_MultiKeyCode.hpp"
#include "../../simulatorCore/src/spongent160.h"
#include <algorithm>
#include <random>
#include <iomanip>

using namespace std;

// =============================================================================
// VARIABLES STATIQUES GLOBALES
// =============================================================================

// Formes initiale et cible
static const vector<Cell3DPosition> initialShape = {
    Cell3DPosition(1,3,2),  // M1
    Cell3DPosition(2,3,2),  // M2
    Cell3DPosition(1,2,2),  // M3
    Cell3DPosition(2,2,2),  // M4 = iM
    Cell3DPosition(3,2,2),  // M5
};
static const vector<Cell3DPosition> targetShape = {
    Cell3DPosition(2,3,2),  // M2 statique
    Cell3DPosition(2,2,2),  // M4 statique (iM)
    Cell3DPosition(3,2,2),  // M5 statique
    Cell3DPosition(3,3,2),  // M1 → ici
    Cell3DPosition(2,2,3),  // M3 → ici
};

// Planification reconfiguration
static queue<bID>            moveQueue;
static bool                  planReady   = false;
static map<bID, Cell3DPosition> assignments;

// Contrôle de phase
static set<bID>  authenticatedModules;
static bool      reconfigStarted = false;

// Statistiques globales
static uint64_t  timeAuthStart  = 0;
static uint64_t  timeAuthEnd    = 0;
static int       totalMessages  = 0;

static bool isInShape(const Cell3DPosition& p, const vector<Cell3DPosition>& s) {
    return find(s.begin(), s.end(), p) != s.end();
}

// Vérifie qu'une interface est valide pour envoyer un message
static bool ifaceValide(P2PNetworkInterface* iface, BuildingBlock* self) {
    if (!iface || !iface->connectedInterface) return false;
    if (!iface->connectedInterface->hostBlock) return false;
    // Éviter d'envoyer à soi-même (bug simulateur après mouvement)
    if (iface->connectedInterface->hostBlock == self) return false;
    if (iface->connectedInterface->hostBlock->blockId == self->blockId) return false;
    // Vérifier que la position du voisin est différente de la nôtre
    if (iface->connectedInterface->hostBlock->position == self->position) return false;
    return true;
}

// =============================================================================
// CONSTRUCTEUR / DESTRUCTEUR
// =============================================================================

MonApplication_MultiKeyCode::MonApplication_MultiKeyCode(Catoms3DBlock *host)
    : Catoms3DBlockCode(host), module(host) {}

MonApplication_MultiKeyCode::~MonApplication_MultiKeyCode() {}

// =============================================================================
// PRIMITIVES CRYPTOGRAPHIQUES
// Utilise SPONGENT-160 (160 bits = 20 octets) — Fonction de hachage légère
// Implémentation conforme à l'article CHES 2011
// =============================================================================

// H(data) → 160 bits  (SPONGENT-160 réel)
vector<uint8_t> MonApplication_MultiKeyCode::H(const vector<uint8_t>& data) {
    return Spongent::Spongent160::hash(data);
}

// H(uint64_t) → 160 bits  (surcharge pratique)
vector<uint8_t> MonApplication_MultiKeyCode::H(uint64_t val) {
    vector<uint8_t> v(8);
    for (int i = 0; i < 8; i++) v[i] = (uint8_t)((val >> (i*8)) & 0xFF);
    return H(v);
}

// L(i) : ligne i du code source → 256 bits (32 octets)
// Déterministe : tous les modules ont le même code → même L(i)
vector<uint8_t> MonApplication_MultiKeyCode::L(int ligne) {
    vector<uint8_t> res(TAILLE_LIGNE_BITS / 8);
    uint64_t g = 0xDEADBEEFULL ^ ((uint64_t)ligne * 0x12345678ULL);
    for (size_t i = 0; i < res.size(); i++) {
        g = g * 1103515245ULL + 12345ULL;
        res[i] = (uint8_t)((g >> 16) & 0xFF);
    }
    return res;
}

// HL(n) = H(L(n mod Nb)) → 160 bits  (conforme article)
vector<uint8_t> MonApplication_MultiKeyCode::HL(const vector<uint8_t>& n) {
    uint64_t val = 0;
    for (int i = 0; i < 8 && i < (int)n.size(); i++)
        val |= ((uint64_t)n[i]) << (i*8);
    return HL(val);
}

vector<uint8_t> MonApplication_MultiKeyCode::HL(uint64_t n) {
    return H(L((int)(n % NB_LIGNES_CODE)));
}

// XOR octet par octet (160 bits)
vector<uint8_t> MonApplication_MultiKeyCode::xorVec(const vector<uint8_t>& a, const vector<uint8_t>& b) {
    vector<uint8_t> res(a.size());
    for (size_t i = 0; i < a.size(); i++)
        res[i] = a[i] ^ b[i % b.size()];
    return res;
}

// Nonce aléatoire 160 bits
vector<uint8_t> MonApplication_MultiKeyCode::genererNonce160() {
    static random_device rd;
    static mt19937_64 gen(rd());
    uniform_int_distribution<uint64_t> dis;
    vector<uint8_t> n(HASH_OUTPUT_BYTES);
    for (int i = 0; i < HASH_OUTPUT_BYTES; i += 8) {
        uint64_t v = dis(gen);
        for (int j = 0; j < 8 && i+j < HASH_OUTPUT_BYTES; j++)
            n[i+j] = (uint8_t)((v >> (j*8)) & 0xFF);
    }
    return n;
}

// Ts = round(ts / DIVISEUR_SYNC) → vecteur 160 bits
vector<uint8_t> MonApplication_MultiKeyCode::tsVersVec(uint64_t ts) {
    uint64_t Ts = ts / DIVISEUR_SYNC;
    vector<uint8_t> v(HASH_OUTPUT_BYTES, 0);
    // Remplir les 8 premiers octets avec Ts
    for (int i = 0; i < 8; i++) v[i] = (uint8_t)((Ts >> (i*8)) & 0xFF);
    // Les 12 octets restants sont à 0 (padding pour atteindre 160 bits)
    // Ceci est conforme à l'article: Ts est une valeur temporelle qui tient sur 64 bits
    return v;
}

// =============================================================================
// UTILITAIRES
// =============================================================================

bool MonApplication_MultiKeyCode::estDansStructure() const {
    return infoSecurite.estModuleInitial ||
           infoSecurite.etat == ETAT_AUTHENTIFIE ||
           infoSecurite.etat == ETAT_CLE_ETABLIE;
}

void MonApplication_MultiKeyCode::definirCommeModuleInitial() {
    infoSecurite.estModuleInitial = true;
    infoSecurite.etat             = ETAT_AUTHENTIFIE;
    infoSecurite.liensStructure   = 0;
    module->setColor(BLUE);
    console << "[iM] Module " << module->blockId
            << " designe module initial — modele MULTI_KEY\n";
}

// Affiche les statistiques d'authentification uniquement
void MonApplication_MultiKeyCode::afficherStats() const {
    if (!infoSecurite.estModuleInitial) return;
    afficherStatsGlobal();
}

// Affiche les statistiques globales (peut être appelé par n'importe quel module)
void MonApplication_MultiKeyCode::afficherStatsGlobal() const {
    uint64_t duree = (timeAuthEnd > timeAuthStart) ? (timeAuthEnd - timeAuthStart) : 0;
    int totalModules = (int)BaseSimulator::getWorld()->buildingBlocksMap.size();
    
    // Utiliser cout avec flush pour s'assurer que la sortie est visible
    cout << "\n" << flush;
    cout << "╔════════════════════════════════════════════════════════════════╗" << endl;
    cout << "║         STATISTIQUES D'AUTHENTIFICATION AKC-PM                 ║" << endl;
    cout << "╠════════════════════════════════════════════════════════════════╣" << endl;
    cout << "║ Modèle de clé        : MULTI_KEY                               ║" << endl;
    cout << "║ Nombre de clés       : Variable (une par lien)                ║" << endl;
    cout << "║ Génération           : Ki+1 = H(L(Ki mod Nb))                 ║" << endl;
    cout << "║ Sécurité             : Clé unique par lien                     ║" << endl;
    cout << "╠════════════════════════════════════════════════════════════════╣" << endl;
    cout << "║ Modules authentifiés : " << setw(3) << authenticatedModules.size() 
         << " / " << setw(3) << totalModules << "                                ║" << endl;
    cout << "║ Messages échangés    : " << setw(6) << totalMessages << "                                  ║" << endl;
    cout << "║ Durée authentif.(µs) : " << setw(10) << duree << "                            ║" << endl;
    
    // Calcul du nombre moyen de messages par module
    double msgParModule = totalModules > 0 ? (double)totalMessages / totalModules : 0;
    cout << "║ Msg par module (moy) : " << fixed << setprecision(2) << setw(6) << msgParModule 
         << "                                  ║" << endl;
    
    // Calcul du temps moyen par module
    double tempsParModule = totalModules > 0 ? (double)duree / totalModules : 0;
    cout << "║ Temps par module(µs) : " << fixed << setprecision(2) << setw(10) << tempsParModule 
         << "                            ║" << endl;
    
    cout << "╠════════════════════════════════════════════════════════════════╣" << endl;
    cout << "║ Complexité Algo1     : O(m) — " << authenticatedModules.size() 
         << " exécutions                     ║" << endl;
    cout << "║ Overhead par lien    : 6H + 1H + 2Tx + 4XOR                    ║" << endl;
    cout << "╚════════════════════════════════════════════════════════════════╝" << endl;
    cout << "\n" << flush;
}

// =============================================================================
// STARTUP
// =============================================================================

void MonApplication_MultiKeyCode::startup() {
    // Module 1 planifie les déplacements (une seule fois)
    if (!planReady && module->blockId == 1) {
        vector<Cell3DPosition> toRemove, toFill;
        for (auto& p : initialShape)
            if (!isInShape(p, targetShape)) toRemove.push_back(p);
        for (auto& p : targetShape)
            if (!isInShape(p, initialShape)) toFill.push_back(p);

        auto world = BaseSimulator::getWorld();
        for (size_t i = 0; i < toRemove.size() && i < toFill.size(); i++) {
            for (auto& kv : world->buildingBlocksMap) {
                if (kv.second->position == toRemove[i]) {
                    assignments[kv.first] = toFill[i];
                    moveQueue.push(kv.first);
                    break;
                }
            }
        }
        planReady = true;
        timeAuthStart = getScheduler()->now();
    }

    // Le module en (2,2,2) est iM
    if (module->position == Cell3DPosition(2,2,2)) {
        definirCommeModuleInitial();
        // Compte iM comme authentifié
        authenticatedModules.insert(module->blockId);
        
        console << "[" << getScheduler()->now() << "] Module " << module->blockId 
                << ": Designe comme MODULE INITIAL (iM)\n";
        console << "    Position: (2,2,2)\n";
        console << "    Modele: AKC-PM (MULTI_KEY)\n";
        console << "    Role: Generer n2 et authentifier les nouveaux modules\n";
        
        // Diffuse STRUCTURE_READY via interruption (après que tous les startup() soient appelés)
        getScheduler()->schedule(
            new InterruptionEvent<unsigned int>(getScheduler()->now(), module, 0));
    } else {
        module->setColor(GREY);
        console << "[" << getScheduler()->now() << "] Module " << module->blockId 
                << ": En attente du signal STRUCTURE_READY...\n";
    }

    // Initialiser la cible si ce module doit bouger
    if (planReady && assignments.count(module->blockId))
        myTarget = assignments[module->blockId];
}

// =============================================================================
// ALGORITHME 1 : Authentification d'un nouveau module nM vers la structure
// Conforme à l'article Section VI.A — Algorithm 1
// =============================================================================

/**
 * SF-1 + SF-2 : nM génère n0 (160b), calcule :
 *   n1 = H(n0)                    — empreinte d'authentification
 *   K0 = H(L(n0 mod Nb))          — preuve d'exécution du même code
 *   x  = Ts XOR n0                — protège n0 (Ts = round(ts/10))
 * Envoie (liens, n1, x, K0) à iM/sM
 */
void MonApplication_MultiKeyCode::algorithme1_Initier(P2PNetworkInterface* dest) {
    if (voisinsAuthentifies.count(dest) || authentificationsEnCours.count(dest)) return;

    // SF-1
    infoSecurite.n0 = genererNonce160();
    infoSecurite.n1 = H(infoSecurite.n0);
    infoSecurite.K0 = HL(infoSecurite.n0);

    // x = Ts XOR n0  (Ts sur 160 bits, zéros au-delà de 8 octets)
    uint64_t ts = getScheduler()->now();
    vector<uint8_t> Ts_vec = tsVersVec(ts);
    vector<uint8_t> x      = xorVec(Ts_vec, infoSecurite.n0);

    int liens = infoSecurite.liensStructure;

    // Affichage détaillé dans la console
    console << "[" << ts << "] Module " << module->blockId 
            << ": ALGORITHME 1 - INITIATION (SF-1, SF-2)\n";
    console << "    Position: (" << module->position[0] << "," 
            << module->position[1] << "," << module->position[2] << ")\n";
    console << "    Voisins connectés: " << liens << "\n";
    console << "    n0 = " << (infoSecurite.n0[0] % 1000) << "... (généré)\n";
    console << "    n1 = H(n0) = " << (infoSecurite.n1[0] % 1000) << "...\n";
    console << "    K0 = HL(n0 mod Nb) = " << (infoSecurite.K0[0] % 1000) << "...\n";
    console << "    Ts = " << (ts / DIVISEUR_SYNC) << ", x = Ts XOR n0\n";
    console << "    Message AUTH_REQUEST envoyé (liens=" << liens << ")\n";

    // SF-2 : envoi (liens, n1, x, K0)
    sendMessage(new MessageDemandeAuth(liens, infoSecurite.n1, x, infoSecurite.K0),
                dest, DELAI_TRANSMISSION, 0);
    totalMessages++;

    n0EnAttente[dest] = infoSecurite.n0;
    authentificationsEnCours.insert(dest);
    infoSecurite.etat = ETAT_AUTHENTIFICATION;
    module->setColor(ORANGE);
}

/**
 * SF-3 + SF-4 : iM/sM reçoit (liens, n1, x, K0), vérifie sync + code, génère n2
 *   Ts' = round((Tr - Δt) / 10)
 *   n0' = x XOR Ts'
 *   Vérifie : n1 == H(n0')  et  K0 == H(L(n0' mod Nb))
 *   Génère n2 (MULTI_KEY : iM génère n2, puis le propage aux sM authentifiés)
 *   x1 = n2 XOR n0'
 *   K1 = H(L(n2 mod Nb))
 * Envoie x1 à nM
 */
void MonApplication_MultiKeyCode::algorithme1_Verifier(P2PNetworkInterface* src, int liens,
                                               const vector<uint8_t>& n1,
                                               const vector<uint8_t>& x,
                                               const vector<uint8_t>& K0) {
    if (voisinsAuthentifies.count(src)) return;
    if (!estDansStructure()) {
        console << "[Algo1 SF-3] Module " << module->blockId << " REJETE: pas dans structure\n";
        return;
    }

    uint64_t Tr = getScheduler()->now();
    console << "[" << Tr << "] Module " << module->blockId 
            << ": Reception AUTH_REQUEST\n";
    console << "    Deja dans la structure\n";
    console << "    liens=" << liens << "\n";

    // SF-3 : récupérer n0' en testant Ts' ± TOLERANCE_SYNC
    // Ts' = round((Tr - Δt) / DIVISEUR_SYNC) — conforme article
    uint64_t Ts_base = (Tr - DELAI_TRANSMISSION) / DIVISEUR_SYNC;
    vector<uint8_t> n0_prime;
    bool syncOK = false;
    int offset_trouve = 0;

    for (int off = -TOLERANCE_SYNC; off <= TOLERANCE_SYNC && !syncOK; off++) {
        uint64_t Ts_test = Ts_base + (uint64_t)off;
        // Construire le vecteur 160 bits directement depuis Ts_test (déjà en unités Ts)
        vector<uint8_t> Ts_vec(HASH_OUTPUT_BYTES, 0);
        for (int i = 0; i < 8; i++)
            Ts_vec[i] = (uint8_t)((Ts_test >> (i * 8)) & 0xFF);
        vector<uint8_t> n0_test = xorVec(x, Ts_vec);
        if (H(n0_test) == n1) {
            n0_prime = n0_test;
            syncOK   = true;
            offset_trouve = off;
        }
    }
    
    if (!syncOK) {
        console << "    ECHEC: Synchronisation impossible (Ts hors tolerance)\n";
        return;
    }
    
    console << "    n0 = x XOR Ts' (offset=" << offset_trouve << ")\n";
    console << "    Ts_base = (Tr - Δt)/10 = " << Ts_base << "\n";

    // SF-3 : vérifier K0 = H(L(n0' mod Nb))  — preuve du même code
    if (HL(n0_prime) != K0) {
        console << "    ECHEC: Verification K0 (code different)\n";
        return;
    }
    console << "    VERIFICATION REUSSIE\n";
    console << "    - Synchronisation OK\n";
    console << "    - Code identique (K0 verifie)\n";

    // SF-4 : MULTI_KEY — Gestion de n2
    if (infoSecurite.n2.empty()) {
        if (infoSecurite.estModuleInitial) {
            infoSecurite.n2 = genererNonce160();
            console << "    Module iM genere n2 = " << (infoSecurite.n2[0] % 1000) 
                    << "... (MULTI_KEY)\n";
        } else {
            console << "    ERREUR: n2 absent (sM non authentifie)\n";
            return;
        }
    }

    // MULTI_KEY: Générer une nouvelle clé à partir de la dernière
    vector<uint8_t> Ki;
    
    if (infoSecurite.derniereClé.empty()) {
        // Première clé: K1 = H(L(n2 mod Nb))
        Ki = HL(infoSecurite.n2);
        infoSecurite.compteurClés = 1;
        console << "    MULTI_KEY: K1 = H(L(n2 mod Nb)) generee\n";
        console << "    Premiere cle du module\n";
    } else {
        // Nouvelle clé: Ki+1 = H(L(Ki mod Nb))
        Ki = HL(infoSecurite.derniereClé);
        infoSecurite.compteurClés++;
        console << "    MULTI_KEY: K" << infoSecurite.compteurClés 
                << " = H(L(K" << (infoSecurite.compteurClés-1) 
                << " mod Nb)) generee\n";
        console << "    Nouvelle cle pour nouveau lien\n";
    }
    
    // Stocker la nouvelle clé
    infoSecurite.derniereClé = Ki;

    vector<uint8_t> x1 = xorVec(infoSecurite.n2, n0_prime);

    clesVoisins[src]          = Ki;
    voisinsAuthentifies[src]  = true;
    voisinsDansStructure[src] = true;
    infoSecurite.liensStructure++;

    console << "    Cle unique K" << infoSecurite.compteurClés 
            << " etablie pour ce lien\n";
    console << "    Message KEY_CHALLENGE envoye (x1 = n2 XOR n0)\n";
    
    sendMessage(new MessageDefiCle(x1), src, DELAI_TRANSMISSION, 0);
    totalMessages++;
}

/**
 * SF-5 : nM reçoit x1, dérive n2 et K1
 *   n2 = x1 XOR n0
 *   K1 = H(L(n2 mod Nb))
 * → nM et iM partagent K1 (MULTI_KEY)
 * IMPORTANT: nM stocke n2 pour pouvoir authentifier de futurs voisins (modèle MULTI_KEY)
 */
void MonApplication_MultiKeyCode::algorithme1_Completer(P2PNetworkInterface* src,
                                                const vector<uint8_t>& x1) {
    if (voisinsAuthentifies.count(src)) return;
    if (!n0EnAttente.count(src)) return;

    uint64_t t = getScheduler()->now();
    console << "[" << t << "] Module " << module->blockId 
            << ": Reception KEY_CHALLENGE (SF-5)\n";

    vector<uint8_t> n0 = n0EnAttente[src];
    vector<uint8_t> n2 = xorVec(x1, n0);
    
    // MULTI_KEY: Calculer K1 = H(L(n2 mod Nb))
    vector<uint8_t> K1 = HL(n2);

    console << "    n2 = x1 XOR n0 = " << (n2[0] % 1000) << "...\n";
    console << "    MULTI_KEY: K1 = H(L(n2 mod Nb)) calculee\n";
    console << "    Cle unique pour ce lien\n";

    // MULTI_KEY: Stocker n2 pour pouvoir authentifier de futurs modules
    infoSecurite.n2           = n2;
    infoSecurite.K1           = K1;
    infoSecurite.derniereClé  = K1;
    infoSecurite.compteurClés = 1;
    clesVoisins[src]          = K1;
    voisinsAuthentifies[src]  = true;
    voisinsDansStructure[src] = true;
    infoSecurite.etat         = ETAT_CLE_ETABLIE;
    infoSecurite.liensStructure++;

    n0EnAttente.erase(src);
    authentificationsEnCours.erase(src);

    console << "    AUTHENTIFICATION REUSSIE\n";
    console << "    Cle K1 etablie (MULTI_KEY)\n";
    console << "    Nouvelles cles generees: Ki+1 = H(L(Ki mod Nb))\n";
    
    // MULTI_KEY: Définir une couleur unique pour chaque clé
    // Utiliser un tableau de couleurs prédéfinies
    const Color couleursMultiKey[] = {CYAN, YELLOW, MAGENTA, ORANGE, LIGHTGREEN, 
                                       PINK, GOLD, LIGHTBLUE, PURPLE, BROWN};
    const string nomsCouleursMultiKey[] = {"CYAN", "YELLOW", "MAGENTA", "ORANGE", "LIGHTGREEN",
                                            "PINK", "GOLD", "LIGHTBLUE", "PURPLE", "BROWN"};
    int indexCouleur = (infoSecurite.compteurClés - 1) % 10;
    Color couleurCle = couleursMultiKey[indexCouleur];
    module->setColor(couleurCle);
    console << "    Couleur du module: K" << infoSecurite.compteurClés 
            << " (" << nomsCouleursMultiKey[indexCouleur] << ")\n";

    // Enregistrer ce module comme authentifié (une seule fois)
    if (!authenticatedModules.count(module->blockId)) {
        authenticatedModules.insert(module->blockId);
        int total = (int)BaseSimulator::getWorld()->buildingBlocksMap.size();
        console << "    [" << authenticatedModules.size() << "/" << total
                << "] modules authentifies\n";
    }

    // Propager STRUCTURE_READY aux voisins non encore authentifiés
    console << "    Diffusion STRUCTURE_READY\n";
    notifierVoisinsStructurePrete();

    // Déclencher la reconfiguration quand tous sont authentifiés
    int total = (int)BaseSimulator::getWorld()->buildingBlocksMap.size();
    if ((int)authenticatedModules.size() >= total && !reconfigStarted) {
        timeAuthEnd = getScheduler()->now();
        
        // Le dernier module à s'authentifier affiche les statistiques
        afficherStatsGlobal();
        
        lancerReconfiguration();
    }
}

// =============================================================================
// ALGORITHME 2 : Authentification entre deux modules déjà dans la structure
// Conforme à l'article Section VI.C — Algorithm 2
// Exemple : 1M veut se connecter à 2M, iM est leur voisin commun
// =============================================================================

/**
 * SF-1 + SF-2 : 1M choisit n0, calcule n1=H(n0), x=K1 XOR n0
 *   → envoie (liens, x)  à iM  (pour propagation vers sM)
 *   → envoie (liens, n1) à 2M  (preuve directe)
 */
void MonApplication_MultiKeyCode::algorithme2_Initier(P2PNetworkInterface* dest) {
    if (!ifaceValide(dest, module)) return;
    if (voisinsAuthentifies.count(dest) || authentificationsEnCours.count(dest)) return;
    if (!estDansStructure()) return;

    // Trouver l'interface vers iM (premier voisin authentifié ET valide)
    P2PNetworkInterface* ifaceIM = nullptr;
    for (auto& kv : voisinsAuthentifies)
        if (kv.second && ifaceValide(kv.first, module)) { ifaceIM = kv.first; break; }
    if (!ifaceIM) {
        console << "[Algo2 SF-2] Module " << module->blockId << " ERREUR: pas de voisin iM valide\n";
        return;
    }

    vector<uint8_t> n0 = genererNonce160();
    vector<uint8_t> n1 = H(n0);
    vector<uint8_t> K1_iM = clesVoisins[ifaceIM];
    vector<uint8_t> x  = xorVec(K1_iM, n0);

    int liens = infoSecurite.liensStructure;

    console << "[Algo2 SF-2] Module " << module->blockId
            << " → iM+2M | liens=" << liens << "\n";

    // Envoi (liens, x) à iM pour propagation
    sendMessage(new MessageRelaiPreuve(liens, x, module->blockId), ifaceIM, DELAI_TRANSMISSION, 0);
    totalMessages++;
    // Envoi (liens, n1) directement à 2M
    sendMessage(new MessagePreuveMembership(liens, n1), dest, DELAI_TRANSMISSION, 0);
    totalMessages++;

    n0Algo2EnAttente[dest] = n0;
    authentificationsEnCours.insert(dest);
}

/**
 * SF-3 iM : reçoit (liens, x, sourceId)
 *   n0 = K1_src XOR x
 *   Pour chaque voisin sM : xv = n0 XOR Kv  → envoie (liens-1, xv) à sM
 * Conforme à l'article: propage à tous les voisins connectés (pas seulement authentifiés)
 */
void MonApplication_MultiKeyCode::algorithme2_Relayer(P2PNetworkInterface* src, int liens,
                                              const vector<uint8_t>& x, bID sourceId) {
    if (!estDansStructure()) return;
    if (!clesVoisins.count(src)) return;

    vector<uint8_t> K1_src = clesVoisins[src];
    vector<uint8_t> n0     = xorVec(K1_src, x);

    console << "[Algo2 SF-3 relay] Module " << module->blockId << " propage n0 aux voisins\n";

    if (liens > 0) {
        for (int i = 0; i < 12; i++) {
            P2PNetworkInterface* iface = module->getInterface(i);
            if (!ifaceValide(iface, module) || iface == src) continue;
            
            // Conforme à l'article: propager à tous les voisins connectés
            // Utiliser la clé partagée si elle existe, sinon K1 (MULTI_KEY)
            vector<uint8_t> Kv;
            if (clesVoisins.count(iface)) {
                Kv = clesVoisins[iface];
            } else if (!infoSecurite.K1.empty()) {
                // MULTI_KEY: utiliser K1 pour les voisins non encore authentifiés
                Kv = infoSecurite.K1;
            } else {
                // Pas de clé disponible, ne pas propager à ce voisin
                continue;
            }
            
            vector<uint8_t> xv = xorVec(n0, Kv);
            sendMessage(new MessageRelaiPreuve(liens - 1, xv, sourceId),
                        iface, DELAI_TRANSMISSION, 0);
            totalMessages++;
        }
    }
}

/**
 * SF-3 2M : reçoit (liens, n1) de 1M — stocke n1 en attente du relai d'iM
 */
void MonApplication_MultiKeyCode::algorithme2_Verifier(P2PNetworkInterface* src, int /*liens*/,
                                               const vector<uint8_t>& n1) {
    // Stocker n1 associé à src (1M) — la vérification se fait dans algorithme2_Completer
    n0EnAttente[src] = n1;
    console << "[Algo2 SF-3] Module " << module->blockId << " n1 recu de 1M, attente relai\n";
}

/**
 * SF-3→SF-5 2M : reçoit le relai d'iM contenant xv = n0 XOR K2
 *   n0 = xv XOR K2
 *   Vérifie H(n0) == n1 stocké
 *   Choisit K2_new, calcule x1 = K2_new XOR n0, K3 = HL(K2_new)
 *   Envoie x1 à 1M
 */
void MonApplication_MultiKeyCode::algorithme2_Completer(P2PNetworkInterface* src,
                                                const vector<uint8_t>& xv) {
    if (!clesVoisins.count(src)) return;

    vector<uint8_t> K2     = clesVoisins[src];
    vector<uint8_t> n0     = xorVec(xv, K2);
    vector<uint8_t> n1_calc = H(n0);

    // Chercher le n1 stocké correspondant à H(n0)
    P2PNetworkInterface* iface1M = nullptr;
    for (auto& kv : n0EnAttente) {
        if (kv.second == n1_calc) { iface1M = kv.first; break; }
    }
    if (!iface1M) {
        console << "[Algo2 SF-3] Module " << module->blockId << " ECHEC: n1 non trouve\n";
        return;
    }
    console << "[Algo2 SF-3] Module " << module->blockId << " Auth 1M OK\n";
    n0EnAttente.erase(iface1M);

    // SF-5 : générer K2_new et K3
    vector<uint8_t> K2_new = genererNonce160();
    vector<uint8_t> x1_out = xorVec(K2_new, n0);
    vector<uint8_t> K3     = HL(K2_new);

    clesVoisins[iface1M]          = K3;
    voisinsAuthentifies[iface1M]  = true;
    voisinsDansStructure[iface1M] = true;
    infoSecurite.liensStructure++;

    console << "[Algo2 SF-5] Module " << module->blockId << " K3 generee → envoi x1 a 1M\n";
    sendMessage(new MessageDefiCle(x1_out), iface1M, DELAI_TRANSMISSION, 0);
    totalMessages++;
}

// =============================================================================
// GESTION DE LA STRUCTURE
// =============================================================================

void MonApplication_MultiKeyCode::notifierVoisinsStructurePrete() {
    uint64_t t = getScheduler()->now();
    int nbNotifications = 0;
    
    for (int i = 0; i < 12; i++) {
        P2PNetworkInterface* iface = module->getInterface(i);
        if (!ifaceValide(iface, module)) continue;
        if (voisinsAuthentifies.count(iface) || authentificationsEnCours.count(iface)) continue;
        authentificationsEnCours.insert(iface);
        try {
            sendMessage(new MessageStructurePrete(), iface, DELAI_TRANSMISSION, 0);
            totalMessages++;
            nbNotifications++;
        } catch (...) {
            authentificationsEnCours.erase(iface);
        }
    }
    
    if (nbNotifications > 0) {
        console << "[" << t << "] Module " << module->blockId 
                << ": Diffusion STRUCTURE_READY\n";
        console << "    Interface 0 à " << (nbNotifications-1) << "\n";
    }
}

void MonApplication_MultiKeyCode::verifierNouveauxVoisins() {
    if (!estDansStructure()) return;
    for (int i = 0; i < 12; i++) {
        P2PNetworkInterface* iface = module->getInterface(i);
        if (!ifaceValide(iface, module)) continue;
        if (voisinsAuthentifies.count(iface) || authentificationsEnCours.count(iface)) continue;
        // Marquer immédiatement pour éviter les envois en doublon
        authentificationsEnCours.insert(iface);
        try {
            sendMessage(new MessageStructurePrete(), iface, DELAI_TRANSMISSION, 0);
            totalMessages++;
            console << "[VerifVoisins] Module " << module->blockId
                    << " notifie nouveau voisin via iface " << i << "\n";
        } catch (...) {
            authentificationsEnCours.erase(iface);
        }
    }
}

void MonApplication_MultiKeyCode::lancerReconfiguration() {
    reconfigStarted = true;

    cout << "\n=== DEBUT RECONFIGURATION ("
         << authenticatedModules.size() << "/"
         << BaseSimulator::getWorld()->buildingBlocksMap.size()
         << " modules authentifies) ===\n";

    if (moveQueue.empty()) {
        cout << "  Aucun module a deplacer.\n";
        return;
    }

    bID nextId = moveQueue.front();
    auto nextBlock = BaseSimulator::getWorld()->getBlockById(nextId);
    if (!nextBlock) {
        cout << "  ERREUR: bloc " << nextId << " introuvable\n";
        return;
    }
    MonApplication_MultiKeyCode* nc = (MonApplication_MultiKeyCode*)nextBlock->blockCode;
    cout << "  Module " << nextId << " : ("
         << nc->module->position[0] << "," << nc->module->position[1] << ","
         << nc->module->position[2] << ") → ("
         << nc->myTarget[0] << "," << nc->myTarget[1] << "," << nc->myTarget[2] << ")\n";
    nc->visited.clear();
    nc->visited.insert(nc->module->position);
    nc->moveSteps = 0;
    nc->tryMoveToward(nc->myTarget);
}

// =============================================================================
// GESTIONNAIRES DE MESSAGES
// =============================================================================

void MonApplication_MultiKeyCode::surReceptionDemandeAuth(shared_ptr<Message> msg,
                                                  P2PNetworkInterface* src) {
    auto* m = static_cast<MessageDemandeAuth*>(msg.get());
    algorithme1_Verifier(src, m->liens, m->n1, m->x, m->K0);
}

void MonApplication_MultiKeyCode::surReceptionDefiCle(shared_ptr<Message> msg,
                                              P2PNetworkInterface* src) {
    auto* m = static_cast<MessageDefiCle*>(msg.get());
    if (n0EnAttente.count(src)) {
        // Réponse Algo1 SF-4 → SF-5
        algorithme1_Completer(src, m->x1);
    } else if (n0Algo2EnAttente.count(src)) {
        // Réponse Algo2 SF-5 → SF-6 : 1M reçoit x1 de 2M
        vector<uint8_t> n0     = n0Algo2EnAttente[src];
        vector<uint8_t> K2_new = xorVec(m->x1, n0);
        vector<uint8_t> K3     = HL(K2_new);

        clesVoisins[src]          = K3;
        voisinsAuthentifies[src]  = true;
        voisinsDansStructure[src] = true;
        infoSecurite.etat         = ETAT_CLE_ETABLIE;
        infoSecurite.liensStructure++;
        n0Algo2EnAttente.erase(src);
        authentificationsEnCours.erase(src);

        console << "[Algo2 SF-6] Module " << module->blockId << " K3 partagee avec 2M\n";
        module->setColor(GREEN);
        notifierVoisinsStructurePrete();
    }
}

void MonApplication_MultiKeyCode::surReceptionStructurePrete(shared_ptr<Message> /*msg*/,
                                                     P2PNetworkInterface* src) {
    voisinsDansStructure[src] = true;
    if (voisinsAuthentifies.count(src) || authentificationsEnCours.count(src)) return;

    uint64_t t = getScheduler()->now();
    
    if (!estDansStructure()) {
        // Nouveau module → Algorithme 1 (authentification avec la structure)
        console << "[" << t << "] Module " << module->blockId 
                << ": Reception STRUCTURE_READY\n";
        console << "    Deja dans la structure\n";
        console << "    Lancement ALGORITHME 1 (authentification)\n";
        algorithme1_Initier(src);
    } else {
        // Module déjà dans la structure → Algorithme 2 (authentification mutuelle)
        console << "[" << t << "] Module " << module->blockId 
                << ": Reception STRUCTURE_READY\n";
        console << "    Module deja authentifie\n";
        
        // Algo2 : ne lancer qu'une seule authentification à la fois pour éviter les conflits
        if (!authentificationsEnCours.empty()) return;
        
        console << "    Lancement ALGORITHME 2 (preuve d'appartenance)\n";
        algorithme2_Initier(src);   // Module dans structure → Algo2
    }
}

void MonApplication_MultiKeyCode::surReceptionPreuveMembership(shared_ptr<Message> msg,
                                                       P2PNetworkInterface* src) {
    auto* m = static_cast<MessagePreuveMembership*>(msg.get());
    algorithme2_Verifier(src, m->liens, m->n1);
}

void MonApplication_MultiKeyCode::surReceptionRelaiPreuve(shared_ptr<Message> msg,
                                                  P2PNetworkInterface* src) {
    auto* m = static_cast<MessageRelaiPreuve*>(msg.get());
    if (m->liens > 0) {
        algorithme2_Relayer(src, m->liens, m->x, m->sourceId);
    } else {
        algorithme2_Completer(src, m->x);
    }
}

// =============================================================================
// TRAITEMENT DES ÉVÉNEMENTS
// =============================================================================

void MonApplication_MultiKeyCode::processLocalEvent(EventPtr pev) {
    // Appel obligatoire à la classe parente → gère EVENT_ROTATION3D_END → onMotionEnd()
    Catoms3DBlockCode::processLocalEvent(pev);

    switch (pev->eventType) {
        case EVENT_NI_RECEIVE: {
            auto msg = static_pointer_cast<NetworkInterfaceReceiveEvent>(pev)->message;
            P2PNetworkInterface* src = msg->destinationInterface;
            switch (msg->type) {
                case MSG_AUTH_REQUEST:     surReceptionDemandeAuth(msg, src);      break;
                case MSG_KEY_CHALLENGE:    surReceptionDefiCle(msg, src);          break;
                case MSG_STRUCTURE_READY:  surReceptionStructurePrete(msg, src);   break;
                case MSG_PROOF_MEMBERSHIP: surReceptionPreuveMembership(msg, src); break;
                case MSG_PROOF_RELAY:      surReceptionRelaiPreuve(msg, src);      break;
            }
            break;
        }
        case EVENT_ADD_NEIGHBOR:
            // Ne pas relancer l'authentification pendant la reconfiguration
            // (les mouvements intermédiaires génèrent des EVENT_ADD_NEIGHBOR parasites)
            if (!reconfigStarted)
                verifierNouveauxVoisins();
            break;
        case EVENT_INTERRUPTION: {
            auto id = static_pointer_cast<InterruptionEvent<unsigned int>>(pev)->data;
            if (id == 0 && infoSecurite.estModuleInitial) {
                // Diffusion initiale de STRUCTURE_READY
                notifierVoisinsStructurePrete();
            } else if (id == 1 && infoSecurite.estModuleInitial) {
                // Affichage des statistiques d'authentification (sur demande)
                afficherStats();
            }
            break;
        }
        default: break;
    }
}

// =============================================================================
// RECONFIGURATION — Mouvement greedy vers la cible
// =============================================================================

bool MonApplication_MultiKeyCode::tryMoveToward(const Cell3DPosition& goal) {
    if (module->position == goal) return false;
    if (moveSteps >= MAX_STEPS) {
        cout << "[WARN] Module " << module->blockId
             << " MAX_STEPS atteint (" << MAX_STEPS << ")\n";
        return false;
    }

    auto tab = Catoms3DMotionEngine::getAllRotationsForModule(module);
    Cell3DPosition fp; short fo;

    struct Candidate { Cell3DPosition pos; double dist; bool vis; };
    vector<Candidate> cands;

    for (auto& elem : tab) {
        elem.second.init(((Catoms3DGlBlock*)module->ptrGlBlock)->mat);
        elem.second.getFinalPositionAndOrientation(fp, fo);
        if (!lattice->isFree(fp)) continue;
        // Vérifier connectivité : au moins 1 voisin autre que la position actuelle
        int nbVoisins = 0;
        for (auto& np : lattice->getNeighborhood(fp))
            if (lattice->cellHasBlock(np) && np != module->position) nbVoisins++;
        if (nbVoisins < 1) continue;
        double d = (double)((fp[0]-goal[0])*(fp[0]-goal[0])
                          + (fp[1]-goal[1])*(fp[1]-goal[1])
                          + (fp[2]-goal[2])*(fp[2]-goal[2]));
        cands.push_back({fp, d, visited.count(fp) > 0});
    }
    if (cands.empty()) return false;

    // Priorité aux positions non visitées, puis la plus proche
    Cell3DPosition best; double bestDist = 1e9; bool found = false;
    for (auto& c : cands)
        if (!c.vis && c.dist < bestDist) { bestDist = c.dist; best = c.pos; found = true; }

    if (!found) {
        // Toutes visitées → reset et réessayer
        visited.clear();
        visited.insert(module->position);
        bestDist = 1e9;
        for (auto& c : cands)
            if (c.dist < bestDist) { bestDist = c.dist; best = c.pos; found = true; }
    }

    if (found) {
        visited.insert(best);
        moveSteps++;
        bool moved = ((Catoms3DBlock*)module)->moveTo(best);
        if (!moved) {
            cout << "[WARN] moveTo refusé pour module " << module->blockId << "\n";
            moveSteps--;
        }
        return moved;
    }
    return false;
}

void MonApplication_MultiKeyCode::onMotionEnd() {
    if (module->position == myTarget) {
        cout << "[Reconf] Module " << module->blockId << " arrive a ("
             << myTarget[0] << "," << myTarget[1] << "," << myTarget[2] << ")\n";
        // Garder la couleur verte (module authentifié) au lieu de passer en jaune
        // module->setColor(YELLOW);

        // Purger les interfaces déconnectées (le module a bougé, ses voisins ont changé)
        for (auto it = voisinsAuthentifies.begin(); it != voisinsAuthentifies.end(); ) {
            if (!it->first->connectedInterface) it = voisinsAuthentifies.erase(it);
            else ++it;
        }
        for (auto it = voisinsDansStructure.begin(); it != voisinsDansStructure.end(); ) {
            if (!it->first->connectedInterface) it = voisinsDansStructure.erase(it);
            else ++it;
        }
        for (auto it = clesVoisins.begin(); it != clesVoisins.end(); ) {
            if (!it->first->connectedInterface) it = clesVoisins.erase(it);
            else ++it;
        }
        authentificationsEnCours.clear();
        n0EnAttente.clear();
        n0Algo2EnAttente.clear();

        // Authentifier les nouveaux voisins (Algo2 — module déjà dans la structure)
        verifierNouveauxVoisins();

        if (!moveQueue.empty()) moveQueue.pop();
        if (!moveQueue.empty()) {
            bID nextId = moveQueue.front();
            auto nextBlock = BaseSimulator::getWorld()->getBlockById(nextId);
            if (nextBlock) {
                MonApplication_MultiKeyCode* nc = (MonApplication_MultiKeyCode*)nextBlock->blockCode;
                cout << "[Reconf] Lancement module " << nextId << " : ("
                     << nc->module->position[0] << "," << nc->module->position[1] << ","
                     << nc->module->position[2] << ") → ("
                     << nc->myTarget[0] << "," << nc->myTarget[1] << ","
                     << nc->myTarget[2] << ")\n";
                nc->visited.clear();
                nc->visited.insert(nc->module->position);
                nc->moveSteps = 0;
                nc->tryMoveToward(nc->myTarget);
            }
        } else {
            cout << "\n=== RECONFIGURATION TERMINEE ===\n";
        }
    } else {
        if (!tryMoveToward(myTarget)) {
            cout << "[WARN] Module " << module->blockId << " bloque — reset\n";
            visited.clear();
            visited.insert(module->position);
            moveSteps = 0;
            tryMoveToward(myTarget);
        }
    }
}

void MonApplication_MultiKeyCode::parseUserBlockElements(TiXmlElement* /*config*/) {}

// =============================================================================
// CONSTRUCTEURS DES MESSAGES
// =============================================================================

MessageDemandeAuth::MessageDemandeAuth(int _liens,
                                       const vector<uint8_t>& _n1,
                                       const vector<uint8_t>& _x,
                                       const vector<uint8_t>& _K0)
    : Message(), liens(_liens), n1(_n1), x(_x), K0(_K0) { type = MSG_AUTH_REQUEST; }

MessageDefiCle::MessageDefiCle(const vector<uint8_t>& _x1)
    : Message(), x1(_x1) { type = MSG_KEY_CHALLENGE; }

MessageStructurePrete::MessageStructurePrete()
    : Message() { type = MSG_STRUCTURE_READY; }

MessagePreuveMembership::MessagePreuveMembership(int _liens, const vector<uint8_t>& _n1)
    : Message(), liens(_liens), n1(_n1) { type = MSG_PROOF_MEMBERSHIP; }

MessageRelaiPreuve::MessageRelaiPreuve(int _liens, const vector<uint8_t>& _x, bID _sourceId)
    : Message(), liens(_liens), x(_x), sourceId(_sourceId) { type = MSG_PROOF_RELAY; }
