/**
 * SCupCatoms3DCode.h
 * Protocole S-CUP — 6 modules, 2 clusters, reconfiguration séquentielle
 *
 * Cluster 1 (JAUNE) : iCH=(2,2,2), CM1=(2,3,2), CM2=(3,3,2)
 * Cluster 2 (BLEU)  : CH2=(3,2,2),  CM3=(4,2,2), CM4=(4,3,2)
 *
 * Ordre de déplacement :
 *   1. (2,2,2) → (5,3,2)  [iCH, s'installe]
 *   2. (3,2,2) → (5,2,2)  [CH2, s'authentifie auprès de iCH]
 *   3. (2,3,2) → (6,3,2)  [CM1, s'authentifie auprès de iCH]
 *   4. (5,2,2) → (6,2,2)  [CH2 se déplace]
 *   5. (3,3,2) → (7,3,2) et (4,2,2) → (6,1,2)  [simultané]
 *   6. (4,3,2) → (6,0,2)  [CM4]
 */
#ifndef SCUP_CATOMS3D_CODE_H_
#define SCUP_CATOMS3D_CODE_H_

#include "robots/catoms3D/catoms3DBlockCode.h"
#include "robots/catoms3D/catoms3DBlock.h"
#include "robots/catoms3D/catoms3DMotionEngine.h"
#include <vector>
#include <map>
#include <set>
#include <sstream>

using namespace Catoms3D;

// =============================================================================
// PARAMETRES DU PROTOCOLE
// =============================================================================
#define SCUP_CODE_LINES         128
#define SCUP_LINE_SIZE_BITS     256
#define SCUP_HASH_SIZE_BYTES     20
#define SCUP_TIME_DIVISOR        10
#define SCUP_TIME_TOLERANCE       5
#define SCUP_TRANSMISSION_DELAY 100
#define SCUP_MAX_STEPS           50

// Types de messages
static const int MSG_SCUP_AUTH_REQUEST  = 5001;
static const int MSG_SCUP_KEY_CHALLENGE = 5002;
static const int MSG_SCUP_AUTH_ECHEC    = 5006;

// Etats de securite
enum SecurityState { STATE_UNLINKED=0, STATE_AUTHENTICATING=1, STATE_KEY_ESTABLISHED=2 };

// Role dans la structure
enum ModuleRole {
    ROLE_NONE = 0,
    ROLE_ICH,   // Initial Cluster Head (cluster 1)
    ROLE_CH2,   // Cluster Head cluster 2
    ROLE_CM     // Cluster Member
};

struct SecurityInfo {
    bool          isICH  = false;
    SecurityState state  = STATE_UNLINKED;
    int           liens  = 0;
    std::vector<uint8_t> n2;
    std::vector<uint8_t> K1;
};

// =============================================================================
// MESSAGES
// =============================================================================
class MessageAuthRequest : public Message {
public:
    std::vector<uint8_t> n1, x, K0;
    MessageAuthRequest(const std::vector<uint8_t>& _n1,
                       const std::vector<uint8_t>& _x,
                       const std::vector<uint8_t>& _K0);
    ~MessageAuthRequest() override {}
    Message* clone() const override { return new MessageAuthRequest(*this); }
};

class MessageKeyChallenge : public Message {
public:
    std::vector<uint8_t> x1;
    explicit MessageKeyChallenge(const std::vector<uint8_t>& _x1);
    ~MessageKeyChallenge() override {}
    Message* clone() const override { return new MessageKeyChallenge(*this); }
};

class MessageAuthEchec : public Message {
public:
    MessageAuthEchec();
    ~MessageAuthEchec() override {}
    Message* clone() const override { return new MessageAuthEchec(*this); }
};

// =============================================================================
// CLASSE PRINCIPALE
// =============================================================================
class SCupCatoms3DCode : public Catoms3DBlockCode {
private:
    Catoms3DBlock* module;
    SecurityInfo   security;
    ModuleRole     role = ROLE_NONE;

    // Reconfiguration
    Cell3DPosition myTarget;
    std::set<Cell3DPosition> visited;
    int  moveSteps = 0;

    std::map<P2PNetworkInterface*, std::vector<uint8_t>> neighborKeys;
    std::map<P2PNetworkInterface*, bool>                 authenticatedNeighbors;
    std::set<P2PNetworkInterface*>                       ongoingAuthentications;
    std::map<P2PNetworkInterface*, std::vector<uint8_t>> pendingN0;

public:
    SCupCatoms3DCode(Catoms3DBlock* host);
    ~SCupCatoms3DCode() override;

    void startup() override;
    void onMotionEnd() override;
    void processLocalEvent(EventPtr pev) override;
    void parseUserBlockElements(TiXmlElement* config) override;

    bool tryMoveToward(const Cell3DPosition& goal);
    bool estDansStructure() const;

    // Primitives cryptographiques
    std::vector<uint8_t> H(const std::vector<uint8_t>& data);
    std::vector<uint8_t> H(uint64_t val);
    std::vector<uint8_t> L(int line);
    std::vector<uint8_t> HL(const std::vector<uint8_t>& n);
    std::vector<uint8_t> HL(uint64_t n);
    std::vector<uint8_t> xorVec(const std::vector<uint8_t>& a, const std::vector<uint8_t>& b);
    std::vector<uint8_t> generateNonce160();
    std::vector<uint8_t> timestampToVec(uint64_t ts);
    std::string          hexShort(const std::vector<uint8_t>& v, int n = 4) const;

    // Algorithme 1
    void algorithm1_Initiate(P2PNetworkInterface* dest);
    void algorithm1_Verify  (P2PNetworkInterface* src,
                             const std::vector<uint8_t>& n1,
                             const std::vector<uint8_t>& x,
                             const std::vector<uint8_t>& K0);
    void algorithm1_Complete(P2PNetworkInterface* src,
                             const std::vector<uint8_t>& x1);

    void lancerReconfiguration();
    void lancerProchainMobile();

    // Gestionnaires de messages
    void onAuthRequestReceived (std::shared_ptr<Message> msg, P2PNetworkInterface* src);
    void onKeyChallengeReceived(std::shared_ptr<Message> msg, P2PNetworkInterface* src);
    void onAuthEchecReceived   (P2PNetworkInterface* src);

    bool isInterfaceValid(P2PNetworkInterface* iface) const;

    static BlockCode* buildNewBlockCode(BuildingBlock* host) {
        return new SCupCatoms3DCode((Catoms3DBlock*)host);
    }
};

#endif
