#include <iostream>
#include <random>
#include <cmath>
#include <queue>
#include <algorithm>
#include "akcpmBlockCode_four_keys.h"

using namespace std;
using namespace Catoms3D;

// ═══════════════════════════════════════════════════════════════════════════
// VARIABLES STATIQUES
// ═══════════════════════════════════════════════════════════════════════════

uint64_t AkcpmBlockCode::n2Global = 0;
bool AkcpmBlockCode::n2GlobalDefini = false;

// Compteur global pour l'attribution des niveaux de clé (partagé entre tous les modules)
static int compteurGlobalNiveauCle = 1;

const int AkcpmBlockCode::NB_LIGNES_CODE;
const int AkcpmBlockCode::TAILLE_LIGNE_BITS;
const int AkcpmBlockCode::DIVISEUR_SYNC;
const int AkcpmBlockCode::TOLERANCE_SYNC;
const int AkcpmBlockCode::DELAI_TRANSMISSION;

// === VARIABLES STATIQUES RECONFIGURATION ===
static vector<Cell3DPosition> initialShape = {
    Cell3DPosition(1,3,2),  // M1
    Cell3DPosition(2,3,2),  // M2
    Cell3DPosition(1,2,2),  // M3
    Cell3DPosition(2,2,2),  // M4
    Cell3DPosition(3,2,2),  // M5
};

static vector<Cell3DPosition> targetShape = {
    Cell3DPosition(2,3,2),  // M2 reste
    Cell3DPosition(2,2,2),  // M4 reste
    Cell3DPosition(3,2,2),  // M5 reste
    Cell3DPosition(3,3,2),  // M1 va ici
    Cell3DPosition(2,2,3),  // M3 monte ici
};

static queue<bID> moveQueue;
static bool planReady = false;
static map<bID, Cell3DPosition> assignments;

static int authenticatedCount = 0;
static set<bID> authenticatedModules;
static const int TOTAL_MODULES = 5;
static bool reconfigStarted = false;

static bool isInShape(const Cell3DPosition &pos, const vector<Cell3DPosition> &shape) {
    return find(shape.begin(), shape.end(), pos) != shape.end();
}

// ═══════════════════════════════════════════════════════════════════════════
// CONSTRUCTEUR / DESTRUCTEUR
// ═══════════════════════════════════════════════════════════════════════════

AkcpmBlockCode::AkcpmBlockCode(Catoms3DBlock *host) 
    : Catoms3DBlockCode(host), compteurNiveauCle(1), moveSteps(0), isMoving(false) {
    // Le compteur commence à 1 (K1)
}

AkcpmBlockCode::~AkcpmBlockCode() {
    clesVoisins.clear();
    niveauxClesVoisins.clear();
    voisinsAuthentifies.clear();
    voisinsDansStructure.clear();
    authentificationsEnCours.clear();
    n0EnAttente.clear();
}

// ═══════════════════════════════════════════════════════════════════════════
// FONCTIONS CRYPTOGRAPHIQUES
// ═══════════════════════════════════════════════════════════════════════════

uint64_t AkcpmBlockCode::H(uint64_t valeur) {
    uint64_t hash = 5381;
    for (int i = 0; i < 8; i++) {
        hash = ((hash << 5) + hash) + ((valeur >> (i * 8)) & 0xFF);
    }
    return hash;
}

uint64_t AkcpmBlockCode::H(const std::vector<uint8_t>& donnees) {
    uint64_t hash = 5381;
    for (auto octet : donnees) {
        hash = ((hash << 5) + hash) + octet;
    }
    return hash;
}

std::vector<uint8_t> AkcpmBlockCode::L(int numeroLigne) {
    std::vector<uint8_t> ligne(TAILLE_LIGNE_BITS / 8);
    uint64_t graine = 0xDEADBEEF ^ (numeroLigne * 0x12345678);
    for (size_t i = 0; i < ligne.size(); i++) {
        graine = graine * 1103515245 + 12345;
        ligne[i] = (graine >> 16) & 0xFF;
    }
    return ligne;
}

std::vector<uint8_t> AkcpmBlockCode::HL(uint64_t n) {
    int numeroLigne = n % NB_LIGNES_CODE;
    std::vector<uint8_t> ligne = L(numeroLigne);
    uint64_t hash = H(ligne);
    
    std::vector<uint8_t> resultat(8);
    for (int i = 0; i < 8; i++) {
        resultat[i] = (hash >> (i * 8)) & 0xFF;
    }
    return resultat;
}

