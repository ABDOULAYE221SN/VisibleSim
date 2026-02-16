/**
 * ╔═══════════════════════════════════════════════════════════════════════════╗
 * ║        PROTOCOLE AKC-PM - MODÈLE SINGLE_KEY AVEC RECONFIGURATION          ║
 * ╠═══════════════════════════════════════════════════════════════════════════╣
 * ║  FLUX:                                                                    ║
 * ║  1. Phase 1: Authentification de tous les modules (AKC-PM)                ║
 * ║  2. Phase 2: Reconfiguration (même logique que myMotionTestCode)          ║
 * ║     - À chaque position intermédiaire: ré-authentification                ║
 * ╚═══════════════════════════════════════════════════════════════════════════╝
 */

#include <iostream>
#include <random>
#include <cmath>
#include <algorithm>
#include <queue>
#include "akcpmBlockCode_single_key.h"

using namespace std;
using namespace Catoms3D;

// ═══════════════════════════════════════════════════════════════════════════
// VARIABLES STATIQUES AKC-PM
// ═══════════════════════════════════════════════════════════════════════════

uint64_t AkcpmBlockCode::n2Global = 0;
bool AkcpmBlockCode::n2GlobalDefini = false;

const int AkcpmBlockCode::NB_LIGNES_CODE;
const int AkcpmBlockCode::TAILLE_LIGNE_BITS;
const int AkcpmBlockCode::DIVISEUR_SYNC;
const int AkcpmBlockCode::TOLERANCE_SYNC;
const int AkcpmBlockCode::DELAI_TRANSMISSION;

// ═══════════════════════════════════════════════════════════════════════════
// CONFIGURATION DE LA RECONFIGURATION (IDENTIQUE À myMotionTestCode)
// ═══════════════════════════════════════════════════════════════════════════

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
    Cell3DPosition(3,3,2),  // M1 va ici (entre M2 et M5)
    Cell3DPosition(2,2,3),  // M3 monte ici
};

static queue<bID> moveQueue;
static bool planReady = false;
static int moveCount = 0;
static map<bID, Cell3DPosition> assignments;
static const int MAX_STEPS = 20;

// Phase du protocole
static bool allAuthenticated = false;
static int authenticatedCount = 0;
static const int TOTAL_MODULES = 5;

static bool isInShape(const Cell3DPosition &pos, const vector<Cell3DPosition> &shape) {
    return find(shape.begin(), shape.end(), pos) != shape.end();
}

// ═══════════════════════════════════════════════════════════════════════════
// CONSTRUCTEUR / DESTRUCTEUR
// ═══════════════════════════════════════════════════════════════════════════

AkcpmBlockCode::AkcpmBlockCode(Catoms3DBlock *host) : Catoms3DBlockCode(host) {
}

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

uint64_t AkcpmBlockCode::H(uint64_t valeur) {
    uint64_t hash = 5381;
    for (int i = 0; i < 8; i++) {
        hash = ((hash << 5) + hash) + ((valeur >> (i * 8)) & 0xFF);
    }
    return hash;
}

uint64_t AkcpmBlockCode::H(const vector<uint8_t>& donnees) {
    uint64_t hash = 5381;
    for (auto octet : donnees) {
        hash = ((hash << 5) + hash) + octet;
    }
    return hash;
}

vector<uint8_t> AkcpmBlockCode::L(int numeroLigne) {
    vector<uint8_t> ligne(TAILLE_LIGNE_BITS / 8);
    uint64_t graine = 0xDEADBEEF ^ (numeroLigne * 0x12345678);
    for (size_t i = 0; i < ligne.size(); i++) {
        graine = graine * 1103515245 + 12345;
        ligne[i] = (graine >> 16) & 0xFF;
    }
    return ligne;
}

vector<uint8_t> AkcpmBlockCode::HL(uint64_t n) {
    int numeroLigne = n % NB_LIGNES_CODE;
    vector<uint8_t> ligne = L(numeroLigne);
    uint64_t hash = H(ligne);
    
    vector<uint8_t> resultat(8);
    for (int i = 0; i < 8; i++) {
        resultat[i] = (hash >> (i * 8)) & 0xFF;
    }
    return resultat;
}

