#include "MonApplicationCode.hpp"
#include <algorithm>
#include <random>

using namespace std;

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

// === CONTROLE DES PHASES ===
static int authenticatedCount = 0;
static set<bID> authenticatedModules;  // Pour éviter les doubles comptages
static const int TOTAL_MODULES = 5;
static bool reconfigStarted = false;

static bool isInShape(const Cell3DPosition &pos, const vector<Cell3DPosition> &shape) {
    return find(shape.begin(), shape.end(), pos) != shape.end();
}

// === CONSTRUCTEUR / DESTRUCTEUR ===

MonApplicationCode::MonApplicationCode(Catoms3DBlock *host) 
    : Catoms3DBlockCode(host), module(host) {}

MonApplicationCode::~MonApplicationCode() {
    clesVoisins.clear();
    voisinsAuthentifies.clear();
    voisinsDansStructure.clear();
    authentificationsEnCours.clear();
    n0EnAttente.clear();
}

// === FONCTIONS CRYPTOGRAPHIQUES AKC-PM ===

uint64_t MonApplicationCode::H(uint64_t valeur) {
    uint64_t hash = 5381;
    for (int i = 0; i < 8; i++) {
        hash = ((hash << 5) + hash) + ((valeur >> (i * 8)) & 0xFF);
    }
    return hash;
}

uint64_t MonApplicationCode::H(const vector<uint8_t>& donnees) {
    uint64_t hash = 5381;
    for (auto octet : donnees) {
        hash = ((hash << 5) + hash) + octet;
    }
    return hash;
}

vector<uint8_t> MonApplicationCode::L(int numeroLigne) {
    vector<uint8_t> ligne(TAILLE_LIGNE_BITS / 8);
    uint64_t graine = 0xDEADBEEF ^ (numeroLigne * 0x12345678);
    for (size_t i = 0; i < ligne.size(); i++) {
        graine = graine * 1103515245 + 12345;
        ligne[i] = (graine >> 16) & 0xFF;
    }
    return ligne;
}

vector<uint8_t> MonApplicationCode::HL(uint64_t n) {
    int numeroLigne = n % NB_LIGNES_CODE;
    vector<uint8_t> ligne = L(numeroLigne);
    uint64_t hash = H(ligne);
    vector<uint8_t> resultat(8);
    for (int i = 0; i < 8; i++) {
        resultat[i] = (hash >> (i * 8)) & 0xFF;
    }
    return resultat;
}

uint64_t MonApplicationCode::XOR(uint64_t a, uint64_t b) {
    return a ^ b;
}

uint64_t MonApplicationCode::genererNonce() {
    static random_device rd;
    static mt19937_64 gen(rd());
    uniform_int_distribution<uint64_t> dis;
    return dis(gen);
}

// === FONCTIONS UTILITAIRES AKC-PM ===

bool MonApplicationCode::estDansStructure() const {
    return infoSecurite.etat == ETAT_AUTHENTIFIE ||
           infoSecurite.etat == ETAT_CLE_ETABLIE ||
           infoSecurite.estModuleInitial;
}

void MonApplicationCode::definirCommeModuleInitial() {
    infoSecurite.estModuleInitial = true;
    infoSecurite.etat = ETAT_AUTHENTIFIE;
    infoSecurite.liensStructure = 0;
    
    module->setColor(BLUE);
    
    console << "  MODULE " << module->blockId << " DEFINI COMME INITIAL (iM) \n";
    console << "  Modele: SINGLE_KEY (cle unique pour tout le reseau)\n";
}

// === STARTUP ===

