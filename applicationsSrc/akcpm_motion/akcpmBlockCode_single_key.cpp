/**
 * PROTOCOLE AKC-PM - MODÈLE SINGLE_KEY AVEC MOUVEMENTS
 */

#include <iostream>
#include <random>
#include <climits>
#include "akcpmBlockCode_single_key.h"

using namespace std;
using namespace Catoms3D;

// Variables statiques
uint64_t AkcpmBlockCode::n2Global = 0;
bool AkcpmBlockCode::n2GlobalDefini = false;

// Plan de mouvements (comme 'order' dans l'exemple)
std::vector<ModuleMovement> AkcpmBlockCode::planMouvements = {
    // Basé sur config.xml: positions (1,3,2), (3,3,2), (2,3,2), (2,2,3)
    ModuleMovement(Cell3DPosition(1,3,2), Cell3DPosition(1,2,3), 0),
    ModuleMovement(Cell3DPosition(3,3,2), Cell3DPosition(2,2,2), 1),
    ModuleMovement(Cell3DPosition(1,2,3), Cell3DPosition(1,2,2), 2),
    ModuleMovement(Cell3DPosition(2,3,2), Cell3DPosition(3,2,2), 3)
};

const int AkcpmBlockCode::NB_LIGNES_CODE;
const int AkcpmBlockCode::DIVISEUR_SYNC;
const int AkcpmBlockCode::TOLERANCE_SYNC;
const int AkcpmBlockCode::DELAI_TRANSMISSION;

// ═══════════════════════════════════════════════════════════════════════════
// CONSTRUCTEUR (comme l'exemple!)
// ═══════════════════════════════════════════════════════════════════════════

AkcpmBlockCode::AkcpmBlockCode(Catoms3DBlock *host) 
    : Catoms3DBlockCode(host), module(host), myCurrent(planMouvements) {
    
    if (not host) return;
    
    addMessageEventFunc2(MSG_MOTION_UPDATE,
        std::bind(&AkcpmBlockCode::myMotionUpdateFunc, this,
                  std::placeholders::_1, std::placeholders::_2));
    
    // Calculer profondeur max
    myDepth = 0;
    for (const auto& mv : myCurrent) {
        if (mv.stage > myDepth) myDepth = mv.stage;
    }
    
    lattice = nullptr;
}

// ═══════════════════════════════════════════════════════════════════════════
// FONCTIONS CRYPTO
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
    std::vector<uint8_t> ligne(32);
    uint64_t seed = numeroLigne * 12345;
    for (int i = 0; i < 32; i++) {
        seed = (seed * 1103515245 + 12345) & 0x7FFFFFFF;
        ligne[i] = seed & 0xFF;
    }
    return ligne;
}

std::vector<uint8_t> AkcpmBlockCode::HL(uint64_t n) {
    int numeroLigne = n % NB_LIGNES_CODE;
    std::vector<uint8_t> ligne = L(numeroLigne);
    uint64_t hashVal = H(ligne);
    std::vector<uint8_t> resultat(8);
    for (int i = 0; i < 8; i++) {
        resultat[i] = (hashVal >> (i * 8)) & 0xFF;
    }
    return resultat;
}

uint64_t AkcpmBlockCode::XOR(uint64_t a, uint64_t b) { return a ^ b; }

uint64_t AkcpmBlockCode::genererNonce() {
    std::random_device rd;
    std::mt19937_64 gen(rd());
    std::uniform_int_distribution<uint64_t> dis;
    return dis(gen);
}

// ═══════════════════════════════════════════════════════════════════════════
// STARTUP
// ═══════════════════════════════════════════════════════════════════════════

void AkcpmBlockCode::startup() {
    cout << "Module " << module->blockId << " startup at " << module->position << endl;
    
    lattice = (FCCLattice*)(Catoms3D::getWorld()->lattice);
    myStage = 0;
    
    // Module ID=1 est le module initial (comme iM dans AKC-PM)
    if (module->blockId == 1) {
        definirCommeModuleInitial();
        // Démarrer le protocole après un délai
        getScheduler()->schedule(
            new InterruptionEvent<unsigned int>(getScheduler()->now() + 1000, module, 0));
    } else {
        module->setColor(GREY);
    }
}

// ═══════════════════════════════════════════════════════════════════════════
// MOUVEMENT - EXACTEMENT COMME L'EXEMPLE!
// ═══════════════════════════════════════════════════════════════════════════