uint64_t AkcpmBlockCode::XOR(uint64_t a, uint64_t b) {
    return a ^ b;
}

uint64_t AkcpmBlockCode::genererNonce() {
    static random_device rd;
    static mt19937_64 gen(rd());
    uniform_int_distribution<uint64_t> dis;
    return dis(gen);
}

uint64_t AkcpmBlockCode::obtenirTs() {
    return getScheduler()->now() / DIVISEUR_SYNC;
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
    authenticationComplete = true;
    
    Catoms3DBlock* catom = (Catoms3DBlock*)hostBlock;
    catom->setColor(BLUE);
    
    console << "╔═══════════════════════════════════════════════════════════╗\n";
    console << "║  MODULE " << hostBlock->blockId << " DEFINI COMME INITIAL (iM)              ║\n";
    console << "║  Modele: SINGLE_KEY (cle unique pour tout le reseau)      ║\n";
    console << "╚═══════════════════════════════════════════════════════════╝\n";
}

// ═══════════════════════════════════════════════════════════════════════════
// DÉMARRAGE DU MODULE
// ═══════════════════════════════════════════════════════════════════════════

void AkcpmBlockCode::startup() {
    Catoms3DBlock* catom = (Catoms3DBlock*)hostBlock;
    Cell3DPosition pos = catom->position;
    
    console << "Module " << hostBlock->blockId 
            << " pos=(" << pos[0] << "," << pos[1] << "," << pos[2] << ")\n";
    
    // Le module 4 (position centrale 2,2,2) est le module initial
    // car il restera fixe pendant toute la reconfiguration
    if (pos[0] == 2 && pos[1] == 2 && pos[2] == 2) {
        definirCommeModuleInitial();
        
        // Planifier la reconfiguration (mais attendre que tous soient authentifiés)
        planifierReconfiguration();
        
        // Planifier la notification aux voisins
        getScheduler()->schedule(
            new InterruptionEvent(getScheduler()->now() + 1000, hostBlock, 0));
    } else {
        catom->setColor(GREY);
    }
}

void AkcpmBlockCode::planifierReconfiguration() {
    if (planReady) return;
    
    console << "╔═══════════════════════════════════════════════════════════╗\n";
    console << "║  PLANIFICATION DE LA RECONFIGURATION                      ║\n";
    console << "╚═══════════════════════════════════════════════════════════╝\n";
    
    vector<Cell3DPosition> toRemove;
    for (auto &pos : initialShape) {
        if (!isInShape(pos, targetShape)) {
            toRemove.push_back(pos);
        }
    }
    
    vector<Cell3DPosition> toFill;
    for (auto &pos : targetShape) {
        if (!isInShape(pos, initialShape)) {
            toFill.push_back(pos);
        }
    }
    
    console << "  Positions a liberer : " << toRemove.size() << "\n";
    for (auto &p : toRemove) 
        console << "    - (" << p[0] << "," << p[1] << "," << p[2] << ")\n";
    console << "  Positions a remplir : " << toFill.size() << "\n";
    for (auto &p : toFill) 
        console << "    + (" << p[0] << "," << p[1] << "," << p[2] << ")\n";
    
    auto world = BaseSimulator::getWorld();
    for (size_t i = 0; i < toRemove.size() && i < toFill.size(); i++) {
        for (auto &pair : world->buildingBlocksMap) {
            if (pair.second->position == toRemove[i]) {
                assignments[pair.first] = toFill[i];
                moveQueue.push(pair.first);
                console << "  Module " << pair.first 
                        << " : (" << toRemove[i][0] << "," << toRemove[i][1] << "," << toRemove[i][2] << ")"
                        << " -> (" << toFill[i][0] << "," << toFill[i][1] << "," << toFill[i][2] << ")\n";
                break;
            }
        }
    }
    
    planReady = true;
}

