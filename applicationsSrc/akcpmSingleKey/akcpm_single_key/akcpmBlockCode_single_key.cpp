/**
 * ╔═══════════════════════════════════════════════════════════════════════════╗
 * ║              IMPLÉMENTATION DU MODÈLE SINGLE_KEY                          ║
 * ║                                                                           ║
 * ║  Ce fichier implémente le protocole AKC-PM avec le modèle de clé unique   ║
 * ║  où tous les modules partagent la même clé K1.                           ║
 * ╚═══════════════════════════════════════════════════════════════════════════╝
 */

#include <iostream>
#include <random>
#include <cmath>
#include "akcpmBlockCode_single_key.h"

using namespace std;
using namespace Catoms3D;

// ═══════════════════════════════════════════════════════════════════════════
// VARIABLES STATIQUES POUR LE MODÈLE SINGLE_KEY
// ═══════════════════════════════════════════════════════════════════════════

// n2 global partagé par tous les modules
uint64_t AkcpmBlockCode::n2Global = 0;
bool AkcpmBlockCode::n2GlobalDefini = false;

// Variables statiques pour les mouvements
std::vector<ModuleMovement> AkcpmBlockCode::planMouvements;
std::map<bID, std::pair<Cell3DPosition, uint8_t>> AkcpmBlockCode::configurationInitiale;

// Définitions des constantes statiques (nécessaire pour l'éditeur de liens)
const int AkcpmBlockCode::NB_LIGNES_CODE;
const int AkcpmBlockCode::TAILLE_LIGNE_BITS;
const int AkcpmBlockCode::DIVISEUR_SYNC;
const int AkcpmBlockCode::TOLERANCE_SYNC;
const int AkcpmBlockCode::DELAI_TRANSMISSION;

// ═══════════════════════════════════════════════════════════════════════════
// CONSTRUCTEUR / DESTRUCTEUR
// ═══════════════════════════════════════════════════════════════════════════

/**
 * Constructeur du module
 * Initialise l'état de sécurité à NON_LIÉ
 */
AkcpmBlockCode::AkcpmBlockCode(Catoms3DBlock *host) : Catoms3DBlockCode(host) {
    // L'état initial est NON_LIÉ (défini dans InfoSecurite)
    lattice = nullptr;
    isMobile = false;
    myStage = 0;
    myMaxStage = 0;
    reconfigurationActive = false;
}

/**
 * Destructeur
 * Nettoie les maps et sets utilisés
 */
AkcpmBlockCode::~AkcpmBlockCode() {
    clesVoisins.clear();
    voisinsAuthentifies.clear();
    voisinsDansStructure.clear();
    authentificationsEnCours.clear();
    n0EnAttente.clear();
}

// ═══════════════════════════════════════════════════════════════════════════
// FONCTIONS CRYPTOGRAPHIQUES
// ═══════════════════════════════════════════════════════════════════════════

/**
 * Fonction de hachage H() - Algorithme DJB2
 * 
 * Caractéristiques:
 *   - Léger et rapide (adapté aux microcontrôleurs)
 *   - Bonne distribution des valeurs
 *   - Utilisé pour n1 = H(n0), K0, K1
 * 
 * @param valeur Valeur 64 bits à hacher
 * @return Hash 64 bits
 */
uint64_t AkcpmBlockCode::H(uint64_t valeur) {
    uint64_t hash = 5381;  // Valeur initiale DJB2
    for (int i = 0; i < 8; i++) {
        // hash * 33 + octet courant
        hash = ((hash << 5) + hash) + ((valeur >> (i * 8)) & 0xFF);
    }
    return hash;
}

/**
 * Surcharge de H() pour vecteur d'octets
 */
uint64_t AkcpmBlockCode::H(const std::vector<uint8_t>& donnees) {
    uint64_t hash = 5381;
    for (auto octet : donnees) {
        hash = ((hash << 5) + hash) + octet;
    }
    return hash;
}

/**
 * L(i) - Retourne la ligne i du code source
 * 
 * Dans une vraie implémentation, cette fonction retournerait
 * la ligne i du code source compilé. Ici, nous simulons
 * un contenu déterministe pour la démonstration.
 * 
 * @param numeroLigne Numéro de la ligne (0 à NB_LIGNES_CODE-1)
 * @return Vecteur de 32 octets (256 bits)
 */
std::vector<uint8_t> AkcpmBlockCode::L(int numeroLigne) {
    std::vector<uint8_t> ligne(TAILLE_LIGNE_BITS / 8);  // 32 octets
    
    // Génération déterministe basée sur le numéro de ligne
    uint64_t graine = 0xDEADBEEF ^ (numeroLigne * 0x12345678);
    for (size_t i = 0; i < ligne.size(); i++) {
        graine = graine * 1103515245 + 12345;  // Générateur congruentiel linéaire
        ligne[i] = (graine >> 16) & 0xFF;
    }
    
    return ligne;
}

