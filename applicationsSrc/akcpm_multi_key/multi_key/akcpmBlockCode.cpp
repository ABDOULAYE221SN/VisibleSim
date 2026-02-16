/**
 * ╔═══════════════════════════════════════════════════════════════════════════╗
 * ║               IMPLÉMENTATION DU MODÈLE MULTI_KEY                          ║
 * ║                                                                           ║
 * ║  Ce fichier implémente le protocole AKC-PM avec le modèle multi-clés     ║
 * ║  où CHAQUE LIEN possède sa propre clé unique.                            ║
 * ╚═══════════════════════════════════════════════════════════════════════════╝
 */

#include <iostream>
#include <random>
#include <cmath>
#include "akcpmBlockCode.h"

using namespace std;
using namespace Catoms3D;

// ═══════════════════════════════════════════════════════════════════════════
// DÉFINITIONS DES CONSTANTES STATIQUES
// ═══════════════════════════════════════════════════════════════════════════

const int AkcpmBlockCode::NB_LIGNES_CODE;
const int AkcpmBlockCode::TAILLE_LIGNE_BITS;
const int AkcpmBlockCode::DIVISEUR_SYNC;
const int AkcpmBlockCode::TOLERANCE_SYNC;
const int AkcpmBlockCode::DELAI_TRANSMISSION;

// ═══════════════════════════════════════════════════════════════════════════
// CONSTRUCTEUR / DESTRUCTEUR
// ═══════════════════════════════════════════════════════════════════════════

AkcpmBlockCode::AkcpmBlockCode(Catoms3DBlock *host) : Catoms3DBlockCode(host) {
}

AkcpmBlockCode::~AkcpmBlockCode() {
    clesVoisins.clear();
    n2Voisins.clear();
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
    
    Catoms3DBlock* catom = (Catoms3DBlock*)hostBlock;
    catom->setColor(BLUE);
    
    console << "═══════════════════════════════════════════════════════════\n";
    console << "★ MODULE " << hostBlock->blockId << " DÉFINI COMME INITIAL (iM) ★\n";
    console << "  Modèle: MULTI_KEY (une clé unique par lien)\n";
    console << "  Chaque connexion génère un nouveau n2 et donc une nouvelle clé\n";
    console << "═══════════════════════════════════════════════════════════\n";
}

// ═══════════════════════════════════════════════════════════════════════════
// DÉMARRAGE
// ═══════════════════════════════════════════════════════════════════════════

void AkcpmBlockCode::startup() {
    console << "═══════════════════════════════════════════════════════════\n";
    console << "Module " << hostBlock->blockId << " - Protocole AKC-PM (MULTI_KEY)\n";
    console << "═══════════════════════════════════════════════════════════\n";
    
    Catoms3DBlock* catom = (Catoms3DBlock*)hostBlock;
    Cell3DPosition pos = catom->position;
    
    console << "  Position: (" << pos[0] << "," << pos[1] << "," << pos[2] << ")\n";
    console << "  Voisins connectés: " << compterVoisinsConnectes() << "\n";
    
    if (pos[0] == 5 && pos[1] == 5 && pos[2] == 0) {
        definirCommeModuleInitial();
        getScheduler()->schedule(
            new InterruptionEvent(getScheduler()->now() + 1000, hostBlock, 0));
    } else {
        catom->setColor(GREY);
        console << "  En attente du signal STRUCTURE_READY...\n";
    }
}

// ═══════════════════════════════════════════════════════════════════════════
// ALGORITHME 1 - MULTI_KEY
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
    console << "  x = Ts ⊕ n0\n";
    
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
 * SF-3, SF-4: Vérification et génération de défi (MULTI_KEY)
 * 
 * Spécificité MULTI_KEY:
 *   - Un NOUVEAU n2 est généré pour CHAQUE connexion
 *   - Chaque lien a donc sa propre clé unique K = H(L(n2 mod Nb))
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
    // SF-4: MULTI_KEY - Génération d'un NOUVEAU n2 pour ce lien
    // ═══════════════════════════════════════════════════════════════════════
    
    // Dans MULTI_KEY, chaque lien a son propre n2
    uint64_t n2 = genererNonce();
    
    console << "  [MULTI_KEY] Nouveau n2 généré pour ce lien: " << n2 << "\n";
    
    // Calculer x1 = n2 ⊕ n0'
    uint64_t x1 = XOR(n2, n0_prime);
    
    // Calculer K = H(L(n2 mod Nb)) - clé unique pour ce lien
    std::vector<uint8_t> cleLien = HL(n2);
    
    console << "  x1 = n2 ⊕ n0' = " << x1 << "\n";
    console << "  Clé unique générée pour ce lien: K_" << (clesVoisins.size() + 1) << "\n";
    
    // Enregistrer
    clesVoisins[src] = cleLien;
    n2Voisins[src] = n2;
    voisinsAuthentifies[src] = true;
    voisinsDansStructure[src] = true;
    infoSecurite.liensStructure++;
    
    // Envoyer le défi
    MessageDefiCle* msg = new MessageDefiCle(x1);
    sendMessage(msg, src, DELAI_TRANSMISSION, 0);
    
    console << "  → Message KEY_CHALLENGE envoyé\n";
    console << "  Nombre total de clés sur ce module: " << clesVoisins.size() << "\n";
}