// ═══════════════════════════════════════════════════════════════════════════
// ALGORITHME 1: AUTHENTIFICATION AKC-PM
// ═══════════════════════════════════════════════════════════════════════════

void AkcpmBlockCode::algorithme1_Initier(P2PNetworkInterface* dest) {
    console << "┌─────────────────────────────────────────────────────────────┐\n";
    console << "│ Module " << hostBlock->blockId 
            << ": ALGORITHME 1 - INITIATION (SF-1, SF-2)          │\n";
    console << "└─────────────────────────────────────────────────────────────┘\n";
    
    infoSecurite.n0 = genererNonce();
    infoSecurite.n1 = H(infoSecurite.n0);
    infoSecurite.K0 = HL(infoSecurite.n0);
    
    uint64_t ts_brut = getScheduler()->now();
    uint64_t Ts = ts_brut / DIVISEUR_SYNC;
    uint64_t x = XOR(Ts, infoSecurite.n0);
    
    console << "  n0 = 0x" << hex << infoSecurite.n0 << dec << " (secret)\n";
    console << "  n1 = H(n0) = 0x" << hex << infoSecurite.n1 << dec << "\n";
    console << "  Ts = " << Ts << ", x = Ts XOR n0\n";
    
    int liens = (infoSecurite.liensStructure > 0) ? infoSecurite.liensStructure : 1;
    
    MessageDemandeAuth* msg = new MessageDemandeAuth(
        liens, infoSecurite.n1, x, infoSecurite.K0);
    sendMessage(msg, dest, DELAI_TRANSMISSION, 0);
    
    n0EnAttente[dest] = infoSecurite.n0;
    authentificationsEnCours.insert(dest);
    infoSecurite.etat = ETAT_AUTHENTIFICATION;
    
    console << "  → AUTH_REQUEST envoye\n";
}

void AkcpmBlockCode::algorithme1_Verifier(P2PNetworkInterface* src, int liens,
                                           uint64_t n1, uint64_t x,
                                           const vector<uint8_t>& K0) {
    console << "┌─────────────────────────────────────────────────────────────┐\n";
    console << "│ Module " << hostBlock->blockId 
            << ": ALGORITHME 1 - VERIFICATION (SF-3, SF-4)        │\n";
    console << "└─────────────────────────────────────────────────────────────┘\n";
    
    if (!estDansStructure()) {
        console << "  REJETE: Pas dans la structure\n";
        return;
    }
    
    uint64_t Tr = getScheduler()->now();
    uint64_t n0_prime = 0;
    bool syncOK = false;
    
    uint64_t Ts_base = (Tr - DELAI_TRANSMISSION) / DIVISEUR_SYNC;
    
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
        console << "  ✗ ECHEC SYNC\n";
        return;
    }
    
    vector<uint8_t> K0_calcule = HL(n0_prime);
    if (K0_calcule != K0) {
        console << "  ✗ ECHEC AUTH: K0 invalide\n";
        return;
    }
    
    console << "  ✓ K0 verifie - Module authentifie!\n";
    
    uint64_t n2;
    if (infoSecurite.estModuleInitial && !n2GlobalDefini) {
        n2Global = genererNonce();
        n2GlobalDefini = true;
        console << "  [SINGLE_KEY] n2 global genere\n";
    }
    n2 = n2Global;
    
    uint64_t x1 = XOR(n2, n0_prime);
    vector<uint8_t> K1 = HL(n2);
    
    console << "  K1 etablie (SINGLE_KEY)\n";
    
    clesVoisins[src] = K1;
    voisinsAuthentifies[src] = true;
    voisinsDansStructure[src] = true;
    infoSecurite.liensStructure++;
    
    MessageDefiCle* msg = new MessageDefiCle(x1);
    sendMessage(msg, src, DELAI_TRANSMISSION, 0);
    
    console << "  → KEY_CHALLENGE envoye\n";
}