uint64_t AkcpmBlockCode::XOR(uint64_t a, uint64_t b) {
    return a ^ b;
}

uint64_t AkcpmBlockCode::genererNonce() {
    static std::random_device rd;
    static std::mt19937_64 gen(rd());
    std::uniform_int_distribution<uint64_t> dis;
    return dis(gen);
}

/**
 * Applique la fonction de hachage H() n fois
 * 
 * Utilisé dans FOUR_KEYS pour:
 *   - Encoder le niveau de clé dans x1
 *   - Vérifier le niveau côté récepteur
 * 
 * @param valeur Valeur initiale
 * @param n Nombre de fois à appliquer H()
 * @return H(H(H(...H(valeur)...))) appliqué n fois
 */
uint64_t AkcpmBlockCode::appliquerHashNFois(uint64_t valeur, int n) {
    uint64_t resultat = valeur;
    for (int i = 0; i < n; i++) {
        resultat = H(resultat);
    }
    return resultat;
}

/**
 * XOR sur 128 bits (pour le format FOUR_KEYS)
 * 
 * x1 = (H^(k-1)(n2) || n2) ⊕ (n0 || n0)
 * Résultat = (aHaut ⊕ bHaut || aBas ⊕ bBas)
 */
void AkcpmBlockCode::xor128(uint64_t aHaut, uint64_t aBas,
                            uint64_t bHaut, uint64_t bBas,
                            uint64_t& resultatHaut, uint64_t& resultatBas) {
    resultatHaut = aHaut ^ bHaut;
    resultatBas = aBas ^ bBas;
}

// ═══════════════════════════════════════════════════════════════════════════
// SYNCHRONISATION TEMPORELLE
// ═══════════════════════════════════════════════════════════════════════════

uint64_t AkcpmBlockCode::obtenirTs() {
    uint64_t ts = getScheduler()->now();
    return ts / DIVISEUR_SYNC;
}

uint64_t AkcpmBlockCode::calculerTs(uint64_t Tr, int deltaT) {
    return (Tr - deltaT) / DIVISEUR_SYNC;
}

// ═══════════════════════════════════════════════════════════════════════════
// FONCTIONS UTILITAIRES
// ═══════════════════════════════════════════════════════════════════════════

int AkcpmBlockCode::compterVoisinsConnectes() {
    Catoms3DBlock* catom = (Catoms3DBlock*)hostBlock;
    int compte = 0;
    for (int i = 0; i < 12; i++) {
        P2PNetworkInterface* iface = catom->getInterface(i);
        if (iface && iface->connectedInterface) {
            compte++;
        }
    }
    return compte;
}

bool AkcpmBlockCode::estDansStructure() const {
    return infoSecurite.etat == ETAT_AUTHENTIFIE ||
           infoSecurite.etat == ETAT_CLE_ETABLIE ||
           infoSecurite.estModuleInitial;
}

void AkcpmBlockCode::definirCommeModuleInitial() {
    infoSecurite.estModuleInitial = true;
    infoSecurite.etat = ETAT_AUTHENTIFIE;
    infoSecurite.liensStructure = 0;
    compteurNiveauCle = 1;  // Commence avec K1
    
    Catoms3DBlock* catom = (Catoms3DBlock*)hostBlock;
    catom->setColor(BLUE);
    
    console << "═══════════════════════════════════════════════════════════\n";
    console << "★ MODULE " << hostBlock->blockId << " DÉFINI COMME INITIAL (iM) ★\n";
    console << "  Modèle: FOUR_KEYS (4 clés: K1, K2, K3, K4)\n";
    console << "  Prochain niveau de clé: K" << compteurNiveauCle << "\n";
    console << "═══════════════════════════════════════════════════════════\n";
}

// ═══════════════════════════════════════════════════════════════════════════
// DÉMARRAGE
// ═══════════════════════════════════════════════════════════════════════════