bool AkcpmBlockCode::tryToMove(int index) {
    vector<std::pair<const Catoms3DMotionRulesLink*, Catoms3DRotation>> tab = 
        Catoms3DMotionEngine::getAllRotationsForModule(module);
    
    Cell3DPosition finalPos;
    short finalOrient;
    
    cout << "Module #" << module->position << ": cherche -> " << myCurrent[index].posTo << endl;
    
    for (auto &elem : tab) {
        elem.second.init(((Catoms3DGlBlock*)module->ptrGlBlock)->mat);
        elem.second.getFinalPositionAndOrientation(finalPos, finalOrient);
        
        cout << "  Test: " << finalPos << " =?= " << myCurrent[index].posTo << endl;
        
        if (finalPos == myCurrent[index].posTo) {
            scheduler->schedule(
                new Catoms3DRotationStartEvent(getScheduler()->now() + 100000, 
                                                module, 
                                                elem.second.pivot, 
                                                finalPos));
            cout << "Module #" << getId() << ": " << module->position << " -> " << finalPos << endl;
            return true;
        }
    }
    return false;
}

void AkcpmBlockCode::onMotionEnd() {
    myStage += 1;
    cout << "onMotionEnd: " << myStage << "/" << myDepth << endl;
    
    if (myStage > myDepth) {
        cout << "Reconfiguration terminée!" << endl;
        module->setColor(GREEN);
        return;
    }
    
    // Chercher mouvement pour cette étape
    size_t i = 0;
    while (i < myCurrent.size() && !(myCurrent[i].stage == myStage && myCurrent[i].posFrom == module->position)) {
        i++;
    }
    
    cout << "search stage=" << myStage << " id=" << getId() << ": ";
    if (i < myCurrent.size() && myCurrent[i].stage == myStage && myCurrent[i].posFrom == module->position) {
        cout << "found!" << endl;
        tryToMove(i);
    } else {
        cout << "not found!" << endl;
    }
    
    setColor(myStage);
    sendMessageToAllNeighbors("motionUpdate", 
        new MessageOf<pair<int,int>>(MSG_MOTION_UPDATE, {myStage, 0}), 15000, 0, 0);
}

void AkcpmBlockCode::myMotionUpdateFunc(std::shared_ptr<Message> _msg, P2PNetworkInterface* sender) {
    MessageOf<pair<int,int>>* msg = static_cast<MessageOf<pair<int,int>>*>(_msg.get());
    int msgStage = msg->getData()->first;
    
    console << "rec. motionUpdate(" << msgStage << "/" << myStage << ")\n";
    
    if (msgStage != myStage) {
        myStage = msgStage;
        setColor(myStage);
        
        size_t i = 0;
        while (i < myCurrent.size() && !(myCurrent[i].stage == myStage && myCurrent[i].posFrom == module->position)) {
            i++;
        }
        
        cout << "msg search stage=" << myStage << " id=" << getId() << ": ";
        if (i < myCurrent.size() && myCurrent[i].stage == myStage && myCurrent[i].posFrom == module->position) {
            cout << "found!" << endl;
            tryToMove(i);
        } else {
            cout << "not found!" << endl;
        }
        
        sendMessageToAllNeighbors("motionUpdate", 
            new MessageOf<pair<int,int>>(MSG_MOTION_UPDATE, {myStage, 0}), 15000, 0, 0);
    }
}

// ═══════════════════════════════════════════════════════════════════════════
// ALGORITHME 1 - AUTHENTIFICATION
// ═══════════════════════════════════════════════════════════════════════════

void AkcpmBlockCode::algorithme1_Initier(P2PNetworkInterface* dest) {
    console << "Module " << module->blockId << ": INITIER AUTH\n";
    
    infoSecurite.n0 = genererNonce();
    infoSecurite.n1 = H(infoSecurite.n0);
    infoSecurite.K0 = HL(infoSecurite.n0);
    
    uint64_t Ts = getScheduler()->now() / DIVISEUR_SYNC;
    uint64_t x = XOR(Ts, infoSecurite.n0);
    
    int liens = (infoSecurite.liensStructure > 0) ? infoSecurite.liensStructure : 1;
    
    MessageDemandeAuth* msg = new MessageDemandeAuth(liens, infoSecurite.n1, x, infoSecurite.K0);
    sendMessage(msg, dest, DELAI_TRANSMISSION, 0);
    
    n0EnAttente[dest] = infoSecurite.n0;
    authentificationsEnCours.insert(dest);
    infoSecurite.etat = ETAT_AUTHENTIFICATION;
}