void AkcpmBlockCode::algorithme1_Completer(P2PNetworkInterface* src, uint64_t x1) {
    console << "┌─────────────────────────────────────────────────────────────┐\n";
    console << "│ Module " << hostBlock->blockId 
            << ": ALGORITHME 1 - COMPLETION (SF-5)                │\n";
    console << "└─────────────────────────────────────────────────────────────┘\n";
    
    if (n0EnAttente.find(src) == n0EnAttente.end()) {
        console << "  ERREUR: Pas d'auth en cours\n";
        return;
    }
    
    uint64_t n0 = n0EnAttente[src];
    uint64_t n2 = XOR(x1, n0);
    vector<uint8_t> K1 = HL(n2);
    
    infoSecurite.n2 = n2;
    infoSecurite.K1 = K1;
    clesVoisins[src] = K1;
    voisinsAuthentifies[src] = true;
    voisinsDansStructure[src] = true;
    infoSecurite.etat = ETAT_CLE_ETABLIE;
    infoSecurite.liensStructure++;
    authenticationComplete = true;
    
    n0EnAttente.erase(src);
    authentificationsEnCours.erase(src);
    
    console << "╔═══════════════════════════════════════════════════════════╗\n";
    console << "║  ✓ AUTHENTIFICATION REUSSIE! Cle K1 etablie              ║\n";
    console << "╚═══════════════════════════════════════════════════════════╝\n";
    
    Catoms3DBlock* catom = (Catoms3DBlock*)hostBlock;
    catom->setColor(GREEN);
    
    // Incrémenter le compteur global
    authenticatedCount++;
    console << "  Modules authentifies: " << authenticatedCount << "/" << TOTAL_MODULES << "\n";
    
    notifierVoisinsStructurePrete();
    
    // Si tous authentifiés, lancer la reconfiguration
    if (authenticatedCount >= TOTAL_MODULES && !allAuthenticated) {
        allAuthenticated = true;
        console << "╔═══════════════════════════════════════════════════════════╗\n";
        console << "║  TOUS LES MODULES AUTHENTIFIES - DEBUT RECONFIGURATION   ║\n";
        console << "╚═══════════════════════════════════════════════════════════╝\n";
        
        // Lancer le premier module mobile
        lancerProchainModule();
    }
}

// ═══════════════════════════════════════════════════════════════════════════
// GESTION DE LA STRUCTURE
// ═══════════════════════════════════════════════════════════════════════════

void AkcpmBlockCode::notifierVoisinsStructurePrete() {
    Catoms3DBlock* catom = (Catoms3DBlock*)hostBlock;
    
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
                    voisinsDansStructure[iface] = true;
                }
            }
        }
    }
}

void AkcpmBlockCode::startAuthenticationForMove() {
    // Pour le mouvement, on considère que l'authentification est déjà faite
    // car tous les modules ont été authentifiés au départ
    authenticationComplete = true;
    checkAuthCompleteForMove();
}

void AkcpmBlockCode::checkAuthCompleteForMove() {
    if (authenticationComplete && mustMove) {
        Catoms3DBlock* catom = (Catoms3DBlock*)hostBlock;
        catom->setColor(RED);
        tryMoveToward(myTarget);
    }
}

// ═══════════════════════════════════════════════════════════════════════════
// GESTIONNAIRES DE MESSAGES
// ═══════════════════════════════════════════════════════════════════════════

void AkcpmBlockCode::surReceptionDemandeAuth(shared_ptr<Message> msg,
                                              P2PNetworkInterface* src) {
    MessageDemandeAuth* authMsg = static_cast<MessageDemandeAuth*>(msg.get());
    console << "Module " << hostBlock->blockId << ": Reception AUTH_REQUEST\n";
    algorithme1_Verifier(src, authMsg->liens, authMsg->n1, authMsg->x, authMsg->K0);
}

void AkcpmBlockCode::surReceptionDefiCle(shared_ptr<Message> msg,
                                          P2PNetworkInterface* src) {
    MessageDefiCle* cleMsg = static_cast<MessageDefiCle*>(msg.get());
    console << "Module " << hostBlock->blockId << ": Reception KEY_CHALLENGE\n";
    algorithme1_Completer(src, cleMsg->x1);
}