void AkcpmBlockCode::startup() {
    console << "═══════════════════════════════════════════════════════════\n";
    console << "Module " << hostBlock->blockId << " - Protocole AKC-PM (FOUR_KEYS)\n";
    console << "═══════════════════════════════════════════════════════════\n";
    
    Catoms3DBlock* catom = (Catoms3DBlock*)hostBlock;
    Cell3DPosition pos = catom->position;
    
    console << "  Position: (" << pos[0] << "," << pos[1] << "," << pos[2] << ")\n";
    console << "  Voisins connectés: " << compterVoisinsConnectes() << "\n";
    
    // Planification (Module 1)
    if (!planReady && hostBlock->blockId == 1) {
        // Réinitialiser le compteur global au début de la simulation
        compteurGlobalNiveauCle = 1;
        
        vector<Cell3DPosition> toRemove, toFill;
        
        for (auto &p : initialShape)
            if (!isInShape(p, targetShape)) toRemove.push_back(p);
        for (auto &p : targetShape)
            if (!isInShape(p, initialShape)) toFill.push_back(p);

        auto world = BaseSimulator::getWorld();
        for (size_t i = 0; i < toRemove.size() && i < toFill.size(); i++) {
            for (auto &pair : world->buildingBlocksMap) {
                if (pair.second->position == toRemove[i]) {
                    assignments[pair.first] = toFill[i];
                    moveQueue.push(pair.first);
                    break;
                }
            }
        }
        planReady = true;
    }
    
    // Module initial
    if (pos == Cell3DPosition(2,2,2)) {
        definirCommeModuleInitial();
        getScheduler()->schedule(
            new InterruptionEvent<unsigned int>(getScheduler()->now() + 1000, hostBlock, 0));
        
        if (authenticatedModules.find(hostBlock->blockId) == authenticatedModules.end()) {
            authenticatedModules.insert(hostBlock->blockId);
            authenticatedCount++;
        }
    } else {
        catom->setColor(GREY);
        console << "  En attente du signal STRUCTURE_READY...\n";
    }
    
    // Configuration modules mobiles
    if (planReady && assignments.count(hostBlock->blockId)) {
        myTarget = assignments[hostBlock->blockId];
    }
}

// ═══════════════════════════════════════════════════════════════════════════
// ALGORITHME 1 - FOUR_KEYS
// ═══════════════════════════════════════════════════════════════════════════

/**
 * SF-1, SF-2: Initiation (identique aux autres modèles)
 */
void AkcpmBlockCode::algorithme1_Initier(P2PNetworkInterface* dest) {
    console << "───────────────────────────────────────────────────────────\n";
    console << "Module " << hostBlock->blockId << ": ALGORITHME 1 - INITIATION (SF-1, SF-2)\n";
    console << "───────────────────────────────────────────────────────────\n";
    
    // SF-1: Génération des valeurs
    infoSecurite.n0 = genererNonce();
    infoSecurite.n1 = H(infoSecurite.n0);
    infoSecurite.K0 = HL(infoSecurite.n0);
    
    uint64_t ts_brut = getScheduler()->now();
    uint64_t Ts = ts_brut / DIVISEUR_SYNC;
    uint64_t x = XOR(Ts, infoSecurite.n0);
    
    console << "  n0 = " << infoSecurite.n0 << " (secret)\n";
    console << "  n1 = H(n0) = " << infoSecurite.n1 << "\n";
    console << "  Ts = " << Ts << "\n";
    console << "  x = Ts ⊕ n0 = " << x << "\n";
    
    // SF-2: Envoi
    int liens = (infoSecurite.liensStructure > 0) ? infoSecurite.liensStructure : 1;
    
    MessageDemandeAuth* msg = new MessageDemandeAuth(
        liens, infoSecurite.n1, x, infoSecurite.K0);
    sendMessage(msg, dest, DELAI_TRANSMISSION, 0);
    
    n0EnAttente[dest] = infoSecurite.n0;
    authentificationsEnCours.insert(dest);
    infoSecurite.etat = ETAT_AUTHENTIFICATION;
    
    console << "  → Message AUTH_REQUEST envoyé\n";
}

/**
 * SF-3, SF-4: Vérification et génération de défi (FOUR_KEYS)
 * 
 * Spécificité FOUR_KEYS:
 *   - Attribue un niveau de clé (1 à 4) au nouveau module
 *   - Encode ce niveau dans x1 sur 128 bits
 *   - x1 = (H^(k-1)(n2) || n2) ⊕ (n0 || n0)
 */
