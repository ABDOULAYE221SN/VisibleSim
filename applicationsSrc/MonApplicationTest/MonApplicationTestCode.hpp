#ifndef MonApplicationTestCode_H_
#define MonApplicationTestCode_H_

#include "robots/catoms3D/catoms3DSimulator.h"
#include "robots/catoms3D/catoms3DWorld.h"
#include "robots/catoms3D/catoms3DBlockCode.h"
#include "robots/catoms3D/catoms3DMotionEngine.h"
#include <set>
#include <map>
#include <vector>

using namespace Catoms3D;

#define NB_LIGNES_CODE      128
#define TAILLE_LIGNE_BITS   256
#define HASH_OUTPUT_BYTES    20
#define DIVISEUR_SYNC        10
#define TOLERANCE_SYNC        5
#define DELAI_TRANSMISSION  100
#define MAX_STEPS            50

static const int MSG_AUTH_REQUEST  = 2001;
static const int MSG_KEY_CHALLENGE = 2002;
static const int MSG_AUTH_ECHEC    = 2006;

enum EtatSecurite {
    ETAT_NON_LIE = 0,
    ETAT_AUTHENTIFICATION,
    ETAT_AUTHENTIFIE,
    ETAT_CLE_ETABLIE
};

struct InfoSecurite {
    EtatSecurite etat        = ETAT_NON_LIE;
    bool estModuleInitial    = false;
    int  liensStructure      = 0;
    std::vector<uint8_t> n0, n1, n2, K0, K1;
};

// SF-2 : nM → iM
class MessageDemandeAuth : public Message {
public:
    int liens;
    std::vector<uint8_t> n1, x, K0;
    MessageDemandeAuth(int _liens,
                       const std::vector<uint8_t>& _n1,
                       const std::vector<uint8_t>& _x,
                       const std::vector<uint8_t>& _K0);
    ~MessageDemandeAuth() {}
};

// SF-4 : iM → nM
class MessageDefiCle : public Message {
public:
    std::vector<uint8_t> x1;
    explicit MessageDefiCle(const std::vector<uint8_t>& _x1);
    ~MessageDefiCle() {}
};

// Echec d'authentification : iM → nM
class MessageAuthEchec : public Message {
public:
    MessageAuthEchec();
    ~MessageAuthEchec() {}
};

class MonApplicationTestCode : public Catoms3DBlockCode {
private:
    Catoms3DBlock *module = nullptr;

    Cell3DPosition myTarget;
    Cell3DPosition myHome;       // position initiale — utilisee par le module falsifie pour rentrer
    std::set<Cell3DPosition> visited;
    int  moveSteps   = 0;
    bool isReturning = false;
    bool authFailed  = false;
    bool isFalsified = false;    // true uniquement pour le module en (3,3,2)

    InfoSecurite infoSecurite;
    std::map<P2PNetworkInterface*, std::vector<uint8_t>> clesVoisins;
    std::map<P2PNetworkInterface*, bool>                 voisinsAuthentifies;
    std::set<P2PNetworkInterface*>                       authentificationsEnCours;
    std::map<P2PNetworkInterface*, std::vector<uint8_t>> n0EnAttente;

public:
    MonApplicationTestCode(Catoms3DBlock *host);
    ~MonApplicationTestCode();

    void startup() override;
    void onMotionEnd() override;
    void processLocalEvent(EventPtr pev) override;
    void parseUserBlockElements(TiXmlElement *config) override;

    bool tryMoveToward(const Cell3DPosition &goal);

    std::vector<uint8_t> H(const std::vector<uint8_t>& data);
    std::vector<uint8_t> H(uint64_t val);
    std::vector<uint8_t> L(int ligne);
    std::vector<uint8_t> HL(const std::vector<uint8_t>& n);
    std::vector<uint8_t> HL(uint64_t n);
    std::vector<uint8_t> xorVec(const std::vector<uint8_t>& a, const std::vector<uint8_t>& b);
    std::vector<uint8_t> genererNonce160();
    std::vector<uint8_t> tsVersVec(uint64_t ts);

    bool estDansStructure() const;
    void definirCommeModuleInitial();
    void afficherStatsGlobal() const;

    void algorithme1_Initier(P2PNetworkInterface* dest);
    void algorithme1_InitierFalsifie(P2PNetworkInterface* dest);
    void algorithme1_Verifier(P2PNetworkInterface* src, int liens,
                              const std::vector<uint8_t>& n1,
                              const std::vector<uint8_t>& x,
                              const std::vector<uint8_t>& K0);
    void algorithme1_Completer(P2PNetworkInterface* src,
                               const std::vector<uint8_t>& x1);

    void lancerReconfiguration();
    void lancerProchainMobile();

    void surReceptionDemandeAuth(std::shared_ptr<Message> msg, P2PNetworkInterface* src);
    void surReceptionDefiCle(std::shared_ptr<Message> msg, P2PNetworkInterface* src);
    void surReceptionAuthEchec(P2PNetworkInterface* src);

    static BlockCode *buildNewBlockCode(BuildingBlock *host) {
        return new MonApplicationTestCode((Catoms3DBlock*)host);
    }
};

#endif