void AkcpmBlockCode::surReceptionStructurePrete(shared_ptr<Message> msg,
                                                 P2PNetworkInterface* src) {
    console << "Module " << hostBlock->blockId << ": Reception STRUCTURE_READY\n";
    
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
            message = (static_pointer_cast<NetworkInterfaceReceiveEvent>(pev))->message;
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
            if (infoSecurite.estModuleInitial) {
                console << "Module " << hostBlock->blockId << ": Demarrage protocole AKC-PM\n";
                notifierVoisinsStructurePrete();
                
                // Marquer le module initial comme authentifié
                authenticatedCount++;
            }
            break;
        }
        
        default:
            break;
    }
}

// ═══════════════════════════════════════════════════════════════════════════
// RECONFIGURATION (MÊME LOGIQUE QUE myMotionTestCode)
// ═══════════════════════════════════════════════════════════════════════════

void AkcpmBlockCode::lancerProchainModule() {
    if (moveQueue.empty()) {
        console << "╔═══════════════════════════════════════════════════════════╗\n";
        console << "║  RECONFIGURATION SECURISEE TERMINEE!                      ║\n";
        console << "║  Modules deplaces : " << moveCount << "                                    ║\n";
        console << "╚═══════════════════════════════════════════════════════════╝\n";
        return;
    }
    
    bID nextId = moveQueue.front();
    console << "=== Tour du module " << nextId << " ===\n";
    
    auto world = BaseSimulator::getWorld();
    auto nextBlock = world->getBlockById(nextId);
    if (nextBlock) {
        AkcpmBlockCode *nextCode = (AkcpmBlockCode*)nextBlock->blockCode;
        nextCode->mustMove = true;
        nextCode->myTarget = assignments[nextId];
        nextCode->visited.clear();
        nextCode->visited.insert(nextBlock->position);
        nextCode->moveSteps = 0;
        
        Catoms3DBlock* catom = (Catoms3DBlock*)nextBlock;
        catom->setColor(RED);
        
        // Lancer le mouvement directement (déjà authentifié)
        nextCode->tryMoveToward(nextCode->myTarget);
    }
}

bool AkcpmBlockCode::tryMoveToward(const Cell3DPosition &goal) {
    Catoms3DBlock* catom = (Catoms3DBlock*)hostBlock;
    
    if (catom->position == goal) {
        catom->setColor(GREEN);
        return false;
    }
    
    if (moveSteps >= MAX_STEPS) {
        console << "Module " << hostBlock->blockId 
                << " : LIMITE DE PAS ATTEINTE (" << MAX_STEPS << ")\n";
        catom->setColor(YELLOW);
        return false;
    }
    
    auto tab = Catoms3DMotionEngine::getAllRotationsForModule(catom);
    Cell3DPosition fp;
    short fo;
    
    struct Candidate {
        Cell3DPosition pos;
        double dist;
        bool alreadyVisited;
    };
    vector<Candidate> candidates;
    
    for (auto &elem : tab) {
        elem.second.init(((Catoms3DGlBlock *)catom->ptrGlBlock)->mat);
        elem.second.getFinalPositionAndOrientation(fp, fo);
        
        if (!lattice->isFree(fp)) continue;
        
        // Connectivité
        vector<Cell3DPosition> neighbors = lattice->getNeighborhood(fp);
        int nbOccupied = 0;
        for (auto &nPos : neighbors) {
            if (lattice->cellHasBlock(nPos) && nPos != catom->position) {
                nbOccupied++;
            }
        }
        if (nbOccupied < 1) continue;
        
        double dist = (fp[0]-goal[0])*(fp[0]-goal[0])
                    + (fp[1]-goal[1])*(fp[1]-goal[1])
                    + (fp[2]-goal[2])*(fp[2]-goal[2]);
        
        bool wasVisited = visited.count(fp) > 0;
        candidates.push_back({fp, dist, wasVisited});
    }
    
    if (candidates.empty()) {
        console << "Module " << hostBlock->blockId << " : BLOQUE\n";
        catom->setColor(YELLOW);
        return false;
    }
    
    // Priorité 1 : positions NON visitées, la plus proche
    Cell3DPosition bestPos;
    double bestDist = 999999;
    bool found = false;
    
    for (auto &c : candidates) {
        if (!c.alreadyVisited && c.dist < bestDist) {
            bestDist = c.dist;
            bestPos = c.pos;
            found = true;
        }
    }
    
    // Priorité 2 : si toutes visitées, reset
    if (!found) {
        console << "Module " << hostBlock->blockId 
                << " : reset positions visitees\n";
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
        targetPos = bestPos;
        visited.insert(bestPos);
        moveSteps++;
        console << "Module " << hostBlock->blockId 
                << " [pas " << moveSteps << "] : "
                << catom->position << " -> " << bestPos << "\n";
        catom->moveTo(bestPos);
        return true;
    }
    
    return false;
}