void AkcpmBlockCode::algorithme1_Verifier(P2PNetworkInterface* src, int liens, 
                                           uint64_t n1, uint64_t x, 
                                           const std::vector<uint8_t>& K0) {
    console << "───────────────────────────────────────────────────────────\n";
    console << "Module " << hostBlock->blockId << ": ALGORITHME 1 - VÉRIFICATION (SF-3, SF-4)\n";
    console << "───────────────────────────────────────────────────────────\n";
    
    if (!estDansStructure()) {
        console << "  ✗ REJETÉ: Pas dans la structure\n";
        return;
    }
    
    // SF-3: Vérification sync et authentification
    uint64_t Tr = getScheduler()->now();
    uint64_t n0_prime = 0;
    bool syncOK = false;
    
    uint64_t Ts_base = (Tr - DELAI_TRANSMISSION) / DIVISEUR_SYNC;
    
    console << "  Tr = " << Tr << ", Δt = " << DELAI_TRANSMISSION << "\n";
    console << "  Ts_base = " << Ts_base << "\n";
    
    for (int offset = -TOLERANCE_SYNC; offset <= TOLERANCE_SYNC && !syncOK; offset++) {
        uint64_t Ts_test = Ts_base + offset;
        uint64_t n0_test = XOR(x, Ts_test);
        uint64_t n1_calcule = H(n0_test);
        
        if (n1_calcule == n1) {
            n0_prime = n0_test;
            syncOK = true;
            console << "  ✓ Sync OK (offset=" << offset << ")\n";
        }
    }
    
    if (!syncOK) {
        console << "  ✗ ÉCHEC SYNC\n";
        return;
    }
    
    std::vector<uint8_t> K0_calcule = HL(n0_prime);
    if (K0_calcule != K0) {
        console << "  ✗ ÉCHEC AUTH: K0 invalide\n";
        return;
    }
    
    console << "  ✓ K0 vérifié - Module authentifié!\n";
    
    // ═══════════════════════════════════════════════════════════════════════
    // SF-4: FOUR_KEYS - Génération du défi avec niveau de clé
    // ═══════════════════════════════════════════════════════════════════════
    
    // Générer n2 si pas encore fait
    if (infoSecurite.estModuleInitial && !n2GlobalDefini) {
        n2Global = genererNonce();
        n2GlobalDefini = true;
    }
    uint64_t n2 = n2Global;
    
    // Déterminer le niveau de clé (cycle 1,2,3,4,1,2,...)
    int niveauCle = compteurGlobalNiveauCle;
    compteurGlobalNiveauCle = (compteurGlobalNiveauCle % 4) + 1;  // Cycle: 1→2→3→4→1...
    
    console << "  [FOUR_KEYS] Niveau de clé attribué: K" << niveauCle << "\n";
    
    // Calculer H^(k-1)(n2) pour x1 - hash appliqué (k-1) fois sur n2
    uint64_t h_n2 = n2;
    for (int i = 0; i < niveauCle - 1; i++) {
        h_n2 = H(h_n2);
    }
    
    console << "  n2 = " << n2 << "\n";
    console << "  H^" << (niveauCle - 1) << "(n2) = " << h_n2 << "\n";
    
    // Calculer x1 = (H^(k-1)(n2) || n2) ⊕ (n0' || n0')
    uint64_t x1Haut, x1Bas;
    xor128(h_n2, n2, n0_prime, n0_prime, x1Haut, x1Bas);
    
    console << "  x1 = (H^(k-1)(n2)||n2) ⊕ (n0'||n0')\n";
    console << "  x1_haut = " << x1Haut << ", x1_bas = " << x1Bas << "\n";
    
    // Calculer la clé correspondante: K1, K2, K3 ou K4
    // K1 = H(L(n2 mod Nb)) = H(n2) simplifié
    // K2 = H(H(K1)), K3 = H(H(K2)), K4 = H(H(K3))
    std::vector<uint8_t> K1 = HL(n2);
    uint64_t K1_uint64 = H(n2);
    uint64_t Kx = K1_uint64;
    
    // Appliquer H(H()) (k-1) fois pour obtenir Kk
    for (int i = 1; i < niveauCle; i++) {
        Kx = H(H(Kx));
    }
    
    // Convertir en vecteur
    std::vector<uint8_t> cleAStocquer(8);
    for (int i = 0; i < 8; i++) {
        cleAStocquer[i] = (Kx >> (i * 8)) & 0xFF;
    }
    
    // Enregistrer
    clesVoisins[src] = cleAStocquer;
    niveauxClesVoisins[src] = niveauCle;
    voisinsAuthentifies[src] = true;
    voisinsDansStructure[src] = true;
    infoSecurite.liensStructure++;
    
    // Envoyer le défi avec le niveau de clé
    MessageDefiCle* msg = new MessageDefiCle(x1Haut, x1Bas, niveauCle);
    sendMessage(msg, src, DELAI_TRANSMISSION, 0);
    
    console << "  → Message KEY_CHALLENGE envoyé (niveau K" << niveauCle << ")\n";
    console << "  Prochain niveau global: K" << compteurGlobalNiveauCle << "\n";
}