void MonApplicationCode::startup() {
    // Planification (Module 1)
    if (!planReady && module->blockId == 1) {
        vector<Cell3DPosition> toRemove, toFill;
        
        for (auto &pos : initialShape)
            if (!isInShape(pos, targetShape)) toRemove.push_back(pos);
        for (auto &pos : targetShape)
            if (!isInShape(pos, initialShape)) toFill.push_back(pos);

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
    if (module->position == Cell3DPosition(2,2,2)) {
        definirCommeModuleInitial();
        getScheduler()->schedule(
            new InterruptionEvent<unsigned int>(getScheduler()->now(), module, 0));
        
        if (authenticatedModules.find(module->blockId) == authenticatedModules.end()) {
            authenticatedModules.insert(module->blockId);
            authenticatedCount++;
        }
    } else {
        module->setColor(GREY);
    }

    // Configuration modules mobiles
    if (planReady && assignments.count(module->blockId)) {
        myTarget = assignments[module->blockId];
    }
}

// === ALGORITHME 1 AKC-PM ===

void MonApplicationCode::algorithme1_Initier(P2PNetworkInterface* dest) {
    console << "Module " << module->blockId << ": ALGORITHME 1 - INITIATION (SF-1, SF-2)\n";
    
    if (voisinsAuthentifies.find(dest) != voisinsAuthentifies.end()) {
        console << "  REJETE: Ce voisin est deja authentifie\n";
        return;
    }
    
    if (authentificationsEnCours.find(dest) != authentificationsEnCours.end()) {
        console << "  REJETE: Authentification deja en cours avec ce voisin\n";
        return;
    }
    
    infoSecurite.n0 = genererNonce();
    infoSecurite.n1 = H(infoSecurite.n0);
    infoSecurite.K0 = HL(infoSecurite.n0);
    
    uint64_t ts_brut = getScheduler()->now();
    uint64_t Ts = ts_brut / DIVISEUR_SYNC;
    uint64_t x = XOR(Ts, infoSecurite.n0);
    
    console << "  n0 = " << infoSecurite.n0 << " (secret)\n";
    console << "  n1 = H(n0) = " << infoSecurite.n1 << "\n";
    console << "  ts = " << ts_brut << ", Ts = ts/10 = " << Ts << "\n";
    console << "  x = Ts XOR n0 = " << x << "\n";
    
    int liens = (infoSecurite.liensStructure > 0) ? infoSecurite.liensStructure : 1;
    
    MessageDemandeAuth* msg = new MessageDemandeAuth(liens, infoSecurite.n1, x, infoSecurite.K0);
    sendMessage(msg, dest, DELAI_TRANSMISSION, 0);
    
    n0EnAttente[dest] = infoSecurite.n0;
    authentificationsEnCours.insert(dest);
    infoSecurite.etat = ETAT_AUTHENTIFICATION;
    
    console << "  → Message AUTH_REQUEST envoye (liens=" << liens << ")\n";
}

void MonApplicationCode::algorithme1_Verifier(P2PNetworkInterface* src, int liens, 
                                               uint64_t n1, uint64_t x, 
                                               const vector<uint8_t>& K0) {
    console << "Module " << module->blockId << ": ALGORITHME 1 - VERIFICATION (SF-3, SF-4)\n";
    
    if (voisinsAuthentifies.find(src) != voisinsAuthentifies.end()) {
        console << "  REJETE: Ce voisin est deja authentifie\n";
        return;
    }
    
    if (!estDansStructure()) {
        console << "  REJETE: Pas dans la structure\n";
        return;
    }
    
    uint64_t Tr = getScheduler()->now();
    uint64_t n0_prime = 0;
    bool syncOK = false;
    uint64_t Ts_base = (Tr - DELAI_TRANSMISSION) / DIVISEUR_SYNC;
    
    console << "  Tr = " << Tr << ", Δt = " << DELAI_TRANSMISSION << "\n";
    console << "  Ts_base = (Tr - Δt)/10 = " << Ts_base << "\n";
    
    for (int offset = -TOLERANCE_SYNC; offset <= TOLERANCE_SYNC && !syncOK; offset++) {
        uint64_t Ts_test = Ts_base + offset;
        uint64_t n0_test = XOR(x, Ts_test);
        uint64_t n1_calcule = H(n0_test);
        
        if (n1_calcule == n1) {
            n0_prime = n0_test;
            syncOK = true;
            console << "   Sync OK (offset=" << offset << ", Ts=" << Ts_test << ")\n";
        }
    }
    
    if (!syncOK) {
        console << "   ECHEC SYNC: Impossible de verifier n1 = H(n0')\n";
        return;
    }
    
    console << "  n0' (recupere) = " << n0_prime << "\n";
    
    vector<uint8_t> K0_calcule = HL(n0_prime);
    if (K0_calcule != K0) {
        console << "   ECHEC AUTH: K0 ne correspond pas\n";
        return;
    }
    
    console << "   K0 verifie - Module authentifie!\n";
    
    uint64_t n2;
    if (infoSecurite.n2 != 0) {
        n2 = infoSecurite.n2;
        console << "  [SINGLE KEY] Utilisation du n2 local = " << n2 << "\n";
    } else if (infoSecurite.estModuleInitial) {
        n2 = genererNonce();
        infoSecurite.n2 = n2;
        console << "  n2 genere par le module initial = " << n2 << "\n";
    } else {
        console << "  ERREUR: Module non-initial sans n2!\n";
        return;
    }
    
    uint64_t x1 = XOR(n2, n0_prime);
    vector<uint8_t> K1 = HL(n2);
    
    console << "  n2 = " << n2 << "\n";
    console << "  x1 = n2 XOR n0' = " << x1 << "\n";
    
    clesVoisins[src] = K1;
    voisinsAuthentifies[src] = true;
    voisinsDansStructure[src] = true;
    infoSecurite.liensStructure++;
    
    MessageDefiCle* msg = new MessageDefiCle(x1);
    sendMessage(msg, src, DELAI_TRANSMISSION, 0);
    
    console << "  → Message KEY_CHALLENGE envoye\n";
}

void MonApplicationCode::algorithme1_Completer(P2PNetworkInterface* src, uint64_t x1) {
    console << "Module " << module->blockId << ": ALGORITHME 1 - COMPLETION (SF-5)\n";
    
    if (voisinsAuthentifies.find(src) != voisinsAuthentifies.end()) {
        console << "  REJETE: Ce voisin est deja authentifie\n";
        return;
    }
    
    if (n0EnAttente.find(src) == n0EnAttente.end()) {
        console << "  ERREUR: Pas d'authentification en cours\n";
        return;
    }
    
    uint64_t n0 = n0EnAttente[src];
    uint64_t n2 = XOR(x1, n0);
    vector<uint8_t> K1 = HL(n2);
    
    console << "  n2 = x1 XOR n0 = " << n2 << "\n";
    console << "  K1 = H(L(n2 mod Nb)) generee\n";
    
    infoSecurite.n2 = n2;
    infoSecurite.K1 = K1;
    clesVoisins[src] = K1;
    voisinsAuthentifies[src] = true;
    voisinsDansStructure[src] = true;
    infoSecurite.etat = ETAT_CLE_ETABLIE;
    infoSecurite.liensStructure++;
    
    n0EnAttente.erase(src);
    authentificationsEnCours.erase(src);
    
    console << "   AUTHENTIFICATION REUSSIE \n";
    console << "  Cle K1 partagee etablie (SINGLE_KEY) \n";
    
    module->setColor(GREEN);
    
    if (authenticatedModules.find(module->blockId) == authenticatedModules.end()) {
        authenticatedModules.insert(module->blockId);
        authenticatedCount++;
        console << "  Modules authentifies: " << authenticatedCount << "/" << TOTAL_MODULES << "\n";
    } else {
        console << "  Module deja compte (total: " << authenticatedCount << "/" << TOTAL_MODULES << ")\n";
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
                MonApplicationCode *nextCode = (MonApplicationCode*)nextBlock->blockCode;
                nextCode->visited.clear();
                nextCode->visited.insert(nextCode->module->position);
                nextCode->moveSteps = 0;
                nextCode->tryMoveToward(nextCode->myTarget);
            }
        }
    }
}

