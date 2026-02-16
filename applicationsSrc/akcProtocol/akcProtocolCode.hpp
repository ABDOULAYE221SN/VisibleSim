/**
 * @file akcProtocolCode.hpp
 * @brief AKC-PM: Lightweight Distributed Security Protocol for Programmable Matter
 * @author Youssou Faye (adapted for VisibleSim with SPONGENT-160)
 * @date 2025-01-07
 * 
 * Implementation of Authentication, Key Management and Confidentiality 
 * protocol for modular robots based on the research paper by Faye et al.
 * 
 * Uses SPONGENT-160 for lightweight cryptography (instead of SHA-256)
 * to respect hardware constraints of ATtiny45 modules.
 **/

#ifndef AKCPROTOCOLCODE_HPP_
#define AKCPROTOCOLCODE_HPP_

#include "robots/catoms3D/catoms3DSimulator.h"
#include "robots/catoms3D/catoms3DWorld.h"
#include "robots/catoms3D/catoms3DBlock.h"
#include "robots/catoms3D/catoms3DBlockCode.h"
#include "spongent160.hpp"

#include <vector>
#include <map>
#include <set>
#include <string>
#include <cstdint>
#include <random>

// =================================================================
// MESSAGE IDs
// =================================================================
static const int AUTH_REQUEST_MSG_ID = 2001;      // SF-2: Authentication request
static const int AUTH_CHALLENGE_MSG_ID = 2002;    // SF-4: Key generation challenge
static const int AUTH_PROOF_MSG_ID = 2003;        // Algorithm 2: Proof of membership
static const int AUTH_PROOF_PROP_MSG_ID = 2004;   // Algorithm 2: Proof propagation

using namespace Catoms3D;

// =================================================================
// KEY MODELS (as per paper)
// =================================================================
enum KeyModel {
    SINGLE_KEY = 0,    // One key for entire network (Figure 3-b)
    FOUR_KEYS = 1,     // Four keys in network (Figure 4)
    MULTI_KEYS = 2     // Pairwise keys (Figure 5)
};

// =================================================================
// MODULE AUTHENTICATION STATES
// =================================================================
enum AuthState {
    UNLINKED = 0,           // Not yet in structure
    AUTHENTICATING = 1,     // Authentication in progress
    LINKED = 2,             // Authenticated and in structure
    INITIAL_MODULE = 3      // Initial module (iM)
};

// =================================================================
// MESSAGE DATA STRUCTURES
// =================================================================

// Authentication Request (Algorithm 1 - SF-2)
struct AuthRequestData {
    int linkCount;              // Number of links in structure
    std::vector<uint8_t> n1;    // H(n0) - 20 bytes with SPONGENT-160
    std::vector<uint8_t> x;     // Ts ⊕ n0
    std::vector<uint8_t> K0;    // H(L(n0 mod Nb)) - 20 bytes
    uint64_t sendTime;          // Timestamp
    
    AuthRequestData() : linkCount(0), sendTime(0) {}
};

// Authentication Challenge (Algorithm 1 - SF-4)
struct AuthChallengeData {
    std::vector<uint8_t> x1;    // n2 ⊕ n0 (or K1 ⊕ n0 for multi-keys)
    int hashIterations;         // Number of hash iterations (for 4-keys)
    int keyModel;               // Key model used
    
    AuthChallengeData() : hashIterations(0), keyModel(SINGLE_KEY) {}
};

// Proof of Membership (Algorithm 2)
struct AuthProofData {
    int hopsToPropagate;                // Hops to propagate
    std::vector<uint8_t> n1;            // H(n0) - 20 bytes
    int sourceInterfaceId;              // Source interface for tracking
    
    AuthProofData() : hopsToPropagate(0), sourceInterfaceId(-1) {}
};

// Proof Propagation (Algorithm 2)
struct AuthProofPropData {
    int hopsRemaining;                  // Remaining hops
    std::vector<uint8_t> xv;            // n0 ⊕ Kv
    
    AuthProofPropData() : hopsRemaining(0) {}
};

// =================================================================
// CRYPTO UTILITIES (with SPONGENT-160)
// =================================================================

class CryptoUtils {
private:
    std::vector<std::string> sourceCode;
    std::mt19937_64 rng;
    Spongent160 spongent;  // SPONGENT-160 hash function
    
public:
    static const int HASH_SIZE = 20;  // SPONGENT-160 = 20 bytes (160 bits)
    
    CryptoUtils();
    
    // Hash functions using SPONGENT-160
    std::vector<uint8_t> hash(const std::vector<uint8_t>& data);
    std::vector<uint8_t> hash(const std::string& str);
    std::vector<uint8_t> hashCodeLine(uint64_t n0);
    std::vector<uint8_t> hashIterative(const std::vector<uint8_t>& data, int iterations);
    
    // XOR operations
    std::vector<uint8_t> xorOp(const std::vector<uint8_t>& a, const std::vector<uint8_t>& b);
    std::vector<uint8_t> xorOp(uint64_t a, const std::vector<uint8_t>& b);
    
    // Nonce management
    uint64_t generateNonce();
    std::vector<uint8_t> nonceToBytes(uint64_t nonce);
    uint64_t bytesToNonce(const std::vector<uint8_t>& bytes);
    