/**
 * HL(n) - Calcule H(L(n mod Nb))
 * 
 * Cette fonction est centrale dans le protocole:
 *   - K0 = H(L(n0 mod Nb)) : empreinte d'authentification
 *   - K1 = H(L(n2 mod Nb)) : clé partagée
 * 
 * @param n Valeur dont on prend le modulo avec NB_LIGNES_CODE
 * @return Vecteur de 8 octets représentant le hash
 */
std::vector<uint8_t> AkcpmBlockCode::HL(uint64_t n) {
    // Sélectionner la ligne correspondante
    int numeroLigne = n % NB_LIGNES_CODE;
    std::vector<uint8_t> ligne = L(numeroLigne);
    
    // Hacher la ligne
    uint64_t hash = H(ligne);
    
    // Convertir en vecteur d'octets
    std::vector<uint8_t> resultat(8);
    for (int i = 0; i < 8; i++) {
        resultat[i] = (hash >> (i * 8)) & 0xFF;
    }
    return resultat;
}

/**
 * Opération XOR (OU exclusif)
 * Utilisée pour protéger les nonces: x = Ts ⊕ n0
 */
uint64_t AkcpmBlockCode::XOR(uint64_t a, uint64_t b) {
    return a ^ b;
}

/**
 * Génère un nonce aléatoire 64 bits
 * Utilise le générateur Mersenne Twister pour une bonne qualité aléatoire
 */
uint64_t AkcpmBlockCode::genererNonce() {
    static std::random_device rd;
    static std::mt19937_64 gen(rd());
    std::uniform_int_distribution<uint64_t> dis;
    return dis(gen);
}

// ═══════════════════════════════════════════════════════════════════════════
// SYNCHRONISATION TEMPORELLE
// ═══════════════════════════════════════════════════════════════════════════

/**
 * Calcule Ts ≈ ts/10
 * 
 * Article Section VII.A:
 * "Le calcul de l'horloge de transmission divisée par 10 et arrondie
 * à l'entier le plus proche permet de corriger les incertitudes de -5 à +5"
 */
uint64_t AkcpmBlockCode::obtenirTs() {
    uint64_t ts = getScheduler()->now();
    return ts / DIVISEUR_SYNC;
}

/**
 * Calcule Ts côté récepteur: Ts ≈ (Tr - Δt) / 10
 * 
 * @param Tr Temps de réception
 * @param deltaT Délai de transmission estimé
 */
uint64_t AkcpmBlockCode::calculerTs(uint64_t Tr, int deltaT) {
    return (Tr - deltaT) / DIVISEUR_SYNC;
}

// ═══════════════════════════════════════════════════════════════════════════
// FONCTIONS UTILITAIRES
// ═══════════════════════════════════════════════════════════════════════════

/**
 * Compte le nombre de voisins connectés (interfaces actives)
 */
int AkcpmBlockCode::compterVoisinsConnectes() {
    Catoms3DBlock* catom = (Catoms3DBlock*)hostBlock;
    int compte = 0;
    for (int i = 0; i < 12; i++) {  // 3D Catoms ont 12 voisins max
        P2PNetworkInterface* iface = catom->getInterface(i);
        if (iface && iface->connectedInterface) {
            compte++;
        }
    }
    return compte;
}

/**
 * Vérifie si le module fait partie de la structure
 */
bool AkcpmBlockCode::estDansStructure() const {
    return infoSecurite.etat == ETAT_AUTHENTIFIE ||
           infoSecurite.etat == ETAT_CLE_ETABLIE ||
           infoSecurite.estModuleInitial;
}

/**
 * Définit ce module comme le module initial (iM)
 * Le module initial lance la formation de la structure
 */
void AkcpmBlockCode::definirCommeModuleInitial() {
    infoSecurite.estModuleInitial = true;
    infoSecurite.etat = ETAT_AUTHENTIFIE;
    infoSecurite.liensStructure = 0;
    
    // Couleur bleue pour le module initial
    Catoms3DBlock* catom = (Catoms3DBlock*)hostBlock;
    catom->setColor(BLUE);
    
    console << "═══════════════════════════════════════════════════════════\n";
    console << "★ MODULE " << hostBlock->blockId << " DÉFINI COMME INITIAL (iM) ★\n";
    console << "  Modèle: SINGLE_KEY (clé unique pour tout le réseau)\n";
    console << "═══════════════════════════════════════════════════════════\n";
}

// ═══════════════════════════════════════════════════════════════════════════
// DÉMARRAGE DU MODULE
// ═══════════════════════════════════════════════════════════════════════════

