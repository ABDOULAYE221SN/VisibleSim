/**
 * SCupCatoms3DTestCode.h
 * Protocole S-CUP — variante de test avec module falsifié
 *
 * 7 modules, 2 clusters :
 *   Cluster 1 (JAUNE) : iCH=(2,2,2), CM1=(2,3,2), CM2=(3,3,2)
 *   Cluster 2 (BLEU)  : CH2=(3,2,2), CM3=(4,1,2), CM4=(4,3,2), CM5=(4,2,2) FAKE
 *   Module FAKE        : CM5=(4,2,2) — code source non conforme (cluster 2, BLEU)
 *
 * Séquence (7 étapes) :
 *   0. iCH  (2,2,2) → (5,3,2)   [pas d'auth]
 *   1. CH2  (3,2,2) → (5,2,2)   [auth → BLEU]
 *   2. CM1  (2,3,2) → (6,3,2)   [auth → JAUNE]
 *   3. CH2  (5,2,2) → (6,2,2)   [auth → BLEU]
 *   4. CM2  (3,3,2) → (7,3,2) et CM3 (4,1,2) → (6,1,2)  [simultané, auth → JAUNE/BLEU]
 *   5. CM5  (4,2,2) → (6,0,2)   [FAKE : auth ECHEC → ROUGE → retour (3,3,2)]
 *   6. CM4  (4,3,2) → (6,0,2)   [auth → BLEU]
 */
#ifndef SCUP_CATOMS3D_TEST_CODE_H_
#define SCUP_CATOMS3D_TEST_CODE_H_

#include "robots/catoms3D/catoms3DBlockCode.h"
#include "robots/catoms3D/catoms3DBlock.h"
#include "robots/catoms3D/catoms3DMotionEngine.h"
#include <vector>
#include <map>
#include <set>
#include <sstream>

using namespace Catoms3D;

// =============================================================================
// PARAMETRES
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

// Etats de sécurité
enum SecurityState { STATE_UNLINKED=0, STATE_AUTHENTICATING=1, STATE_KEY_ESTABLISHED=2 };

// Rôle dans la structure
enum ModuleRole { ROLE_NONE=0, ROLE_ICH, ROLE_CH2, ROLE_CM, ROLE_FAKE };

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
class MessageAuthRequestT : public Message {
public:
    std::vector<uint8_t> n1, x, K0;
    MessageAuthRequestT(const std::vector<uint8_t>& _n1,
                        const std::vector<uint8_t>& _x,
                        const std::vector<uint8_t>& _K0);
    ~MessageAuthRequestT() override {}
    Message* clone() const override { return new MessageAuthRequestT(*this); }
};

class MessageKeyChallengeT : public Message {
public:
    std::vector<uint8_t> x1;
    explicit MessageKeyChallengeT(const std::vector<uint8_t>& _x1);
    ~MessageKeyChallengeT() override {}
    Message* clone() const override { return new MessageKeyChallengeT(*this); }
};

class MessageAuthEchecT : public Message {
public:
    MessageAuthEchecT();
    ~MessageAuthEchecT() override {}
    Message* clone() const override { return new MessageAuthEchecT(*this); }
};

// =============================================================================
// CLASSE PRINCIPALE
// =============================================================================
class SCupCatoms3DTestCode : public Catoms3DBlockCode {
private:
    Catoms3DBlock* module;
    SecurityInfo   security;
    ModuleRole     role        = ROLE_NONE;
    bool           isFalsified = false;
    bool           isReturning = false;  // true quand le FAKE repart vers sa position de retrait

    Cell3DPosition myTarget;
    std::set<Cell3DPosition> visited;
    int  moveSteps = 0;

    std::map<P2PNetworkInterface*, std::vector<uint8_t>> neighborKeys;
    std::map<P2PNetworkInterface*, bool>                 authenticatedNeighbors;
    std::set<P2PNetworkInterface*>                       ongoingAuthentications;
    std::map<P2PNetworkInterface*, std::vector<uint8_t>> pendingN0;

public:
    SCupCatoms3DTestCode(Catoms3DBlock* host);
    ~SCupCatoms3DTestCode() override;

    void startup() override;
    void onMotionEnd() override;
    void processLocalEvent(EventPtr pev) override;
    void parseUserBlockElements(TiXmlElement* config) override;

    bool tryMoveToward(const Cell3DPosition& goal);
    bool estDansStructure() const;
    bool isInterfaceValid(P2PNetworkInterface* iface) const;

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

    // Algorithme 1 — version normale
    void algorithm1_Initiate(P2PNetworkInterface* dest);
    // Algorithme 1 — version falsifiée (K0 aléatoire invalide)
    void algorithm1_InitiateFalsified(P2PNetworkInterface* dest);

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

    static BlockCode* buildNewBlockCode(BuildingBlock* host) {
        return new SCupCatoms3DTestCode((Catoms3DBlock*)host);
    }
};

#endif
