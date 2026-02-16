#ifndef MonApplicationCode_H_
#define MonApplicationCode_H_

#include "robots/catoms3D/catoms3DSimulator.h"
#include "robots/catoms3D/catoms3DWorld.h"
#include "robots/catoms3D/catoms3DBlockCode.h"
#include "robots/catoms3D/catoms3DMotionEngine.h"
#include <set>
#include <map>
#include <vector>
#include <queue>

using namespace Catoms3D;

#define NB_LIGNES_CODE 128
#define TAILLE_LIGNE_BITS 256
#define DIVISEUR_SYNC 10
#define TOLERANCE_SYNC 5
#define DELAI_TRANSMISSION 100

static const int MSG_AUTH_REQUEST = 2001;
static const int MSG_KEY_CHALLENGE = 2002;
static const int MSG_STRUCTURE_READY = 2003;

enum EtatSecurite {
    ETAT_NON_LIE = 0,
    ETAT_AUTHENTIFICATION,
    ETAT_AUTHENTIFIE,
    ETAT_CLE_ETABLIE
};

struct InfoSecurite {
    EtatSecurite etat = ETAT_NON_LIE;
    bool estModuleInitial = false;
    int liensStructure = 0;
    uint64_t n0 = 0;
    uint64_t n1 = 0;
    uint64_t n2 = 0;
    std::vector<uint8_t> K0;
    std::vector<uint8_t> K1;
};

class MessageDemandeAuth : public Message {
public:
    int liens;
    uint64_t n1;
    uint64_t x;
    std::vector<uint8_t> K0;
    MessageDemandeAuth(int _liens, uint64_t _n1, uint64_t _x, const std::vector<uint8_t>& _K0);
    ~MessageDemandeAuth();
};

class MessageDefiCle : public Message {
public:
    uint64_t x1;
    MessageDefiCle(uint64_t _x1);
    ~MessageDefiCle();
};

class MessageStructurePrete : public Message {
public:
    MessageStructurePrete();
    ~MessageStructurePrete();
};

class MonApplicationCode : public Catoms3DBlockCode {
private:
    Catoms3DBlock *module = nullptr;
    
    // === RECONFIGURATION ===
    Cell3DPosition myTarget;
    std::set<Cell3DPosition> visited;
    int moveSteps = 0;
    bool isMoving = false;
    Cell3DPosition lastPosition;
    
    // === AKC-PM ===
    InfoSecurite infoSecurite;
    std::map<P2PNetworkInterface*, std::vector<uint8_t>> clesVoisins;
    std::map<P2PNetworkInterface*, bool> voisinsAuthentifies;
    std::map<P2PNetworkInterface*, bool> voisinsDansStructure;
    std::set<P2PNetworkInterface*> authentificationsEnCours;
    std::map<P2PNetworkInterface*, uint64_t> n0EnAttente;

public:
    MonApplicationCode(Catoms3DBlock *host);
    ~MonApplicationCode();

    void startup() override;
    void onMotionEnd() override;
    void processLocalEvent(EventPtr pev) override;
    void parseUserBlockElements(TiXmlElement *config) override;
    
    // === Reconfiguration ===
    bool tryMoveToward(const Cell3DPosition &goal);
    
    // === AKC-PM ===
    uint64_t H(uint64_t valeur);
    uint64_t H(const std::vector<uint8_t>& donnees);
    std::vector<uint8_t> L(int numeroLigne);
    std::vector<uint8_t> HL(uint64_t n);
    uint64_t XOR(uint64_t a, uint64_t b);
    uint64_t genererNonce();
    
    void definirCommeModuleInitial();
    bool estDansStructure() const;
    void algorithme1_Initier(P2PNetworkInterface* dest);
    void algorithme1_Verifier(P2PNetworkInterface* src, int liens, uint64_t n1, uint64_t x, const std::vector<uint8_t>& K0);
    void algorithme1_Completer(P2PNetworkInterface* src, uint64_t x1);
    void notifierVoisinsStructurePrete();
    void verifierNouveauxVoisins();
    
    void surReceptionDemandeAuth(std::shared_ptr<Message> msg, P2PNetworkInterface* src);
    void surReceptionDefiCle(std::shared_ptr<Message> msg, P2PNetworkInterface* src);
    void surReceptionStructurePrete(std::shared_ptr<Message> msg, P2PNetworkInterface* src);

    static BlockCode *buildNewBlockCode(BuildingBlock *host) {
        return (new MonApplicationCode((Catoms3DBlock*)host));
    }
};

#endif