void AkcpmBlockCode::algorithme1_Verifier(P2PNetworkInterface* src, int liens, 
                                           uint64_t n1, uint64_t x, 
                                           const std::vector<uint8_t>& K0) {
    console << "Module " << module->blockId << ": VERIFIER AUTH\n";
    
    if (!estDansStructure()) return;
    
    uint64_t Tr = getScheduler()->now();
    uint64_t n0_prime = 0;
    bool syncOK = false;
    uint64_t Ts_base = (Tr - DELAI_TRANSMISSION) / DIVISEUR_SYNC;
    
    for (int offset = -TOLERANCE_SYNC; offset <= TOLERANCE_SYNC && !syncOK; offset++) {
        uint64_t Ts_test = Ts_base + offset;
        uint64_t n0_test = XOR(x, Ts_test);
        if (H(n0_test) == n1) {
            n0_prime = n0_test;
            syncOK = true;
        }
    }
    
    if (!syncOK || HL(n0_prime) != K0) {
        console << "  ECHEC AUTH\n";
        return;
    }
    
    console << "  AUTH OK!\n";
    
    if (infoSecurite.estModuleInitial && !n2GlobalDefini) {
        n2Global = genererNonce();
        n2GlobalDefini = true;
    }
    
    uint64_t x1 = XOR(n2Global, n0_prime);
    clesVoisins[src] = HL(n2Global);
    voisinsAuthentifies[src] = true;
    voisinsDansStructure[src] = true;
    infoSecurite.liensStructure++;
    
    sendMessage(new MessageDefiCle(x1), src, DELAI_TRANSMISSION, 0);
}

void AkcpmBlockCode::algorithme1_Completer(P2PNetworkInterface* src, uint64_t x1) {
    console << "Module " << module->blockId << ": COMPLETER AUTH\n";
    
    if (n0EnAttente.find(src) == n0EnAttente.end()) return;
    
    uint64_t n0 = n0EnAttente[src];
    uint64_t n2 = XOR(x1, n0);
    
    infoSecurite.n2 = n2;
    infoSecurite.K1 = HL(n2);
    clesVoisins[src] = infoSecurite.K1;
    voisinsAuthentifies[src] = true;
    voisinsDansStructure[src] = true;
    infoSecurite.etat = ETAT_CLE_ETABLIE;
    infoSecurite.liensStructure++;
    
    n0EnAttente.erase(src);
    authentificationsEnCours.erase(src);
    
    console << "  ★ CLE K1 ETABLIE ★\n";
    module->setColor(GREEN);
    
    notifierVoisinsStructurePrete();
    
    // LANCER MOUVEMENT SI MOBILE
    if (isMobile) {
        cout << "Module " << module->blockId << " mobile, cherche mouvement étape " << myStage << endl;
        
        size_t i = 0;
        while (i < myCurrent.size() && !(myCurrent[i].stage == myStage && myCurrent[i].posFrom == module->position)) {
            i++;
        }
        
        if (i < myCurrent.size() && myCurrent[i].stage == myStage && myCurrent[i].posFrom == module->position) {
            cout << "  -> Trouvé! Lancement mouvement..." << endl;
            tryToMove(i);
        }
    }
}

// ═══════════════════════════════════════════════════════════════════════════
// GESTION STRUCTURE
// ═══════════════════════════════════════════════════════════════════════════

void AkcpmBlockCode::notifierVoisinsStructurePrete() {
    for (int i = 0; i < 12; i++) {
        P2PNetworkInterface* iface = module->getInterface(i);
        if (iface && iface->connectedInterface) {
            if (voisinsAuthentifies.find(iface) == voisinsAuthentifies.end()) {
                sendMessage(new MessageStructurePrete(), iface, DELAI_TRANSMISSION, 0);
            }
        }
    }
}

void AkcpmBlockCode::verifierNouveauxVoisins() {
    for (int i = 0; i < 12; i++) {
        P2PNetworkInterface* iface = module->getInterface(i);
        if (iface && iface->connectedInterface) {
            if (voisinsDansStructure.find(iface) == voisinsDansStructure.end() &&
                voisinsAuthentifies.find(iface) == voisinsAuthentifies.end() &&
                authentificationsEnCours.find(iface) == authentificationsEnCours.end()) {
                if (estDansStructure()) {
                    sendMessage(new MessageStructurePrete(), iface, DELAI_TRANSMISSION, 0);
                }
            }
        }
    }
}