/**
 * SF-5: Complétion de l'authentification (MULTI_KEY)
 */
void AkcpmBlockCode::algorithme1_Completer(P2PNetworkInterface* src, uint64_t x1) {
    console << "───────────────────────────────────────────────────────────\n";
    console << "Module " << hostBlock->blockId << ": ALGORITHME 1 - COMPLÉTION (SF-5)\n";
    console << "───────────────────────────────────────────────────────────\n";
    
    if (n0EnAttente.find(src) == n0EnAttente.end()) {
        console << "  ✗ ERREUR: Pas d'authentification en cours\n";
        return;
    }
    
    uint64_t n0 = n0EnAttente[src];
    
    // Calculer n2 = x1 ⊕ n0
    uint64_t n2 = XOR(x1, n0);
    
    // Calculer K = H(L(n2 mod Nb)) - clé unique pour ce lien
    std::vector<uint8_t> cleLien = HL(n2);
    
    console << "  n2 = x1 ⊕ n0 = " << n2 << "\n";
    console << "  [MULTI_KEY] Clé unique générée pour ce lien\n";
    
    // Enregistrer
    clesVoisins[src] = cleLien;
    n2Voisins[src] = n2;
    voisinsAuthentifies[src] = true;
    voisinsDansStructure[src] = true;
    infoSecurite.etat = ETAT_CLE_ETABLIE;
    infoSecurite.liensStructure++;
    
    n0EnAttente.erase(src);
    authentificationsEnCours.erase(src);
    
    console << "╔═══════════════════════════════════════════════════════════╗\n";
    console << "║  ★★★ AUTHENTIFICATION RÉUSSIE ★★★                        ║\n";
    console << "║  Module intégré à la structure!                          ║\n";
    console << "║  Clé UNIQUE établie pour ce lien (modèle MULTI_KEY)      ║\n";
    console << "║  Nombre de clés sur ce module: " << clesVoisins.size() << "                        ║\n";
    console << "╚═══════════════════════════════════════════════════════════╝\n";
    
    // Dans MULTI_KEY, on utilise une couleur basée sur le nombre de clés
    // pour visualiser la diversité
    Catoms3DBlock* catom = (Catoms3DBlock*)hostBlock;
    
    // Couleur verte de base, légèrement différente selon le nombre de clés
    int nbCles = clesVoisins.size();
    if (nbCles == 1) {
        catom->setColor(GREEN);
    } else if (nbCles == 2) {
        catom->setColor(CYAN);
    } else if (nbCles == 3) {
        catom->setColor(LIGHTBLUE);
    } else {
        catom->setColor(MAGENTA);
    }
    
    notifierVoisinsStructurePrete();
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
    console << "Module " << hostBlock->blockId << ": Réception KEY_CHALLENGE\n";
    algorithme1_Completer(src, cleMsg->x1);
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
            
        case EVENT_INTERRUPTION:
            if (infoSecurite.estModuleInitial) {
                notifierVoisinsStructurePrete();
            }
            break;
            
        default:
            break;
    }
}

void AkcpmBlockCode::onMotionEnd() {
    verifierNouveauxVoisins();
}

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
    console << "║ Modèle: MULTI_KEY (clé unique par lien)\n";
    console << "║ Nombre de clés distinctes: " << clesVoisins.size() << "\n";
    console << "║ Liens authentifiés: " << infoSecurite.liensStructure << "\n";
    console << "╚═══════════════════════════════════════════════════════════╝\n";
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

MessageDefiCle::MessageDefiCle(uint64_t _x1)
    : Message(), x1(_x1) {
    type = MSG_KEY_CHALLENGE;
}
MessageDefiCle::~MessageDefiCle() {}

MessageStructurePrete::MessageStructurePrete() : Message() {
    type = MSG_STRUCTURE_READY;
}
MessageStructurePrete::~MessageStructurePrete() {}
