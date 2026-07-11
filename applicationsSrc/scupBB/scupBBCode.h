/**
 * @file scupBBCode.h
 * @brief S-CUP Protocol Implementation for BlinkyBlocks
 * 
 * S-CUP: A Security Protocol for Self-reconfiguration by Clustering
 * on Programmable Matter based Modular Robots (WINCOM 2025)
 * 
 * Authors: Youssou Faye, Abdallah Makhoul, Mohammed Ouzzif, et al.
 * 
 * This implementation provides:
 * - Mutual authentication using SPONGENT-160 lightweight hash
 * - Secure skeleton formation (Cluster Heads authentication)
 * - Cluster-based secure reconfiguration
 * - Time synchronization with tolerance
 * - Proof of code execution
 * 
 * @author Implementation for VisibleSim
 * @date 2025
 */

#ifndef SCUP_BB_CODE_H_
#define SCUP_BB_CODE_H_

#include "robots/blinkyBlocks/blinkyBlocksBlockCode.h"
#include "robots/blinkyBlocks/blinkyBlocksBlock.h"
#include <vector>
#include <map>
#include <set>
#include <queue>
#include <memory>

using namespace BlinkyBlocks;

// =============================================================================
// PROTOCOL PARAMETERS (as per S-CUP paper)
// =============================================================================

// Cryptographic parameters
#define SCUP_CODE_LINES         128     // Nb: Number of source code lines
#define SCUP_LINE_SIZE_BITS     256     // Size of one code line in bits
#define SCUP_HASH_SIZE_BYTES     20     // SPONGENT-160 → 160 bits = 20 bytes
#define SCUP_TIME_DIVISOR        10     // Ts = round(ts / 10)
#define SCUP_TIME_TOLERANCE       5     // ±5 units of desynchronization tolerated
#define SCUP_TRANSMISSION_DELAY 100     // Δt simulated (µs)

// Formation model
enum FormationModel {
    SERIAL_MODEL = 1,      // Serial formation (1 shared key)
    PARALLEL_MODEL = 2     // Parallel formation (keys per pair)
};

#define SCUP_FORMATION_MODEL SERIAL_MODEL

// Cluster colors
#define SCUP_CLUSTER_1_COLOR  RED       // Cluster 1
#define SCUP_CLUSTER_2_COLOR  GREEN     // Cluster 2
#define SCUP_CLUSTER_3_COLOR  BLUE      // Cluster 3
#define SCUP_ICH_COLOR        YELLOW    // Initial Cluster Head
#define SCUP_CH_COLOR         ORANGE    // Cluster Head (before auth)
#define SCUP_CM_COLOR         GREY      // Cluster Member (before auth)
#define SCUP_AUTH_COLOR       CYAN      // During authentication

// Message types
static const int MSG_SCUP_AUTH_REQUEST   = 4001; // SF-2: CHn → iCH
static const int MSG_SCUP_KEY_CHALLENGE  = 4002; // SF-4: iCH → CHn
static const int MSG_SCUP_SKELETON_READY = 4003; // Skeleton ready signal
static const int MSG_SCUP_CLUSTER_READY  = 4004; // Cluster ready signal
static const int MSG_SCUP_START_FORMATION= 4005; // Start formation signal

// =============================================================================
// MODULE ROLES
// =============================================================================

enum ModuleRole {
    ROLE_UNDEFINED = 0,
    ROLE_ICH,              // Initial Cluster Head (first CH)
    ROLE_CH,               // Cluster Head (cluster leader)
    ROLE_CM                // Cluster Member
};

// =============================================================================
// PROTOCOL PHASES
// =============================================================================

enum ProtocolPhase {
    PHASE_WAITING = 0,
    PHASE_SKELETON,        // Phase 1: Skeleton formation (CHs)
    PHASE_CLUSTERING,      // Phase 2: Cluster formation (CMs)
    PHASE_OPERATIONAL      // Phase 3: Structure formed, operational
};

// =============================================================================
// SECURITY STATES
// =============================================================================

enum SecurityState {
    STATE_UNLINKED = 0,
    STATE_AUTHENTICATING,
    STATE_AUTHENTICATED,
    STATE_KEY_ESTABLISHED
};

// =============================================================================
// SECURITY INFORMATION STRUCTURE
// =============================================================================

struct SecurityInfo {
    ModuleRole role = ROLE_UNDEFINED;
    ProtocolPhase phase = PHASE_WAITING;
    SecurityState state = STATE_UNLINKED;
    int clusterId = -1;    // Cluster ID (-1 if unassigned)
    