int AkcpmBlockCode::compterVoisinsConnectes() {
    int count = 0;
    for (int i = 0; i < 12; i++) {
        P2PNetworkInterface* iface = module->getInterface(i);
        if (iface && iface->connectedInterface) count++;
    }
    return count;
}

bool AkcpmBlockCode::estDansStructure() const {
    return infoSecurite.estModuleInitial || infoSecurite.etat == ETAT_CLE_ETABLIE;
}

void AkcpmBlockCode::definirCommeModuleInitial() {
    infoSecurite.estModuleInitial = true;
    infoSecurite.etat = ETAT_CLE_ETABLIE;
    module->setColor(BLUE);
    console << "Module " << module->blockId << " = MODULE INITIAL (iM)\n";
}

// ═══════════════════════════════════════════════════════════════════════════
// GESTIONNAIRES MESSAGES
// ═══════════════════════════════════════════════════════════════════════════

void AkcpmBlockCode::surReceptionDemandeAuth(std::shared_ptr<Message> msg, P2PNetworkInterface* src) {
    MessageDemandeAuth* m = static_cast<MessageDemandeAuth*>(msg.get());
    algorithme1_Verifier(src, m->liens, m->n1, m->x, m->K0);
}

void AkcpmBlockCode::surReceptionDefiCle(std::shared_ptr<Message> msg, P2PNetworkInterface* src) {
    MessageDefiCle* m = static_cast<MessageDefiCle*>(msg.get());
    algorithme1_Completer(src, m->x1);
}

void AkcpmBlockCode::surReceptionStructurePrete(std::shared_ptr<Message> msg, P2PNetworkInterface* src) {
    voisinsDansStructure[src] = true;
    if (!estDansStructure()) {
        if (authentificationsEnCours.find(src) == authentificationsEnCours.end() &&
            voisinsAuthentifies.find(src) == voisinsAuthentifies.end()) {
            algorithme1_Initier(src);
        }
    }
}

// ═══════════════════════════════════════════════════════════════════════════
// PROCESSLOCALEVENT
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
                case MSG_MOTION_UPDATE:
                    myMotionUpdateFunc(message, src);
                    break;
            }
            break;
        }
        
        case EVENT_ADD_NEIGHBOR:
            verifierNouveauxVoisins();
            break;
        
        case EVENT_INTERRUPTION:
            if (infoSecurite.estModuleInitial) {
                console << "Démarrage protocole AKC-PM\n";
                notifierVoisinsStructurePrete();
            }
            break;
        
        default:
            break;
    }
}

// ═══════════════════════════════════════════════════════════════════════════
// AUTRES
// ═══════════════════════════════════════════════════════════════════════════

void AkcpmBlockCode::parseUserBlockElements(TiXmlElement *config) {
    const char* attr = config->Attribute("mobile");
    if (attr != nullptr) {
        string str(attr);
        if (str == "true" || str == "1" || str == "yes") {
            isMobile = true;
            cout << module->blockId << " is mobile!" << endl;
        }
    }
}

string AkcpmBlockCode::onInterfaceDraw() {
    string str = "Module #" + to_string(module->blockId);
    str += "\nAuth: " + string(estDansStructure() ? "OK" : "...");
    str += "\nMobile: " + string(isMobile ? "Oui" : "Non");
    str += "\nEtape: " + to_string(myStage) + "/" + to_string(myDepth);
    return str;
}

// ═══════════════════════════════════════════════════════════════════════════
// MESSAGES
// ═══════════════════════════════════════════════════════════════════════════

MessageDemandeAuth::MessageDemandeAuth(int _liens, uint64_t _n1, uint64_t _x,
                                       const std::vector<uint8_t>& _K0)
    : Message(), liens(_liens), n1(_n1), x(_x), K0(_K0) { type = MSG_AUTH_REQUEST; }
MessageDemandeAuth::~MessageDemandeAuth() {}

MessageDefiCle::MessageDefiCle(uint64_t _x1) : Message(), x1(_x1) { type = MSG_KEY_CHALLENGE; }
MessageDefiCle::~MessageDefiCle() {}

MessageStructurePrete::MessageStructurePrete() : Message() { type = MSG_STRUCTURE_READY; }
MessageStructurePrete::~MessageStructurePrete() {}