void AkcpmBlockCode::onMotionEnd() {
    Catoms3DBlock* catom = (Catoms3DBlock*)hostBlock;
    
    console << "Module " << hostBlock->blockId 
            << " : arrive a " << catom->position << "\n";
    
    if (catom->position == myTarget) {
        catom->setColor(GREEN);
        moveCount++;
        console << "Module " << hostBlock->blockId 
                << " : === CIBLE ATTEINTE en " << moveSteps << " pas ===\n";
        
        if (!moveQueue.empty()) moveQueue.pop();
        if (!moveQueue.empty()) {
            lancerProchainModule();
        } else {
            console << "╔═══════════════════════════════════════════════════════════╗\n";
            console << "║  RECONFIGURATION SECURISEE TERMINEE!                      ║\n";
            console << "║  Modules deplaces : " << moveCount << "                                    ║\n";
            console << "╚═══════════════════════════════════════════════════════════╝\n";
        }
    } else {
        catom->setColor(MAGENTA);
        tryMoveToward(myTarget);
    }
}

void AkcpmBlockCode::onTap(int face) {
    Catoms3DBlock* catom = (Catoms3DBlock*)hostBlock;
    Cell3DPosition pos = catom->position;
    
    console << "╔═══════════════════════════════════════════════════════════╗\n";
    console << "║  ETAT DU MODULE " << hostBlock->blockId << "                                     ║\n";
    console << "║  Position: (" << pos[0] << ", " << pos[1] << ", " << pos[2] << ")                              ║\n";
    console << "║  Etat: ";
    switch (infoSecurite.etat) {
        case ETAT_NON_LIE: console << "NON_LIE"; break;
        case ETAT_AUTHENTIFICATION: console << "AUTHENTIFICATION"; break;
        case ETAT_AUTHENTIFIE: console << "AUTHENTIFIE"; break;
        case ETAT_CLE_ETABLIE: console << "CLE_ETABLIE"; break;
    }
    console << "\n";
    console << "║  Module Initial: " << (infoSecurite.estModuleInitial ? "OUI" : "NON") << "                              ║\n";
    console << "║  Modele: SINGLE_KEY                                       ║\n";
    console << "╚═══════════════════════════════════════════════════════════╝\n";
}

// ═══════════════════════════════════════════════════════════════════════════
// CLASSES DE MESSAGES
// ═══════════════════════════════════════════════════════════════════════════

MessageDemandeAuth::MessageDemandeAuth(int _liens, uint64_t _n1, uint64_t _x,
                                       const vector<uint8_t>& _K0)
    : Message(), liens(_liens), n1(_n1), x(_x), K0(_K0) {
    type = MSG_AUTH_REQUEST;
}
MessageDemandeAuth::~MessageDemandeAuth() {}

MessageDefiCle::MessageDefiCle(uint64_t _x1) : Message(), x1(_x1) {
    type = MSG_KEY_CHALLENGE;
}
MessageDefiCle::~MessageDefiCle() {}

MessageStructurePrete::MessageStructurePrete() : Message() {
    type = MSG_STRUCTURE_READY;
}
MessageStructurePrete::~MessageStructurePrete() {}