    // Nonces (160 bits = 20 bytes)
    std::vector<uint8_t> n0;  // Local secret nonce (never transmitted)
    std::vector<uint8_t> n1;  // H(n0) — authentication fingerprint
    std::vector<uint8_t> n2;  // Key generation nonce
    
    // Keys (160 bits)
    std::vector<uint8_t> K0;  // H(L(n0 mod Nb)) — code proof
    std::vector<uint8_t> K1;  // H(L(n2 mod Nb)) — shared key
};

// =============================================================================
// S-CUP MESSAGES
// =============================================================================

/**
 * @brief SF-2: CHn → iCH : (n1[160b], x[160b], K0[160b])
 * Authentication request from CH to iCH
 */
class MessageAuthRequest : public Message {
public:
    std::vector<uint8_t> n1;   // 160 bits - H(n0)
    std::vector<uint8_t> x;    // 160 bits - Ts ⊕ n0
    std::vector<uint8_t> K0;   // 160 bits - H(L(n0 mod Nb))
    
    MessageAuthRequest(const std::vector<uint8_t>& _n1,
                      const std::vector<uint8_t>& _x,
                      const std::vector<uint8_t>& _K0);
    ~MessageAuthRequest() override {}
    
    Message* clone() const override {
        return new MessageAuthRequest(*this);
    }
};

/**
 * @brief SF-4: iCH → CHn : x1[160b]
 * Key challenge from iCH to CH
 */
class MessageKeyChallenge : public Message {
public:
    std::vector<uint8_t> x1;   // 160 bits - n2 ⊕ n0
    
    explicit MessageKeyChallenge(const std::vector<uint8_t>& _x1);
    ~MessageKeyChallenge() override {}
    
    Message* clone() const override {
        return new MessageKeyChallenge(*this);
    }
};

/**
 * @brief Skeleton ready notification
 */
class MessageSkeletonReady : public Message {
public:
    MessageSkeletonReady();
    ~MessageSkeletonReady() override {}
    
    Message* clone() const override {
        return new MessageSkeletonReady(*this);
    }
};

/**
 * @brief Cluster ready notification
 */
class MessageClusterReady : public Message {
public:
    int clusterId;
    
    explicit MessageClusterReady(int _clusterId);
    ~MessageClusterReady() override {}
    
    Message* clone() const override {
        return new MessageClusterReady(*this);
    }
};

/**
 * @brief Start formation signal
 */
class MessageStartFormation : public Message {
public:
    MessageStartFormation();
    ~MessageStartFormation() override {}
    
    Message* clone() const override {
        return new MessageStartFormation(*this);
    }
};

// =============================================================================
// MAIN S-CUP BLOCKCODE CLASS
// =============================================================================

/**
 * @class SCupBBCode
 * @brief S-CUP protocol implementation for BlinkyBlocks
 * 
 * This class implements the complete S-CUP protocol including:
 * - Algorithm 1: Mutual authentication for skeleton formation (SF-1 to SF-5)
 * - Cryptographic primitives (SPONGENT-160 based)
 * - Cluster management
 * - Secure reconfiguration
 */
class SCupBBCode : public BlinkyBlocksBlockCode {
private:
    BlinkyBlocksBlock *module;
    
    // --- Security Information ---
    SecurityInfo security;
    
    // Shared keys with each neighbor (160 bits)
    std::map<P2PNetworkInterface*, std::vector<uint8_t>> neighborKeys;
    
    // Authenticated neighbors
    std::map<P2PNetworkInterface*, bool> authenticatedNeighbors;
    
    // Ongoing authentications (avoid duplicates)
    std::set<P2PNetworkInterface*> ongoingAuthentications;
    
    // Pending nonces (for SF-5)
    std::map<P2PNetworkInterface*, std::vector<uint8_t>> pendingN0;
    
    // Statistics counters
    int messagesSent = 0;
    int messagesReceived = 0;
    int successfulAuthentications = 0;
    
public:
    SCupBBCode(BlinkyBlocksBlock *host);
    ~SCupBBCode() override;
    
    /**
     * @brief Initialization function called at simulation start
     */
    void startup() override;
    
    /**
     * @brief Process incoming events (messages, etc.)
     */
    void processLocalEvent(EventPtr pev) override;
    