/**
 * SF-5: Complétion de l'authentification (FOUR_KEYS)
 * 
 * Le nouveau module:
 *   1. Décode x1 pour obtenir n2 et H^(k-1)(n2)
 *   2. Vérifie que H^(k-1)(n2) est correct
 *   3. Génère la clé Kx selon le niveau
 */
void AkcpmBlockCode::algorithme1_Completer(P2PNetworkInterface* src, 
                                            uint64_t x1Haut, uint64_t x1Bas, 
                                            int niveauCle) {
    console << "───────────────────────────────────────────────────────────\n";
    console << "Module " << hostBlock->blockId << ": ALGORITHME 1 - COMPLÉTION (SF-5)\n";
    console << "───────────────────────────────────────────────────────────\n";
    
    if (n0EnAttente.find(src) == n0EnAttente.end()) {
        console << "  ✗ ERREUR: Pas d'authentification en cours\n";
        return;
    }
    
    uint64_t n0 = n0EnAttente[src];
    
    // Décoder x1: (H^(k-1)(n2) || n2) = x1 ⊕ (n0 || n0)
    uint64_t decode_haut, decode_bas;
    xor128(x1Haut, x1Bas, n0, n0, decode_haut, decode_bas);
    
    uint64_t n2 = decode_bas;           // n2 est dans la partie basse
    uint64_t h_n2_recu = decode_haut;   // H^(k-1)(n2) est dans la partie haute
    
    console << "  [FOUR_KEYS] Niveau de clé reçu: K" << niveauCle << "\n";
    console << "  n2 décodé = " << n2 << "\n";
    console << "  H^(k-1)(n2) reçu = " << h_n2_recu << "\n";
    
    // Vérifier H^(k-1)(n2) en appliquant H() (k-1) fois sur n2
    uint64_t h_n2_calcule = n2;
    for (int i = 0; i < niveauCle - 1; i++) {
        h_n2_calcule = H(h_n2_calcule);
    }
    
    if (h_n2_calcule != h_n2_recu) {
        console << "  ✗ ERREUR: Vérification H^(k-1)(n2) échouée!\n";
        console << "    Calculé: " << h_n2_calcule << "\n";
        console << "    Reçu: " << h_n2_recu << "\n";
        return;
    }
    console << "  ✓ H^(k-1)(n2) vérifié\n";
    
    // Générer la clé selon le niveau
    // K1 = H(L(n2 mod Nb)) = H(n2) simplifié
    // K2 = H(H(K1)), K3 = H(H(K2)), K4 = H(H(K3))
    uint64_t K1_uint64 = H(n2);
    uint64_t Kx = K1_uint64;
    
    // Appliquer H(H()) (k-1) fois
    for (int i = 1; i < niveauCle; i++) {
        Kx = H(H(Kx));
    }
    
    std::vector<uint8_t> clePartagee(8);
    for (int i = 0; i < 8; i++) {
        clePartagee[i] = (Kx >> (i * 8)) & 0xFF;
    }
    
    console << "  K" << niveauCle << " générée (H appliqué " 
            << ((niveauCle > 1) ? (niveauCle - 1) * 2 : 0) << " fois après K1)\n";
    
    // Enregistrer
    infoSecurite.n2 = n2;
    infoSecurite.clePartagee = clePartagee;
    infoSecurite.niveauCle = niveauCle;
    clesVoisins[src] = clePartagee;
    niveauxClesVoisins[src] = niveauCle;
    voisinsAuthentifies[src] = true;
    voisinsDansStructure[src] = true;
    infoSecurite.etat = ETAT_CLE_ETABLIE;
    infoSecurite.liensStructure++;
    
    n0EnAttente.erase(src);
    authentificationsEnCours.erase(src);
    
    console << "╔═══════════════════════════════════════════════════════════╗\n";
    console << "║  ★★★ AUTHENTIFICATION RÉUSSIE ★★★                        ║\n";
    console << "║  Module intégré à la structure!                          ║\n";
    console << "║  Clé K" << niveauCle << " établie (modèle FOUR_KEYS)                    ║\n";
    console << "╚═══════════════════════════════════════════════════════════╝\n";
    
    // Couleur selon le niveau de clé
    Catoms3DBlock* catom = (Catoms3DBlock*)hostBlock;
    switch (niveauCle) {
        case 1: catom->setColor(GREEN);  break;   // K1 = Vert
        case 2: catom->setColor(YELLOW); break;   // K2 = Jaune
        case 3: catom->setColor(ORANGE); break;   // K3 = Orange
        case 4: catom->setColor(RED);    break;   // K4 = Rouge
    }
    
    if (authenticatedModules.find(hostBlock->blockId) == authenticatedModules.end()) {
        authenticatedModules.insert(hostBlock->blockId);
        authenticatedCount++;
        console << "  Modules authentifiés: " << authenticatedCount << "/" << TOTAL_MODULES << "\n";
    }
    
    notifierVoisinsStructurePrete();
    
    // Lancer reconfiguration si tous authentifiés
    if (authenticatedCount >= TOTAL_MODULES && !reconfigStarted) {
        reconfigStarted = true;
        console << "\n=== DEBUT RECONFIGURATION ===\n";
        
        if (!moveQueue.empty()) {
            bID nextId = moveQueue.front();
            auto world = BaseSimulator::getWorld();
            auto nextBlock = world->getBlockById(nextId);
            if (nextBlock) {
                AkcpmBlockCode *nextCode = (AkcpmBlockCode*)nextBlock->blockCode;
                nextCode->visited.clear();
                nextCode->visited.insert(nextCode->hostBlock->position);
                nextCode->moveSteps = 0;
                nextCode->tryMoveToward(nextCode->myTarget);
            }
        }
    }
}