/**
 * Fonction appelée au démarrage de chaque module
 */
void AkcpmBlockCode::startup() {
    console << "═══════════════════════════════════════════════════════════\n";
    console << "Module " << hostBlock->blockId << " - Protocole AKC-PM (SINGLE_KEY)\n";
    console << "═══════════════════════════════════════════════════════════\n";
    
    Catoms3DBlock* catom = (Catoms3DBlock*)hostBlock;
    Cell3DPosition pos = catom->position;
    
    // Initialiser le lattice pour les mouvements
    lattice = (FCCLattice*)(Catoms3DWorld::getWorld()->lattice);
    
    // Le module ID=1 sauvegarde la configuration initiale
    if (hostBlock->blockId == 1) {
        sauvegarderConfiguration();
    }
    
    console << "  Position: (" << pos[0] << "," << pos[1] << "," << pos[2] << ")\n";
    console << "  Voisins connectés: " << compterVoisinsConnectes() << "\n";
    console << "  Mobile: " << (isMobile ? "Oui" : "Non") << "\n";
    
    // Le module ID=1 est désigné comme initial (iM)
    if (hostBlock->blockId == 1) {
        definirCommeModuleInitial();
        
        // Calculer le nombre d'étapes max depuis le plan de mouvements
        for (const auto& mv : planMouvements) {
            if (mv.stage > myMaxStage) myMaxStage = mv.stage;
        }
        
        // Planifier la notification aux voisins après 1ms
        getScheduler()->schedule(
            new InterruptionEvent<unsigned int>(getScheduler()->now() + 1000, hostBlock, 0));
    } else {
        // Module non-initial: couleur grise, en attente
        catom->setColor(GREY);
        console << "  En attente du signal STRUCTURE_READY...\n";
    }
}

// ═══════════════════════════════════════════════════════════════════════════
// ALGORITHME 1: AUTHENTIFICATION POUR FORMATION DE STRUCTURE
// Référence: Section VI.A de l'article
// ═══════════════════════════════════════════════════════════════════════════

/**
 * SF-1, SF-2: Le nouveau module (nM) initie l'authentification
 * 
 * Étapes:
 *   SF-1: nM génère n0, calcule n1=H(n0), K0=H(L(n0 mod Nb)), x=Ts⊕n0
 *   SF-2: nM envoie (liens, n1, x||K0) au module de structure
 */
void AkcpmBlockCode::algorithme1_Initier(P2PNetworkInterface* dest) {
    console << "───────────────────────────────────────────────────────────\n";
    console << "Module " << hostBlock->blockId << ": ALGORITHME 1 - INITIATION (SF-1, SF-2)\n";
    console << "───────────────────────────────────────────────────────────\n";
    
    // ═══════════════════════════════════════════════════════════════════════
    // SF-1: Génération des valeurs cryptographiques
    // ═══════════════════════════════════════════════════════════════════════
    
    // Générer le nonce secret n0
    infoSecurite.n0 = genererNonce();
    
    // Calculer n1 = H(n0) pour vérification de synchronisation
    infoSecurite.n1 = H(infoSecurite.n0);
    
    // Calculer K0 = H(L(n0 mod Nb)) - empreinte d'authentification
    infoSecurite.K0 = HL(infoSecurite.n0);
    
    // Calculer x = Ts ⊕ n0 - protection du nonce avec le timestamp
    uint64_t ts_brut = getScheduler()->now();
    uint64_t Ts = ts_brut / DIVISEUR_SYNC;
    uint64_t x = XOR(Ts, infoSecurite.n0);
    
    console << "  n0 = " << infoSecurite.n0 << " (secret)\n";
    console << "  n1 = H(n0) = " << infoSecurite.n1 << "\n";
    console << "  ts = " << ts_brut << ", Ts = ts/10 = " << Ts << "\n";
    console << "  x = Ts ⊕ n0 = " << x << "\n";
    
    // ═══════════════════════════════════════════════════════════════════════
    // SF-2: Envoi du message d'authentification
    // ═══════════════════════════════════════════════════════════════════════
    
    // liens = 1 signifie "pas encore lié à la structure"
    int liens = (infoSecurite.liensStructure > 0) ? infoSecurite.liensStructure : 1;
    
    MessageDemandeAuth* msg = new MessageDemandeAuth(
        liens, infoSecurite.n1, x, infoSecurite.K0);
    sendMessage(msg, dest, DELAI_TRANSMISSION, 0);
    
    // Mémoriser n0 pour cette authentification en cours
    n0EnAttente[dest] = infoSecurite.n0;
    authentificationsEnCours.insert(dest);
    infoSecurite.etat = ETAT_AUTHENTIFICATION;
    
    console << "  → Message AUTH_REQUEST envoyé (liens=" << liens << ", n1, x||K0)\n";
}