    /**
     * @brief Parse user-defined configuration elements
     */
    void parseUserBlockElements(TiXmlElement *config) override;
    
    // --- Cryptographic Primitives ---
    
    /**
     * @brief H(): SPONGENT-160 hash function → 160 bits
     */
    std::vector<uint8_t> H(const std::vector<uint8_t>& data);
    std::vector<uint8_t> H(uint64_t val);
    
    /**
     * @brief L(i): Source code line i → 256 bits
     */
    std::vector<uint8_t> L(int line);
    
    /**
     * @brief HL(n) = H(L(n mod Nb)) → 160 bits
     */
    std::vector<uint8_t> HL(const std::vector<uint8_t>& n);
    std::vector<uint8_t> HL(uint64_t n);
    
    /**
     * @brief XOR operation on 160-bit vectors
     */
    std::vector<uint8_t> xorVec(const std::vector<uint8_t>& a,
                                const std::vector<uint8_t>& b);
    
    /**
     * @brief Generate random 160-bit nonce
     */
    std::vector<uint8_t> generateNonce160();
    
    /**
     * @brief Convert timestamp to vector: Ts = round(ts / DIVISOR) → 160 bits
     */
    std::vector<uint8_t> timestampToVec(uint64_t ts);
    
    // --- Algorithm 1: Authentication for Skeleton Formation ---
    
    /**
     * @brief SF-1 + SF-2: CHn initiates authentication
     * Generates n0, calculates n1 = H(n0), K0 = H(L(n0 mod Nb)),
     * x = Ts ⊕ n0, and sends (n1, x, K0) to iCH
     */
    void algorithm1_Initiate(P2PNetworkInterface* dest);
    
    /**
     * @brief SF-3 + SF-4: iCH verifies synchronization and code
     * Recovers n0' = x ⊕ Ts', verifies n1 == H(n0') and K0 == H(L(n0' mod Nb)),
     * generates n2, calculates x1 = n2 ⊕ n0', K1 = H(L(n2 mod Nb)),
     * and sends x1 to CHn
     */
    void algorithm1_Verify(P2PNetworkInterface* src,
                          const std::vector<uint8_t>& n1,
                          const std::vector<uint8_t>& x,
                          const std::vector<uint8_t>& K0);
    
    /**
     * @brief SF-5: CHn completes authentication
     * Recovers n2 = x1 ⊕ n0, calculates K1 = H(L(n2 mod Nb))
     * CHn and iCH now share K1
     */
    void algorithm1_Complete(P2PNetworkInterface* src,
                            const std::vector<uint8_t>& x1);
    
    // --- Formation Management ---
    
    /**
     * @brief Start skeleton formation phase
     */
    void startSkeletonFormation();
    
    /**
     * @brief Start cluster formation phase
     */
    void startClusterFormation();
    
    /**
     * @brief Start operational phase
     */
    void startOperationalPhase();
    
    // --- Utility Functions ---
    
    bool isCH() const;
    bool isICH() const;
    bool isCM() const;
    bool isInSkeleton() const;
    
    void setAsICH();
    void setAsCH();
    void setAsCM();
    
    // --- Cluster Management ---
    
    int determineClusterId() const;
    Color determineClusterColor() const;
    void applyClusterColor();
    
    // --- Notifications ---
    
    void notifyNeighborsFormationStart();
    void notifyNeighborsSkeletonReady();
    void notifyNeighborsClusterReady();
    
    // --- Statistics ---
    
    void displayStatistics() const;
    void displayGlobalStatistics() const;
    
    // --- Message Handlers ---
    
    void onAuthRequestReceived(MessagePtr msg, P2PNetworkInterface* src);
    void onKeyChallengeReceived(MessagePtr msg, P2PNetworkInterface* src);
    void onSkeletonReadyReceived(MessagePtr msg, P2PNetworkInterface* src);
    void onClusterReadyReceived(MessagePtr msg, P2PNetworkInterface* src);
    void onStartFormationReceived(MessagePtr msg, P2PNetworkInterface* src);
    
    // --- Helper Functions ---
    
    /**
     * @brief Check if interface is valid and connected
     */
    bool isInterfaceValid(P2PNetworkInterface* iface) const;
    
    /**
     * @brief Required function to associate code with module
     */
    static BlockCode *buildNewBlockCode(BuildingBlock *host) {
        return new SCupBBCode((BlinkyBlocksBlock*)host);
    }
};

#endif /* SCUP_BB_CODE_H_ */
