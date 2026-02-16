/**
 * PROTOCOLE AKC-PM - MODÈLE SINGLE_KEY AVEC MOUVEMENTS
 */

#ifndef AKCPMBLOCKCODE_SINGLE_KEY_H_
#define AKCPMBLOCKCODE_SINGLE_KEY_H_

#include <algorithm>
#include "robots/catoms3D/catoms3DSimulator.h"
#include "robots/catoms3D/catoms3DBlockCode.h"
#include "robots/catoms3D/catoms3DMotionEngine.h"
#include "motion/teleportationEvents.h"
#include <vector>
#include <map>
#include <set>

using namespace Catoms3D;

static const int MSG_AUTH_REQUEST = 2001;
static const int MSG_KEY_CHALLENGE = 2002;
static const int MSG_STRUCTURE_READY = 2003;
static const int MSG_MOTION_UPDATE = 2004;

// Structure pour mouvements (comme ModuleData dans l'exemple)
class ModuleMovement {
public:
    Cell3DPosition posFrom;
    Cell3DPosition posTo;
    uint16_t stage;
    ModuleMovement(const Cell3DPosition& from, const Cell3DPosition& to, uint16_t s)
        : posFrom(from), posTo(to), stage(s) {}
};

enum EtatSecurite {
    ETAT_NON_LIE,
    ETAT_AUTHENTIFICATION,
    ETAT_AUTHENTIFIE,
    ETAT_CLE_ETABLIE
};

struct InfoSecurite {
    EtatSecurite etat;
    uint64_t n0, n1, n2;
    std::vector<uint8_t> K0, K1;
    int liensStructure;
    bool estModuleInitial;
    InfoSecurite() : etat(ETAT_NON_LIE), n0(0), n1(0), n2(0),
                     liensStructure(0), estModuleInitial(false) {}
};

class AkcpmBlockCode : public Catoms3DBlockCode {
private:
    Catoms3DBlock *module;  // Comme l'exemple!
    FCCLattice *lattice;
    InfoSecurite infoSecurite;
    
    static const int NB_LIGNES_CODE = 128;
    static const int DIVISEUR_SYNC = 10;
    static const int TOLERANCE_SYNC = 5;
    static const int DELAI_TRANSMISSION = 100;
    
    std::map<P2PNetworkInterface*, std::vector<uint8_t>> clesVoisins;
    std::map<P2PNetworkInterface*, bool> voisinsAuthentifies;
    std::map<P2PNetworkInterface*, bool> voisinsDansStructure;
    std::set<P2PNetworkInterface*> authentificationsEnCours;
    std::map<P2PNetworkInterface*, uint64_t> n0EnAttente;
    
    static uint64_t n2Global;
    static bool n2GlobalDefini;
    
    // Variables mouvement (comme l'exemple!)
    bool isMobile = false;
    int myStage = 0;
    int myDepth = 0;
    std::vector<ModuleMovement> &myCurrent;  // Référence!
    
    uint64_t H(uint64_t valeur);
    uint64_t H(const std::vector<uint8_t>& donnees);
    std::vector<uint8_t> L(int numeroLigne);
    std::vector<uint8_t> HL(uint64_t n);
    uint64_t XOR(uint64_t a, uint64_t b);
    uint64_t genererNonce();
    
    void algorithme1_Initier(P2PNetworkInterface* dest);
    void algorithme1_Verifier(P2PNetworkInterface* src, int liens, uint64_t n1, uint64_t x, const std::vector<uint8_t>& K0);
    void algorithme1_Completer(P2PNetworkInterface* src, uint64_t x1);
    void notifierVoisinsStructurePrete();
    void verifierNouveauxVoisins();
    int compterVoisinsConnectes();

public:
    static std::vector<ModuleMovement> planMouvements;
    
    AkcpmBlockCode(Catoms3DBlock *host);
    ~AkcpmBlockCode() {};
    
    static BlockCode* buildNewBlockCode(BuildingBlock *host) {
        return new AkcpmBlockCode((Catoms3DBlock*)host);
    }
    
    void surReceptionDemandeAuth(std::shared_ptr<Message> msg, P2PNetworkInterface* src);
    void surReceptionDefiCle(std::shared_ptr<Message> msg, P2PNetworkInterface* src);
    void surReceptionStructurePrete(std::shared_ptr<Message> msg, P2PNetworkInterface* src);
    void myMotionUpdateFunc(std::shared_ptr<Message> msg, P2PNetworkInterface* sender);
    
    bool tryToMove(int index);  // INDEX comme l'exemple!
    
    void startup() override;
    void processLocalEvent(EventPtr pev) override;
    void onMotionEnd() override;
    void parseUserBlockElements(TiXmlElement *config) override;
    string onInterfaceDraw() override;
    
    bool estDansStructure() const;
    void definirCommeModuleInitial();
};

class MessageDemandeAuth : public Message {
public:
    int liens;
    uint64_t n1, x;
    std::vector<uint8_t> K0;
    MessageDemandeAuth(int _liens, uint64_t _n1, uint64_t _x, const std::vector<uint8_t>& _K0);
    ~MessageDemandeAuth() override;
};

class MessageDefiCle : public Message {
public:
    uint64_t x1;
    MessageDefiCle(uint64_t _x1);
    ~MessageDefiCle() override;
};

class MessageStructurePrete : public Message {
public:
    MessageStructurePrete();
    ~MessageStructurePrete() override;
};

#endif