// ═══════════════════════════════════════════════════════════════════════════
// GESTION DE LA STRUCTURE
// ═══════════════════════════════════════════════════════════════════════════

void AkcpmBlockCode::notifierVoisinsStructurePrete() {
    Catoms3DBlock* catom = (Catoms3DBlock*)hostBlock;
    
    console << "Module " << hostBlock->blockId << ": Diffusion STRUCTURE_READY\n";
    
    for (int i = 0; i < 12; i++) {
        P2PNetworkInterface* iface = catom->getInterface(i);
        if (iface && iface->connectedInterface) {
            if (voisinsAuthentifies.find(iface) == voisinsAuthentifies.end()) {
                MessageStructurePrete* msg = new MessageStructurePrete();
                sendMessage(msg, iface, DELAI_TRANSMISSION, 0);
            }
        }
    }
}

void AkcpmBlockCode::verifierNouveauxVoisins() {
    Catoms3DBlock* catom = (Catoms3DBlock*)hostBlock;
    
    for (int i = 0; i < 12; i++) {
        P2PNetworkInterface* iface = catom->getInterface(i);
        if (iface && iface->connectedInterface) {
            if (voisinsDansStructure.find(iface) == voisinsDansStructure.end() &&
                voisinsAuthentifies.find(iface) == voisinsAuthentifies.end() &&
                authentificationsEnCours.find(iface) == authentificationsEnCours.end()) {
                
                if (estDansStructure()) {
                    MessageStructurePrete* msg = new MessageStructurePrete();
                    sendMessage(msg, iface, DELAI_TRANSMISSION, 0);
                }
            }
        }
    }
}

// ═══════════════════════════════════════════════════════════════════════════
// GESTIONNAIRES DE MESSAGES
// ═══════════════════════════════════════════════════════════════════════════

void AkcpmBlockCode::surReceptionDemandeAuth(std::shared_ptr<Message> msg, 
                                              P2PNetworkInterface* src) {
    MessageDemandeAuth* authMsg = static_cast<MessageDemandeAuth*>(msg.get());
    algorithme1_Verifier(src, authMsg->liens, authMsg->n1, authMsg->x, authMsg->K0);
}