/**
 * SF-3, SF-4: Le module de structure (sM) vérifie et répond
 * 
 * Étapes:
 *   SF-3: sM calcule Ts≈((Tr-Δt)/10), dérive n0'=x⊕Ts, vérifie n1=H(n0')
 *         puis vérifie K0=H(L(n0' mod Nb))
 *   SF-4: sM génère n2 (ou utilise n2Global pour SINGLE_KEY),
 *         calcule x1=n2⊕n0', K1=H(L(n2 mod Nb)), envoie x1
 */
void AkcpmBlockCode::algorithme1_Verifier(P2PNetworkInterface* src, int liens, 
                                           uint64_t n1, uint64_t x, 
                                           const std::vector<uint8_t>& K0) {
    console << "───────────────────────────────────────────────────────────\n";
    console << "Module " << hostBlock->blockId << ": ALGORITHME 1 - VÉRIFICATION (SF-3, SF-4)\n";
    console << "───────────────────────────────────────────────────────────\n";
    
    // Seuls les modules de la structure peuvent authentifier
    if (!estDansStructure()) {
        console << "  ✗ REJETÉ: Pas dans la structure\n";
        return;
    }
    
    // ═══════════════════════════════════════════════════════════════════════
    // SF-3: Vérification de la synchronisation et de l'authenticité
    // ═══════════════════════════════════════════════════════════════════════
    
    uint64_t Tr = getScheduler()->now();
    uint64_t n0_prime = 0;
    bool syncOK = false;
    
    // Calculer Ts_base avec le délai de transmission
    uint64_t Ts_base = (Tr - DELAI_TRANSMISSION) / DIVISEUR_SYNC;
    
    console << "  Tr = " << Tr << ", Δt = " << DELAI_TRANSMISSION << "\n";
    console << "  Ts_base = (Tr - Δt)/10 = " << Ts_base << "\n";
    
    // Essayer avec tolérance ±5 (comme décrit Section VII.A)
    for (int offset = -TOLERANCE_SYNC; offset <= TOLERANCE_SYNC && !syncOK; offset++) {
        uint64_t Ts_test = Ts_base + offset;
        
        // Dériver n0' = x ⊕ Ts
        uint64_t n0_test = XOR(x, Ts_test);
        
        // Vérifier si n1 = H(n0')
        uint64_t n1_calcule = H(n0_test);
        
        if (n1_calcule == n1) {
            n0_prime = n0_test;
            syncOK = true;
            console << "  ✓ Sync OK (offset=" << offset << ", Ts=" << Ts_test << ")\n";
        }
    }
    
    if (!syncOK) {
        console << "  ✗ ÉCHEC SYNC: Impossible de vérifier n1 = H(n0')\n";
        console << "    Plage testée: Ts de " << (Ts_base - TOLERANCE_SYNC) 
                << " à " << (Ts_base + TOLERANCE_SYNC) << "\n";
        return;
    }
    
    console << "  n0' (récupéré) = " << n0_prime << "\n";
    
    // Vérifier K0 = H(L(n0' mod Nb))
    std::vector<uint8_t> K0_calcule = HL(n0_prime);
    if (K0_calcule != K0) {
        console << "  ✗ ÉCHEC AUTH: K0 ne correspond pas (empreinte invalide)\n";
        return;
    }
    
    console << "  ✓ K0 vérifié - Module authentifié!\n";
    
    // ═══════════════════════════════════════════════════════════════════════
    // SF-4: Génération du défi de clé (spécifique SINGLE_KEY)
    // ═══════════════════════════════════════════════════════════════════════
    
    uint64_t n2;
    
    // SINGLE_KEY: utiliser le même n2 pour tous les modules
    if (infoSecurite.estModuleInitial && !n2GlobalDefini) {
        // Le module initial génère n2 une seule fois
        n2Global = genererNonce();
        n2GlobalDefini = true;
        console << "  [SINGLE_KEY] n2 global généré par le module initial\n";
    }
    n2 = n2Global;
    
    // Calculer x1 = n2 ⊕ n0'
    uint64_t x1 = XOR(n2, n0_prime);
    
    // Calculer K1 = H(L(n2 mod Nb)) - la clé partagée
    std::vector<uint8_t> K1 = HL(n2);
    
    console << "  n2 = " << n2 << " (global pour SINGLE_KEY)\n";
    console << "  x1 = n2 ⊕ n0' = " << x1 << "\n";
    console << "  K1 = H(L(n2 mod Nb)) - clé unique pour tout le réseau\n";
    
    // Enregistrer la clé et l'état
    clesVoisins[src] = K1;
    voisinsAuthentifies[src] = true;
    voisinsDansStructure[src] = true;
    infoSecurite.liensStructure++;
    
    // Envoyer x1 au nouveau module
    MessageDefiCle* msg = new MessageDefiCle(x1);
    sendMessage(msg, src, DELAI_TRANSMISSION, 0);
    
    console << "  → Message KEY_CHALLENGE envoyé (x1)\n";
    console << "  Liens dans la structure: " << infoSecurite.liensStructure << "\n";
}