// === GESTION STRUCTURE AKC-PM ===

void MonApplicationCode::notifierVoisinsStructurePrete() {
    console << "Module " << module->blockId << ": Diffusion STRUCTURE_READY\n";
    
    for (int i = 0; i < 12; i++) {
        P2PNetworkInterface* iface = module->getInterface(i);
        if (iface && iface->connectedInterface) {
            if (voisinsAuthentifies.find(iface) == voisinsAuthentifies.end()) {
                MessageStructurePrete* msg = new MessageStructurePrete();
                sendMessage(msg, iface, DELAI_TRANSMISSION, 0);
            }
        }
    }
}

void MonApplicationCode::verifierNouveauxVoisins() {
    for (int i = 0; i < 12; i++) {
        P2PNetworkInterface* iface = module->getInterface(i);
        if (iface && iface->connectedInterface) {
            if (voisinsDansStructure.find(iface) == voisinsDansStructure.end() &&
                voisinsAuthentifies.find(iface) == voisinsAuthentifies.end() &&
                authentificationsEnCours.find(iface) == authentificationsEnCours.end()) {
                if (estDansStructure()) {
                    console << "Module " << module->blockId << ": Nouveau voisin detecte\n";
                    
                    // Vérifier si le voisin est aussi dans la structure
                    // Pour simplifier, on utilise toujours STRUCTURE_READY d'abord
                    MessageStructurePrete* msg = new MessageStructurePrete();
                    sendMessage(msg, iface, DELAI_TRANSMISSION, 0);
                }
            }
        }
    }
}

// === GESTIONNAIRES DE MESSAGES ===