void AkcpmBlockCode::surReceptionDefiCle(std::shared_ptr<Message> msg,
                                          P2PNetworkInterface* src) {
    MessageDefiCle* cleMsg = static_cast<MessageDefiCle*>(msg.get());
    console << "Module " << hostBlock->blockId << ": Réception KEY_CHALLENGE (niveau K" 
            << cleMsg->niveauCle << ")\n";
    algorithme1_Completer(src, cleMsg->x1Haut, cleMsg->x1Bas, cleMsg->niveauCle);
}

void AkcpmBlockCode::surReceptionStructurePrete(std::shared_ptr<Message> msg,
                                                 P2PNetworkInterface* src) {
    voisinsDansStructure[src] = true;
    
    if (!estDansStructure()) {
        if (authentificationsEnCours.find(src) == authentificationsEnCours.end() &&
            voisinsAuthentifies.find(src) == voisinsAuthentifies.end()) {
            algorithme1_Initier(src);
        }
    }
}

// ═══════════════════════════════════════════════════════════════════════════
// TRAITEMENT DES ÉVÉNEMENTS
// ═══════════════════════════════════════════════════════════════════════════

void AkcpmBlockCode::processLocalEvent(EventPtr pev) {
    MessagePtr message;
    
    switch (pev->eventType) {
        case EVENT_NI_RECEIVE: {
            message = (std::static_pointer_cast<NetworkInterfaceReceiveEvent>(pev))->message;
            P2PNetworkInterface* src = message->destinationInterface;
            
            switch (message->type) {
                case MSG_AUTH_REQUEST:
                    surReceptionDemandeAuth(message, src);
                    break;
                case MSG_KEY_CHALLENGE:
                    surReceptionDefiCle(message, src);
                    break;
                case MSG_STRUCTURE_READY:
                    surReceptionStructurePrete(message, src);
                    break;
            }
            break;
        }
        
        case EVENT_ADD_NEIGHBOR:
            verifierNouveauxVoisins();
            break;
            
        case EVENT_INTERRUPTION: {
            unsigned int interruptId = (std::static_pointer_cast<InterruptionEvent<unsigned int>>(pev))->data;
            
            if (interruptId == 0) {
                if (infoSecurite.estModuleInitial) {
                    notifierVoisinsStructurePrete();
                }
            } else if (interruptId == 2) {
                // Vérification de fin de mouvement
                if (isMoving) {
                    Catoms3DBlock* catom = (Catoms3DBlock*)hostBlock;
                    if (catom->position != lastPosition) {
                        isMoving = false;
                        
                        if (catom->position == myTarget) {
                            if (!moveQueue.empty()) moveQueue.pop();
                            
                            if (!moveQueue.empty()) {
                                bID nextId = moveQueue.front();
                                auto nextBlock = BaseSimulator::getWorld()->getBlockById(nextId);
                                if (nextBlock) {
                                    AkcpmBlockCode *nextCode = (AkcpmBlockCode*)nextBlock->blockCode;
                                    nextCode->visited.clear();
                                    nextCode->visited.insert(nextCode->hostBlock->position);
                                    nextCode->moveSteps = 0;
                                    nextCode->tryMoveToward(nextCode->myTarget);
                                }
                            }
                        } else {
                            tryMoveToward(myTarget);
                        }
                    } else {
                        getScheduler()->schedule(
                            new InterruptionEvent<unsigned int>(getScheduler()->now() + 10000, hostBlock, 2));
                    }
                }
            }
            break;
        }
            
        default:
            break;
    }
}

void AkcpmBlockCode::onMotionEnd() {}