/**
 * SF-5: Le nouveau module (nM) complète l'authentification
 * 
 * Étapes:
 *   - nM calcule n2 = x1 ⊕ n0
 *   - nM calcule K1 = H(L(n2 mod Nb))
 *   - Authentification mutuelle réussie, clé K1 partagée
 */
void AkcpmBlockCode::algorithme1_Completer(P2PNetworkInterface* src, uint64_t x1) {
    console << "───────────────────────────────────────────────────────────\n";
    console << "Module " << hostBlock->blockId << ": ALGORITHME 1 - COMPLÉTION (SF-5)\n";
    console << "───────────────────────────────────────────────────────────\n";
    
    // Récupérer n0 mémorisé pour cette authentification
    if (n0EnAttente.find(src) == n0EnAttente.end()) {
        console << "  ✗ ERREUR: Pas d'authentification en cours pour cette source\n";
        return;
    }
    
    uint64_t n0 = n0EnAttente[src];
    
    // Calculer n2 = x1 ⊕ n0
    uint64_t n2 = XOR(x1, n0);
    
    // Calculer K1 = H(L(n2 mod Nb))
    std::vector<uint8_t> K1 = HL(n2);
    
    console << "  n2 = x1 ⊕ n0 = " << n2 << "\n";
    console << "  K1 = H(L(n2 mod Nb)) générée\n";
    
    // Enregistrer la clé et mettre à jour l'état
    infoSecurite.n2 = n2;
    infoSecurite.K1 = K1;
    clesVoisins[src] = K1;
    voisinsAuthentifies[src] = true;
    voisinsDansStructure[src] = true;
    infoSecurite.etat = ETAT_CLE_ETABLIE;
    infoSecurite.liensStructure++;
    
    // Nettoyer les données temporaires
    n0EnAttente.erase(src);
    authentificationsEnCours.erase(src);
    
    console << "╔═══════════════════════════════════════════════════════════╗\n";
    console << "║  ★★★ AUTHENTIFICATION RÉUSSIE ★★★                        ║\n";
    console << "║  Module intégré à la structure!                          ║\n";
    console << "║  Clé K1 partagée établie (SINGLE_KEY)                    ║\n";
    console << "║  Cette clé K1 est identique pour tout le réseau          ║\n";
    console << "╚═══════════════════════════════════════════════════════════╝\n";
    
    // Changer la couleur en VERT (authentifié)
    Catoms3DBlock* catom = (Catoms3DBlock*)hostBlock;
    catom->setColor(GREEN);
    
    // Notifier les autres voisins
    notifierVoisinsStructurePrete();
    
    // Si des mouvements sont planifiés et que ce module est mobile, 
    // lancer la phase de reconfiguration après un délai
    if (!planMouvements.empty() && isMobile) {
        // Calculer myMaxStage si pas encore fait
        for (const auto& mv : planMouvements) {
            if (mv.stage > myMaxStage) myMaxStage = mv.stage;
        }
        
        // Programmer le démarrage de la reconfiguration
        getScheduler()->schedule(
            new InterruptionEvent<unsigned int>(getScheduler()->now() + 500000, hostBlock, 1));
    }
}

// ═══════════════════════════════════════════════════════════════════════════
// GESTION DE LA STRUCTURE
// ═══════════════════════════════════════════════════════════════════════════

/**
 * Notifie tous les voisins non authentifiés que ce module fait partie
 * de la structure et peut les authentifier
 */
void AkcpmBlockCode::notifierVoisinsStructurePrete() {
    Catoms3DBlock* catom = (Catoms3DBlock*)hostBlock;
    
    console << "Module " << hostBlock->blockId << ": Diffusion STRUCTURE_READY\n";
    
    for (int i = 0; i < 12; i++) {
        P2PNetworkInterface* iface = catom->getInterface(i);
        if (iface && iface->connectedInterface) {
            // Ne pas notifier les voisins déjà authentifiés
            if (voisinsAuthentifies.find(iface) == voisinsAuthentifies.end()) {
                console << "  → Interface " << i << "\n";
                MessageStructurePrete* msg = new MessageStructurePrete();
                sendMessage(msg, iface, DELAI_TRANSMISSION, 0);
            }
        }
    }
}

/**
 * Vérifie s'il y a de nouveaux voisins à authentifier
 */