void MonApplicationCode::surReceptionDemandeAuth(shared_ptr<Message> msg, P2PNetworkInterface* src) {
    MessageDemandeAuth* authMsg = static_cast<MessageDemandeAuth*>(msg.get());
    console << "Module " << module->blockId << ": Reception AUTH_REQUEST\n";
    algorithme1_Verifier(src, authMsg->liens, authMsg->n1, authMsg->x, authMsg->K0);
}

void MonApplicationCode::surReceptionDefiCle(shared_ptr<Message> msg, P2PNetworkInterface* src) {
    MessageDefiCle* cleMsg = static_cast<MessageDefiCle*>(msg.get());
    console << "Module " << module->blockId << ": Reception KEY_CHALLENGE\n";
    algorithme1_Completer(src, cleMsg->x1);
}

void MonApplicationCode::surReceptionStructurePrete(shared_ptr<Message> msg, P2PNetworkInterface* src) {
    console << "Module " << module->blockId << ": Reception STRUCTURE_READY\n";
    
    voisinsDansStructure[src] = true;
    
    if (voisinsAuthentifies.find(src) != voisinsAuthentifies.end()) {
        console << "  -> Deja authentifie avec ce voisin\n";
        return;
    }
    
    if (authentificationsEnCours.find(src) != authentificationsEnCours.end()) {
        console << "  -> Authentification deja en cours avec ce voisin\n";
        return;
    }
    
    // Toujours utiliser Algorithme 1 pour la stabilité
    // L'Algorithme 2 nécessite des conditions plus strictes
    if (!estDansStructure()) {
        console << "  -> Initiation de l'authentification\n";
        algorithme1_Initier(src);
    } else {
        console << "  -> Deja dans la structure, pas d'authentification necessaire\n";
    }
}

// === TRAITEMENT DES EVENEMENTS ===

void MonApplicationCode::processLocalEvent(EventPtr pev) {
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
            unsigned int interruptId = (std::static_pointer_cast<InterruptionEvent<unsigned int>>(pev))->data;
            
            if (interruptId == 0) {
                if (infoSecurite.estModuleInitial) {
                    notifierVoisinsStructurePrete();
                }
            } else if (interruptId == 2) {
                if (isMoving) {
                    if (module->position != lastPosition) {
                        isMoving = false;
                        
                        if (module->position == myTarget) {
                            if (!moveQueue.empty()) moveQueue.pop();
                            
                            if (!moveQueue.empty()) {
                                bID nextId = moveQueue.front();
                                auto nextBlock = BaseSimulator::getWorld()->getBlockById(nextId);
                                if (nextBlock) {
                                    MonApplicationCode *nextCode = (MonApplicationCode*)nextBlock->blockCode;
                                    nextCode->visited.clear();
                                    nextCode->visited.insert(nextCode->module->position);
                                    nextCode->moveSteps = 0;
                                    nextCode->tryMoveToward(nextCode->myTarget);
                                }
                            }
                        } else {
                            tryMoveToward(myTarget);
                        }
                    } else {
                        getScheduler()->schedule(
                            new InterruptionEvent<unsigned int>(getScheduler()->now() + 10000, module, 2));
                    }
                }
            }
            break;
        }
        default:
            break;
    }
}

// === RECONFIGURATION ===

bool MonApplicationCode::tryMoveToward(const Cell3DPosition &goal) {
    if (module->position == goal) return false;

    auto tab = Catoms3DMotionEngine::getAllRotationsForModule(module);
    Cell3DPosition fp;
    short fo;

    struct Candidate { Cell3DPosition pos; double dist; bool visited; };
    vector<Candidate> candidates;

    for (auto &elem : tab) {
        elem.second.init(((Catoms3DGlBlock *)module->ptrGlBlock)->mat);
        elem.second.getFinalPositionAndOrientation(fp, fo);

        if (!lattice->isFree(fp)) continue;

        int nbOccupied = 0;
        for (auto &nPos : lattice->getNeighborhood(fp))
            if (lattice->cellHasBlock(nPos) && nPos != module->position) nbOccupied++;
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
        visited.insert(module->position);
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
        lastPosition = module->position;
        
        Catoms3DBlock* catom = (Catoms3DBlock*)module;
        catom->moveTo(bestPos);
        
        getScheduler()->schedule(
            new InterruptionEvent<unsigned int>(getScheduler()->now() + 50000, module, 2));
        
        return true;
    }
    return false;
}

void MonApplicationCode::onMotionEnd() {}

void MonApplicationCode::parseUserBlockElements(TiXmlElement *config) {}

// === CLASSES DE MESSAGES ===

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