void AkcpmBlockCode::onTap(int face) {
    Catoms3DBlock* catom = (Catoms3DBlock*)hostBlock;
    Cell3DPosition pos = catom->position;
    
    console << "╔═══════════════════════════════════════════════════════════╗\n";
    console << "║ ÉTAT DU MODULE " << hostBlock->blockId << "\n";
    console << "╠═══════════════════════════════════════════════════════════╣\n";
    console << "║ Position: (" << pos[0] << ", " << pos[1] << ", " << pos[2] << ")\n";
    console << "║ État: ";
    switch (infoSecurite.etat) {
        case ETAT_NON_LIE: console << "NON_LIÉ\n"; break;
        case ETAT_AUTHENTIFICATION: console << "AUTHENTIFICATION\n"; break;
        case ETAT_AUTHENTIFIE: console << "AUTHENTIFIÉ\n"; break;
        case ETAT_CLE_ETABLIE: console << "CLÉ_ÉTABLIE\n"; break;
    }
    console << "║ Module Initial: " << (infoSecurite.estModuleInitial ? "OUI" : "NON") << "\n";
    console << "║ Dans structure: " << (estDansStructure() ? "OUI" : "NON") << "\n";
    console << "║ Modèle: FOUR_KEYS\n";
    if (infoSecurite.niveauCle > 0) {
        console << "║ Niveau de clé: K" << infoSecurite.niveauCle << "\n";
    }
    console << "║ Prochain niveau à attribuer: K" << compteurNiveauCle << "\n";
    console << "╚═══════════════════════════════════════════════════════════╝\n";
}

// ═══════════════════════════════════════════════════════════════════════════
// CLASSES DE MESSAGES
// ═══════════════════════════════════════════════════════════════════════════

// ═══════════════════════════════════════════════════════════════════════════
// RECONFIGURATION
// ═══════════════════════════════════════════════════════════════════════════

bool AkcpmBlockCode::tryMoveToward(const Cell3DPosition &goal) {
    Catoms3DBlock* catom = (Catoms3DBlock*)hostBlock;
    if (catom->position == goal) return false;

    auto tab = Catoms3DMotionEngine::getAllRotationsForModule(catom);
    Cell3DPosition fp;
    short fo;

    struct Candidate { Cell3DPosition pos; double dist; bool visited; };
    vector<Candidate> candidates;

    for (auto &elem : tab) {
        elem.second.init(((Catoms3DGlBlock *)catom->ptrGlBlock)->mat);
        elem.second.getFinalPositionAndOrientation(fp, fo);

        if (!lattice->isFree(fp)) continue;

        int nbOccupied = 0;
        for (auto &nPos : lattice->getNeighborhood(fp))
            if (lattice->cellHasBlock(nPos) && nPos != catom->position) nbOccupied++;
        if (nbOccupied < 1) continue;

        double dist = (fp[0]-goal[0])*(fp[0]-goal[0])
                    + (fp[1]-goal[1])*(fp[1]-goal[1])
                    + (fp[2]-goal[2])*(fp[2]-goal[2]);

        candidates.push_back({fp, dist, visited.count(fp) > 0});
    }

    if (candidates.empty()) return false;

    Cell3DPosition bestPos;
    double bestDist = 999999;
    bool found = false;

    for (auto &c : candidates) {
        if (!c.visited && c.dist < bestDist) {
            bestDist = c.dist;
            bestPos = c.pos;
            found = true;
        }
    }

    if (!found) {
        visited.clear();
        visited.insert(catom->position);
        for (auto &c : candidates) {
            if (c.dist < bestDist) {
                bestDist = c.dist;
                bestPos = c.pos;
                found = true;
            }
        }
    }

    if (found) {
        visited.insert(bestPos);
        moveSteps++;
        isMoving = true;
        lastPosition = catom->position;
        
        catom->moveTo(bestPos);
        
        getScheduler()->schedule(
            new InterruptionEvent<unsigned int>(getScheduler()->now() + 50000, hostBlock, 2));
        
        return true;
    }
    return false;
}

// ═══════════════════════════════════════════════════════════════════════════
// CLASSES DE MESSAGES
// ═══════════════════════════════════════════════════════════════════════════

MessageDemandeAuth::MessageDemandeAuth(int _liens, uint64_t _n1, uint64_t _x,
                                       const std::vector<uint8_t>& _K0)
    : Message(), liens(_liens), n1(_n1), x(_x), K0(_K0) {
    type = MSG_AUTH_REQUEST;
}
MessageDemandeAuth::~MessageDemandeAuth() {}

MessageDefiCle::MessageDefiCle(uint64_t _x1Haut, uint64_t _x1Bas, int _niveauCle)
    : Message(), x1Haut(_x1Haut), x1Bas(_x1Bas), niveauCle(_niveauCle) {
    type = MSG_KEY_CHALLENGE;
}
MessageDefiCle::~MessageDefiCle() {}

MessageStructurePrete::MessageStructurePrete() : Message() {
    type = MSG_STRUCTURE_READY;
}
MessageStructurePrete::~MessageStructurePrete() {}