void AkcpmBlockCode::verifierNouveauxVoisins() {
    Catoms3DBlock* catom = (Catoms3DBlock*)hostBlock;
    
    for (int i = 0; i < 12; i++) {
        P2PNetworkInterface* iface = catom->getInterface(i);
        if (iface && iface->connectedInterface) {
            // Vérifier si c'est un nouveau voisin
            if (voisinsDansStructure.find(iface) == voisinsDansStructure.end() &&
                voisinsAuthentifies.find(iface) == voisinsAuthentifies.end() &&
                authentificationsEnCours.find(iface) == authentificationsEnCours.end()) {
                
                if (estDansStructure()) {
                    console << "Module " << hostBlock->blockId << ": Nouveau voisin détecté\n";
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

/**
 * Gestionnaire de réception d'une demande d'authentification
 */
void AkcpmBlockCode::surReceptionDemandeAuth(std::shared_ptr<Message> msg, 
                                              P2PNetworkInterface* src) {
    MessageDemandeAuth* authMsg = static_cast<MessageDemandeAuth*>(msg.get());
    
    console << "Module " << hostBlock->blockId << ": Réception AUTH_REQUEST\n";
    console << "  liens = " << authMsg->liens << "\n";
    
    // Appeler la vérification de l'Algorithme 1
    algorithme1_Verifier(src, authMsg->liens, authMsg->n1, authMsg->x, authMsg->K0);
}

/**
 * Gestionnaire de réception d'un défi de clé
 */
void AkcpmBlockCode::surReceptionDefiCle(std::shared_ptr<Message> msg,
                                          P2PNetworkInterface* src) {
    MessageDefiCle* cleMsg = static_cast<MessageDefiCle*>(msg.get());
    
    console << "Module " << hostBlock->blockId << ": Réception KEY_CHALLENGE\n";
    
    // Appeler la complétion de l'Algorithme 1
    algorithme1_Completer(src, cleMsg->x1);
}

/**
 * Gestionnaire de réception du signal STRUCTURE_READY
 */
void AkcpmBlockCode::surReceptionStructurePrete(std::shared_ptr<Message> msg,
                                                 P2PNetworkInterface* src) {
    console << "───────────────────────────────────────────────────────────\n";
    console << "Module " << hostBlock->blockId << ": Réception STRUCTURE_READY\n";
    
    // Marquer ce voisin comme étant dans la structure
    voisinsDansStructure[src] = true;
    
    // Si je ne suis pas dans la structure, initier l'authentification
    if (!estDansStructure()) {
        if (authentificationsEnCours.find(src) == authentificationsEnCours.end() &&
            voisinsAuthentifies.find(src) == voisinsAuthentifies.end()) {
            
            console << "  → Initiation de l'authentification (Algorithme 1)\n";
            algorithme1_Initier(src);
        }
    } else {
        console << "  Déjà dans la structure\n";
    }
}

// ═══════════════════════════════════════════════════════════════════════════
// TRAITEMENT DES ÉVÉNEMENTS
// ═══════════════════════════════════════════════════════════════════════════

/**
 * Traitement des événements locaux
 */
void AkcpmBlockCode::processLocalEvent(EventPtr pev) {
    MessagePtr message;
    
    switch (pev->eventType) {
        case EVENT_NI_RECEIVE: {
            // Réception d'un message
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
                case MSG_MOTION_SYNC:
                    surReceptionMotionSync(message, src);
                    break;
            }
            break;
        }
        
        case EVENT_ADD_NEIGHBOR: {
            // Nouveau voisin détecté
            verifierNouveauxVoisins();
            break;
        }
        
        case EVENT_INTERRUPTION: {
            // Récupérer l'ID de l'interruption
            unsigned int interruptId = (std::static_pointer_cast<InterruptionEvent<unsigned int>>(pev))->mode;
            
            if (interruptId == 0) {
                // Le module initial démarre le protocole
                if (infoSecurite.estModuleInitial) {
                    console << "Module " << hostBlock->blockId << ": Démarrage du protocole\n";
                    notifierVoisinsStructurePrete();
                }
            } else if (interruptId == 1) {
                // Démarrer la reconfiguration
                console << "Module " << hostBlock->blockId << ": Démarrage de la reconfiguration\n";
                verifierEtLancerReconfiguration();
            }
            break;
        }
        
        default:
            break;
    }
}

/**
 * Appelée à la fin d'un mouvement
 */
void AkcpmBlockCode::onMotionEnd() {
    Catoms3DBlock* catom = (Catoms3DBlock*)hostBlock;
    
    console << "═══════════════════════════════════════════════════════════\n";
    console << "Module " << hostBlock->blockId << ": FIN DE MOUVEMENT\n";
    console << "  Nouvelle position: " << catom->position << "\n";
    console << "═══════════════════════════════════════════════════════════\n";
    
    // Incrémenter l'étape
    myStage++;
    
    // Vérifier les nouveaux voisins
    verifierNouveauxVoisins();
    
    // Envoyer la synchronisation aux voisins
    sendMessageToAllNeighbors("MotionSync", 
        new MessageMotionSync(myStage, true), 
        DELAI_TRANSMISSION, 0, 0);
    
    // Si on n'a pas atteint la fin, chercher le prochain mouvement
    if (myStage <= myMaxStage) {
        executerMouvementEtape();
    } else {
        console << "  ★ Reconfiguration terminée pour ce module!\n";
        // Couleur finale selon la forme cible
        if (target && target->isInTarget(catom->position)) {
            catom->setColor(target->getTargetColor(catom->position));
        } else {
            catom->setColor(GREEN);
        }
    }
}

/**
 * Appelée lors d'un clic sur le module (affiche l'état)
 */
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
    console << "║ Module Initial (iM): " << (infoSecurite.estModuleInitial ? "OUI" : "NON") << "\n";
    console << "║ Dans la structure: " << (estDansStructure() ? "OUI" : "NON") << "\n";
    console << "║ Liens dans structure: " << infoSecurite.liensStructure << "\n";
    console << "║ Voisins authentifiés: " << voisinsAuthentifies.size() << "\n";
    console << "║ Mobile: " << (isMobile ? "OUI" : "NON") << "\n";
    console << "║ Étape mouvement: " << myStage << " / " << myMaxStage << "\n";
    console << "║ Modèle de clé: SINGLE_KEY (clé unique K1)\n";
    console << "╚═══════════════════════════════════════════════════════════╝\n";
}

// ═══════════════════════════════════════════════════════════════════════════
// IMPLÉMENTATION DES CLASSES DE MESSAGES
// ═══════════════════════════════════════════════════════════════════════════

MessageDemandeAuth::MessageDemandeAuth(int _liens, uint64_t _n1, uint64_t _x,
                                       const std::vector<uint8_t>& _K0)
    : Message(), liens(_liens), n1(_n1), x(_x), K0(_K0) {
    type = MSG_AUTH_REQUEST;
}
MessageDemandeAuth::~MessageDemandeAuth() {}

MessageDefiCle::MessageDefiCle(uint64_t _x1)
    : Message(), x1(_x1) {
    type = MSG_KEY_CHALLENGE;
}
MessageDefiCle::~MessageDefiCle() {}

MessageStructurePrete::MessageStructurePrete() : Message() {
    type = MSG_STRUCTURE_READY;
}
MessageStructurePrete::~MessageStructurePrete() {}

MessageMotionSync::MessageMotionSync(int _stage, bool _allAuth)
    : Message(), stage(_stage), allAuthenticated(_allAuth) {
    type = MSG_MOTION_SYNC;
}
MessageMotionSync::~MessageMotionSync() {}

// ═══════════════════════════════════════════════════════════════════════════
// GESTION DES MOUVEMENTS
// ═══════════════════════════════════════════════════════════════════════════

/**
 * Sauvegarde la configuration initiale de tous les modules
 */
void AkcpmBlockCode::sauvegarderConfiguration() {
    auto modules = Catoms3DWorld::getWorld()->getMap();
    configurationInitiale.clear();
    for (auto& module : modules) {
        configurationInitiale[module.first] = {
            module.second->position, 
            module.second->orientationCode
        };
    }
    console << "Configuration initiale sauvegardée (" << configurationInitiale.size() << " modules)\n";
}

/**
 * Restaure la configuration initiale
 */
void AkcpmBlockCode::restaurerConfiguration() {
    auto wrld = Catoms3DWorld::getWorld();
    auto modules = wrld->getMap();
    
    // Déconnecter tous les modules
    for (auto& module : modules) {
        lattice->remove(module.second->position, false);
        wrld->disconnectBlock(module.second, false);
    }
    
    // Repositionner les modules
    for (auto& module : modules) {
        module.second->setPositionAndOrientation(
            configurationInitiale[module.first].first,
            configurationInitiale[module.first].second
        );
    }
    
    // Reconnecter
    for (auto& module : modules) {
        wrld->connectBlock(module.second, false);
    }
    
    console << "Configuration restaurée\n";
}

/**
 * Parse les attributs personnalisés du fichier XML
 */
void AkcpmBlockCode::parseUserBlockElements(TiXmlElement *config) {
    const char* attr = config->Attribute("mobile");
    if (attr != nullptr) {
        string str(attr);
        if (str == "true" || str == "1" || str == "yes") {
            isMobile = true;
            console << "Module " << hostBlock->blockId << " marqué comme mobile\n";
        }
    }
}

/**
 * Tente d'effectuer un mouvement vers la position cible
 */
bool AkcpmBlockCode::tryToMove(const Cell3DPosition& targetPos) {
    Catoms3DBlock* catom = (Catoms3DBlock*)hostBlock;
    
    // Obtenir toutes les rotations possibles pour ce module
    vector<pair<const Catoms3DMotionRulesLink*, Catoms3DRotation>> rotations = 
        Catoms3DMotionEngine::getAllRotationsForModule(catom);
    
    console << "  Rotations possibles: " << rotations.size() << "\n";
    
    Cell3DPosition finalPos;
    short finalOrient;
    
    for (auto& rotation : rotations) {
        rotation.second.init(((Catoms3DGlBlock*)catom->ptrGlBlock)->mat);
        rotation.second.getFinalPositionAndOrientation(finalPos, finalOrient);
        
        console << "    Test: " << catom->position << " -> " << finalPos 
                << " (cible: " << targetPos << ")\n";
        
        if (finalPos == targetPos) {
            // Mouvement trouvé! Programmer la rotation
            console << "  ★ Mouvement trouvé vers " << finalPos << "\n";
            
            getScheduler()->schedule(
                new Catoms3DRotationStartEvent(
                    getScheduler()->now() + 100000,  // Délai de 100ms
                    catom,
                    rotation.second.pivot,
                    finalPos
                )
            );
            return true;
        }
    }
    
    console << "  ✗ Aucun mouvement valide vers " << targetPos << "\n";
    return false;
}

/**
 * Exécute le mouvement pour l'étape courante si ce module doit bouger
 */
void AkcpmBlockCode::executerMouvementEtape() {
    Catoms3DBlock* catom = (Catoms3DBlock*)hostBlock;
    Cell3DPosition currentPos = catom->position;
    
    console << "Module " << hostBlock->blockId << ": Recherche mouvement étape " << myStage << "\n";
    
    // Chercher un mouvement pour cette étape et cette position
    for (const auto& mv : planMouvements) {
        if (mv.stage == myStage && mv.posFrom == currentPos) {
            console << "  Mouvement planifié: " << mv.posFrom << " -> " << mv.posTo << "\n";
            if (tryToMove(mv.posTo)) {
                return;  // Mouvement initié
            }
        }
    }
    
    console << "  Pas de mouvement pour ce module à cette étape\n";
}

/**
 * Vérifie si tous les modules sont authentifiés et lance la reconfiguration
 */
void AkcpmBlockCode::verifierEtLancerReconfiguration() {
    if (reconfigurationActive) return;
    
    // Vérifier que ce module est authentifié
    if (!estDansStructure()) return;
    
    console << "Module " << hostBlock->blockId << ": Vérification pour lancer reconfiguration\n";
    
    // On suppose que tous les modules sont authentifiés quand on reçoit ce signal
    reconfigurationActive = true;
    myStage = 0;
    
    // Lancer le premier mouvement
    executerMouvementEtape();
}

/**
 * Gestionnaire de message de synchronisation des mouvements
 */
void AkcpmBlockCode::surReceptionMotionSync(std::shared_ptr<Message> msg, P2PNetworkInterface* src) {
    MessageMotionSync* syncMsg = static_cast<MessageMotionSync*>(msg.get());
    
    console << "Module " << hostBlock->blockId << ": Reçu MotionSync (stage=" 
            << syncMsg->stage << ")\n";
    
    // Si on n'est pas encore en reconfiguration et que le message indique que tout est authentifié
    if (!reconfigurationActive && syncMsg->allAuthenticated) {
        verifierEtLancerReconfiguration();
    }
    
    // Synchroniser l'étape si nécessaire
    if (syncMsg->stage != myStage && syncMsg->stage <= myMaxStage) {
        myStage = syncMsg->stage;
        
        // Propager aux voisins
        sendMessageToAllNeighbors("MotionSync", 
            new MessageMotionSync(myStage, true), 
            DELAI_TRANSMISSION, 0, 1, src);
        
        // Exécuter le mouvement pour cette étape
        executerMouvementEtape();
    }
}

/**
 * Affiche des informations dans l'interface graphique
 */
std::string AkcpmBlockCode::onInterfaceDraw() {
    string str = "Module #" + to_string(hostBlock->blockId);
    str += "\nÉtat: ";
    switch (infoSecurite.etat) {
        case ETAT_NON_LIE: str += "Non lié"; break;
        case ETAT_AUTHENTIFICATION: str += "Auth..."; break;
        case ETAT_AUTHENTIFIE: str += "Authentifié"; break;
        case ETAT_CLE_ETABLIE: str += "Clé K1 établie"; break;
    }
    str += "\nÉtape: " + to_string(myStage) + "/" + to_string(myMaxStage);
    return str;
}