    // Source code initialization
    void initSourceCode(int nbLines = 128);
    int getLineCount() const { return sourceCode.size(); }
    
    // Utility
    static std::string bytesToHex(const std::vector<uint8_t>& bytes);
};

// =================================================================
// MAIN PROTOCOL CLASS
// =================================================================

class AkcProtocolCode : public Catoms3DBlockCode {
private:
    Catoms3DBlock *module = nullptr;
    
    // ============================================================
    // Protocol State
    // ============================================================
    AuthState authState;
    bool isInitialModule;
    int linkCount;
    KeyModel currentKeyModel;
    
    // ============================================================
    // Cryptography
    // ============================================================
    CryptoUtils crypto;
    
    // ============================================================
    // Key Storage
    // ============================================================
    std::vector<uint8_t> singleNetworkKey;                          // SINGLE_KEY model
    std::vector<std::vector<uint8_t>> fourKeys;                     // FOUR_KEYS model (4 keys)
    std::map<P2PNetworkInterface*, std::vector<uint8_t>> pairwiseKeys;  // MULTI_KEYS model
    int currentKeyIndex;
    
    // ============================================================
    // Authentication Management
    // ============================================================
    std::map<P2PNetworkInterface*, uint64_t> pendingAuth_n0;
    std::map<P2PNetworkInterface*, uint64_t> pendingAuth_sendTime;
    std::set<P2PNetworkInterface*> authenticatedNeighbors;
    
    // ============================================================
    // Protocol Parameters (from paper)
    // ============================================================
    static const int TRANSMISSION_DELAY = 5;  // Δt in ms
    static const int SOURCE_CODE_LINES = 128; // Nb
    
    // ============================================================
    // Stored nonce for key regeneration
    // ============================================================
    uint64_t storedNonce;
    
    // ============================================================
    // Performance Metrics
    // ============================================================
    int totalHashOperations;
    int totalXorOperations;
    int totalAuthAttempts;
    int successfulAuths;

public:
    AkcProtocolCode(Catoms3DBlock *host);
    ~AkcProtocolCode() {};
    
    /**
     * @brief Startup function called when simulation starts
     * Initializes the module and starts authentication if needed
     */
    void startup() override;
    
    /**
     * @brief Message handlers (registered via addMessageEventFunc2)
     */
    void handle_AuthRequest(std::shared_ptr<Message> _msg, P2PNetworkInterface *sender);
    void handle_AuthChallenge(std::shared_ptr<Message> _msg, P2PNetworkInterface *sender);
    void handle_AuthProof(std::shared_ptr<Message> _msg, P2PNetworkInterface *sender);
    void handle_AuthProofProp(std::shared_ptr<Message> _msg, P2PNetworkInterface *sender);
    
    /**
     * @brief Parse configuration attributes (leader, keyModel, etc.)
     */
    void parseUserBlockElements(TiXmlElement *config) override;
    
    // ============================================================
    // ALGORITHM 1: Structure Formation (iM ↔ nM)
    // ============================================================
    void initiateStructureFormation(P2PNetworkInterface* target);
    void processAuthRequest(std::shared_ptr<Message> _msg, P2PNetworkInterface* sender);
    void processAuthChallenge(std::shared_ptr<Message> _msg, P2PNetworkInterface* sender);
    
    // ============================================================
    // ALGORITHM 2: Proof of Membership (Mi ↔ Mj)
    // ============================================================
    void initiateProofOfMembership(P2PNetworkInterface* target);
    void processProofOfMembership(std::shared_ptr<Message> _msg, P2PNetworkInterface* sender);
    void propagateProof(uint64_t n0, P2PNetworkInterface* excludeInterface);
    
    // ============================================================
    // Key Management
    // ============================================================
    std::vector<uint8_t> generateKey(uint64_t n2, uint64_t n0, P2PNetworkInterface* interface = nullptr);
    void storeKey(const std::vector<uint8_t>& key, P2PNetworkInterface* interface = nullptr);
    std::vector<uint8_t> getKey(P2PNetworkInterface* interface = nullptr);
    bool hasKey(P2PNetworkInterface* interface = nullptr);
    int calculateHashIterations();
    
    // ============================================================
    // Time Synchronization (as per paper: Ts ≈ (ts/10))
    // ============================================================
    uint64_t calculateTs(uint64_t sendTime);
    bool verifyTimeSynchronization(const std::vector<uint8_t>& n1, uint64_t n0_derived);
    
    // ============================================================
    // Utilities
    // ============================================================
    void setAsInitialModule();
    bool isInStructure() const { return (authState == LINKED || authState == INITIAL_MODULE); }
    void onAuthenticationComplete(P2PNetworkInterface* interface);
    void printStatus();
    void printKeys();
    void printMetrics();
    
    // ============================================================
    // Required by VisibleSim
    // ============================================================
    static BlockCode *buildNewBlockCode(BuildingBlock *host) {
        return new AkcProtocolCode(static_cast<Catoms3DBlock*>(host));
    }
};

#endif /* AKCPROTOCOLCODE_HPP_ */
