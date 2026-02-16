#include <iostream>
#include <random>
#include <cmath>
#include "akcpmBlockCode.h"

using namespace std;
using namespace Catoms3D;

// Static variables for SINGLE_KEY model
uint64_t AkcpmBlockCode::globalN2 = 0;
bool AkcpmBlockCode::globalN2Set = false;

// Static const definitions (required for linker)
const int AkcpmBlockCode::NB_CODE_LINES;
const int AkcpmBlockCode::LINE_SIZE_BITS;
const int AkcpmBlockCode::SYNC_DIVISOR;
const int AkcpmBlockCode::SYNC_TOLERANCE;
const int AkcpmBlockCode::TRANSMISSION_DELAY;

// ═══════════════════════════════════════════════════════════════════════════
// CONSTRUCTOR / DESTRUCTOR
// ═══════════════════════════════════════════════════════════════════════════

AkcpmBlockCode::AkcpmBlockCode(Catoms3DBlock *host) 
    : Catoms3DBlockCode(host), keyModel(FOUR_KEYS), fourKeysCounter(1) {
}

AkcpmBlockCode::~AkcpmBlockCode() {
    neighborKeys.clear();
    authenticatedNeighbors.clear();
    neighborsInStructure.clear();
    pendingAuthentications.clear();
    pendingN0.clear();
}

// ═══════════════════════════════════════════════════════════════════════════
// CRYPTOGRAPHIC FUNCTIONS
// ═══════════════════════════════════════════════════════════════════════════

// H() - One-way hash function (DJB2 algorithm - lightweight)
uint64_t AkcpmBlockCode::H(uint64_t value) {
    uint64_t hash = 5381;
    for (int i = 0; i < 8; i++) {
        hash = ((hash << 5) + hash) + ((value >> (i * 8)) & 0xFF);
    }
    return hash;
}

uint64_t AkcpmBlockCode::H(const std::vector<uint8_t>& data) {
    uint64_t hash = 5381;
    for (auto byte : data) {
        hash = ((hash << 5) + hash) + byte;
    }
    return hash;
}

// L(i) - Get line i of source code (simulated)
std::vector<uint8_t> AkcpmBlockCode::L(int lineNumber) {
    // Simulate source code line with deterministic content
    std::vector<uint8_t> line(LINE_SIZE_BITS / 8);  // 256 bits = 32 bytes
    
    // Generate deterministic line content based on line number
    uint64_t seed = 0xDEADBEEF ^ (lineNumber * 0x12345678);
    for (size_t i = 0; i < line.size(); i++) {
        seed = seed * 1103515245 + 12345;  // LCG
        line[i] = (seed >> 16) & 0xFF;
    }
    
    return line;
}

// H(L(n mod Nb)) - Hash of code line for K0, K1 generation
std::vector<uint8_t> AkcpmBlockCode::HL(uint64_t n) {
    int lineNumber = n % NB_CODE_LINES;
    std::vector<uint8_t> line = L(lineNumber);
    uint64_t hash = H(line);
    
    // Convert hash to vector
    std::vector<uint8_t> result(8);
    for (int i = 0; i < 8; i++) {
        result[i] = (hash >> (i * 8)) & 0xFF;
    }
    return result;
}

// XOR operation
uint64_t AkcpmBlockCode::XOR(uint64_t a, uint64_t b) {
    return a ^ b;
}

// Concatenation: (high || low) -> 128 bits represented as two 64-bit values
void AkcpmBlockCode::concat128(uint64_t high, uint64_t low, 
                                uint64_t& outHigh, uint64_t& outLow) {
    outHigh = high;
    outLow = low;
}

// XOR on 128-bit values
void AkcpmBlockCode::xor128(uint64_t aHigh, uint64_t aLow, 
                            uint64_t bHigh, uint64_t bLow,
                            uint64_t& outHigh, uint64_t& outLow) {
    outHigh = aHigh ^ bHigh;
    outLow = aLow ^ bLow;
}

// Apply H() n times: H(H(H(...H(value)...)))
uint64_t AkcpmBlockCode::applyHashNTimes(uint64_t value, int n) {
    uint64_t result = value;
    for (int i = 0; i < n; i++) {
        result = H(result);
    }
    return result;
}

// Generate random nonce
uint64_t AkcpmBlockCode::generateNonce() {
    static std::random_device rd;
    static std::mt19937_64 gen(rd());
    std::uniform_int_distribution<uint64_t> dis;
    return dis(gen);
}

// ═══════════════════════════════════════════════════════════════════════════
// TIME SYNCHRONIZATION (Section VI.A, VII.A)
// Ts ≈ ts/10 - Allows correction of ±5 units offset
// ═══════════════════════════════════════════════════════════════════════════

uint64_t AkcpmBlockCode::getTs() {
    // Ts ≈ ts/10 (Article Section VI.A)
    uint64_t ts = getScheduler()->now();
    return ts / SYNC_DIVISOR;
}

uint64_t AkcpmBlockCode::calculateTs(uint64_t Tr, int deltaT) {
    // Ts ≈ (Tr - Δt)/10 (Article Algorithm 1, SF-3)
    return (Tr - deltaT) / SYNC_DIVISOR;
}

// ═══════════════════════════════════════════════════════════════════════════
// UTILITY FUNCTIONS
// ═══════════════════════════════════════════════════════════════════════════

int AkcpmBlockCode::countConnectedNeighbors() {
    Catoms3DBlock* catom = (Catoms3DBlock*)hostBlock;
    int count = 0;
    for (int i = 0; i < 12; i++) {
        P2PNetworkInterface* iface = catom->getInterface(i);
        if (iface && iface->connectedInterface) {
            count++;
        }
    }
    return count;
}

bool AkcpmBlockCode::isInStructure() const {
    return securityInfo.state == STATE_AUTHENTICATED ||
           securityInfo.state == STATE_KEY_ESTABLISHED ||
           securityInfo.isInitialModule;
}

void AkcpmBlockCode::setAsInitialModule() {
    securityInfo.isInitialModule = true;
    securityInfo.state = STATE_AUTHENTICATED;
    securityInfo.linksInStructure = 0;
    fourKeysCounter = 1;  // Start with K1 for FOUR_KEYS model
    
    Catoms3DBlock* catom = (Catoms3DBlock*)hostBlock;
    catom->setColor(BLUE);
    
    console << "═══════════════════════════════════════════════════════════\n";
    console << "★ MODULE " << hostBlock->blockId << " SET AS INITIAL (iM) ★\n";
    console << "  Key Model: " << (keyModel == SINGLE_KEY ? "SINGLE_KEY" : 
                                   keyModel == FOUR_KEYS ? "FOUR_KEYS" : "MULTI_KEY") << "\n";
    console << "═══════════════════════════════════════════════════════════\n";
}

// ═══════════════════════════════════════════════════════════════════════════
// STARTUP
// ═══════════════════════════════════════════════════════════════════════════

void AkcpmBlockCode::startup() {
    console << "═══════════════════════════════════════════════════════════\n";
    console << "Block " << hostBlock->blockId << " - AKC-PM Protocol Starting\n";
    console << "═══════════════════════════════════════════════════════════\n";
    
    Catoms3DBlock* catom = (Catoms3DBlock*)hostBlock;
    Cell3DPosition pos = catom->position;
    
    console << "Position: (" << pos[0] << "," << pos[1] << "," << pos[2] << ")\n";
    console << "Neighbors: " << countConnectedNeighbors() << "\n";
    console << "Key Model: " << (keyModel == SINGLE_KEY ? "SINGLE_KEY" : 
                                 keyModel == FOUR_KEYS ? "FOUR_KEYS" : "MULTI_KEY") << "\n";
    
    // Initial module designation by position (5,5,0)
    if (pos[0] == 5 && pos[1] == 5 && pos[2] == 0) {
        setAsInitialModule();
        
        // Schedule notification to neighbors after initialization
        getScheduler()->schedule(
            new InterruptionEvent(getScheduler()->now() + 1000, hostBlock, 0));
    } else {
        catom->setColor(GREY);
        console << "Waiting for STRUCTURE_READY signal...\n";
    }
}

// ═══════════════════════════════════════════════════════════════════════════
// ALGORITHM 1: Authentication for Structure Formation
// Reference: Section VI.A, Algorithm 1 in the article
// ═══════════════════════════════════════════════════════════════════════════

// SF-1, SF-2: New module (nM) initiates authentication to structure module (sM)
void AkcpmBlockCode::algorithm1_Initiate(P2PNetworkInterface* dest) {
    console << "───────────────────────────────────────────────────────────\n";
    console << "Block " << hostBlock->blockId << ": ALGORITHM 1 - INITIATE (SF-1, SF-2)\n";
    console << "───────────────────────────────────────────────────────────\n";
    
    // SF-1: Generate nonces and compute values
    // nM chooses n0
    securityInfo.n0 = generateNonce();
    
    // nM computes n1 = H(n0)
    securityInfo.n1 = H(securityInfo.n0);
    
    // nM computes K0 = H(L(n0 mod Nb))
    securityInfo.K0 = HL(securityInfo.n0);
    
    // nM computes x = Ts ⊕ n0
    // Article: "Ts ≈ ts/10"
    uint64_t ts_raw = getScheduler()->now();
    uint64_t Ts = ts_raw / SYNC_DIVISOR;
    uint64_t x = XOR(Ts, securityInfo.n0);
    
    console << "  n0 = " << securityInfo.n0 << " (secret)\n";
    console << "  n1 = H(n0) = " << securityInfo.n1 << "\n";
    console << "  ts = " << ts_raw << ", Ts = ts/10 = " << Ts << "\n";
    console << "  x = Ts ⊕ n0 = " << x << "\n";
    
    // SF-2: Send (links, n1, x||K0) to sM
    // links = 1 means "not yet joined structure"
    int links = (securityInfo.linksInStructure > 0) ? securityInfo.linksInStructure : 1;
    
    AuthRequestMessage* msg = new AuthRequestMessage(links, securityInfo.n1, x, securityInfo.K0);
    sendMessage(msg, dest, 100, 0);
    
    // Store n0 for this pending authentication
    pendingN0[dest] = securityInfo.n0;
    pendingAuthentications.insert(dest);
    securityInfo.state = STATE_AUTHENTICATING;
    
    console << "  → Sent AUTH_REQUEST (links=" << links << ", n1, x||K0)\n";
}

// SF-3, SF-4: Structure module (sM) verifies and responds
void AkcpmBlockCode::algorithm1_Verify(P2PNetworkInterface* src, int links, 
                                        uint64_t n1, uint64_t x, 
                                        const std::vector<uint8_t>& K0) {
    console << "───────────────────────────────────────────────────────────\n";
    console << "Block " << hostBlock->blockId << ": ALGORITHM 1 - VERIFY (SF-3, SF-4)\n";
    console << "───────────────────────────────────────────────────────────\n";
    
    // Only structure modules can authenticate new modules
    if (!isInStructure()) {
        console << "  ✗ REJECTED: Not in structure, cannot authenticate\n";
        return;
    }
    
    // SF-3: Verify synchronization and authentication
    // Article: "iM computes Ts ≈ ((Tr - Δt)/10)"
    // Then: "n0' = x ⊕ Ts" and verify "n1 = H(n0')"
    
    uint64_t Tr = getScheduler()->now();
    uint64_t n0_prime = 0;
    bool syncOK = false;
    
    // Calculate base Ts using transmission delay: Ts ≈ (Tr - Δt) / 10
    uint64_t Ts_base = (Tr - TRANSMISSION_DELAY) / SYNC_DIVISOR;
    
    console << "  Tr = " << Tr << ", Δt = " << TRANSMISSION_DELAY << "\n";
    console << "  Ts_base = (Tr - Δt)/10 = " << Ts_base << "\n";
    
    // Article Section VII.A: "Calculating the transmission clock divided by 10 
    // and rounded off to the nearest integer allows us to correct for 
    // uncertainties of -5 to +5 of the normal value"
    for (int offset = -SYNC_TOLERANCE; offset <= SYNC_TOLERANCE && !syncOK; offset++) {
        uint64_t Ts_test = Ts_base + offset;
        
        // Drift: n0' = x ⊕ Ts
        uint64_t n0_test = XOR(x, Ts_test);
        
        // Verify: n1 = H(n0')
        uint64_t n1_computed = H(n0_test);
        
        if (n1_computed == n1) {
            n0_prime = n0_test;
            syncOK = true;
            console << "  ✓ Sync OK at offset " << offset << " (Ts=" << Ts_test << ")\n";
        }
    }
    
    if (!syncOK) {
        console << "  ✗ SYNC FAILED: Could not verify n1 = H(n0')\n";
        console << "    Tried Ts from " << (Ts_base - SYNC_TOLERANCE) << " to " << (Ts_base + SYNC_TOLERANCE) << "\n";
        return;
    }
    
    console << "  n0' (recovered) = " << n0_prime << "\n";
    
    // Verify: K0 = H(L(n0' mod Nb))
    std::vector<uint8_t> K0_computed = HL(n0_prime);
    if (K0_computed != K0) {
        console << "  ✗ AUTH FAILED: K0 mismatch (fingerprint invalid)\n";
        return;
    }
    
    console << "  ✓ K0 verified - Module authenticated!\n";
    
    // SF-4: Generate key challenge
    uint64_t n2;
    std::vector<uint8_t> keyToStore;
    
    if (keyModel == SINGLE_KEY) {
        // SINGLE_KEY: use same n2 for all modules -> same K1
        if (securityInfo.isInitialModule) {
            if (!globalN2Set) {
                globalN2 = generateNonce();
                globalN2Set = true;
            }
        }
        n2 = globalN2;
        
        // Compute x1 = n2 ⊕ n0'
        uint64_t x1 = XOR(n2, n0_prime);
        
        // Compute K1 = H(L(n2 mod Nb))
        keyToStore = HL(n2);
        
        console << "  [SINGLE_KEY Model]\n";
        console << "  n2 = " << n2 << "\n";
        console << "  x1 = n2 ⊕ n0' = " << x1 << "\n";
        console << "  K1 = H(L(n2 mod Nb))\n";
        
        // Store key with this neighbor
        neighborKeys[src] = keyToStore;
        authenticatedNeighbors[src] = true;
        neighborsInStructure[src] = true;
        securityInfo.linksInStructure++;
        
        // Send x1 to nM (keyLevel = 0 for SINGLE_KEY)
        KeyChallengeMessage* msg = new KeyChallengeMessage(x1);
        sendMessage(msg, src, TRANSMISSION_DELAY, 0);
        
    } else if (keyModel == FOUR_KEYS) {
        // FOUR_KEYS: Article Section VI.B, Figure 4
        // Keys: K1, K2=H(H(K1)), K3=H(H(K2)), K4=H(H(K3))
        // x1 encoding: (H^(k-1)(n2) || n2) ⊕ (n0 || n0)
        
        n2 = generateNonce();
        
        // Determine which key level (1-4)
        int keyLevel = fourKeysCounter;
        fourKeysCounter = (fourKeysCounter % 4) + 1;  // Cycle: 1,2,3,4,1,2,...
        
        // Compute K1 first
        std::vector<uint8_t> K1 = HL(n2);
        uint64_t K1_uint64 = H(n2);  // As uint64 for chained hashing
        
        // Generate Kx by applying H twice for each level > 1
        // K2 = H(H(K1)), K3 = H(H(K2)), K4 = H(H(K3))
        uint64_t Kx = K1_uint64;
        for (int i = 1; i < keyLevel; i++) {
            Kx = H(H(Kx));  // Apply H twice for each level
        }
        
        // Store the appropriate key
        keyToStore.resize(8);
        for (int i = 0; i < 8; i++) {
            keyToStore[i] = (Kx >> (i * 8)) & 0xFF;
        }
        
        // Compute x1 = (H^(k-1)(n2) || n2) ⊕ (n0' || n0')
        // H^(k-1)(n2) means apply H (k-1) times to n2
        uint64_t h_n2 = applyHashNTimes(n2, keyLevel - 1);
        
        uint64_t x1_high, x1_low;
        xor128(h_n2, n2, n0_prime, n0_prime, x1_high, x1_low);
        
        console << "  [FOUR_KEYS Model - Level K" << keyLevel << "]\n";
        console << "  n2 = " << n2 << "\n";
        console << "  H^" << (keyLevel-1) << "(n2) = " << h_n2 << "\n";
        console << "  x1 = (H^(k-1)(n2)||n2) ⊕ (n0'||n0')\n";
        console << "  x1_high = " << x1_high << ", x1_low = " << x1_low << "\n";
        
        // Store key with this neighbor
        neighborKeys[src] = keyToStore;
        authenticatedNeighbors[src] = true;
        neighborsInStructure[src] = true;
        securityInfo.linksInStructure++;
        
        // Send x1 (128-bit) with key level
        KeyChallengeMessage* msg = new KeyChallengeMessage(x1_high, x1_low, keyLevel);
        sendMessage(msg, src, TRANSMISSION_DELAY, 0);
        
    } else {
        // MULTI_KEY: generate new n2 for each link
        n2 = generateNonce();
        
        // Compute x1 = n2 ⊕ n0'
        uint64_t x1 = XOR(n2, n0_prime);
        
        // Compute K = H(L(n2 mod Nb)) - unique per link
        keyToStore = HL(n2);
        
        console << "  [MULTI_KEY Model]\n";
        console << "  n2 = " << n2 << "\n";
        console << "  x1 = n2 ⊕ n0' = " << x1 << "\n";
        
        // Store key with this neighbor
        neighborKeys[src] = keyToStore;
        authenticatedNeighbors[src] = true;
        neighborsInStructure[src] = true;
        securityInfo.linksInStructure++;
        
        // Send x1 to nM
        KeyChallengeMessage* msg = new KeyChallengeMessage(x1);
        sendMessage(msg, src, TRANSMISSION_DELAY, 0);
    }
    
    console << "  → Sent KEY_CHALLENGE\n";
    console << "  Links in structure: " << securityInfo.linksInStructure << "\n";
}

// SF-5: New module (nM) completes authentication
void AkcpmBlockCode::algorithm1_Complete(P2PNetworkInterface* src, 
                                          uint64_t x1_high, uint64_t x1_low, 
                                          int keyLevel) {
    console << "───────────────────────────────────────────────────────────\n";
    console << "Block " << hostBlock->blockId << ": ALGORITHM 1 - COMPLETE (SF-5)\n";
    console << "───────────────────────────────────────────────────────────\n";
    
    // Retrieve stored n0 for this authentication
    if (pendingN0.find(src) == pendingN0.end()) {
        console << "  ✗ ERROR: No pending authentication for this source\n";
        return;
    }
    
    uint64_t n0 = pendingN0[src];
    uint64_t n2;
    std::vector<uint8_t> sharedKey;
    
    if (keyLevel == 0) {
        // SINGLE_KEY or MULTI_KEY: simple 64-bit x1
        // SF-5: Compute n2 = x1 ⊕ n0
        n2 = XOR(x1_low, n0);
        
        // Compute K1 = H(L(n2 mod Nb))
        sharedKey = HL(n2);
        
        console << "  [SINGLE_KEY/MULTI_KEY Model]\n";
        console << "  n2 = x1 ⊕ n0 = " << n2 << "\n";
        console << "  K1 = H(L(n2 mod Nb)) generated\n";
        
    } else {
        // FOUR_KEYS: 128-bit x1, need to decode key level
        // Article: "nM modules, having added their n0, need to extract n2 and see 
        // how many times to apply H() to find H((n2))"
        
        // x1 = (H^(k-1)(n2) || n2) ⊕ (n0 || n0)
        // So: (H^(k-1)(n2) || n2) = x1 ⊕ (n0 || n0)
        uint64_t decoded_high, decoded_low;
        xor128(x1_high, x1_low, n0, n0, decoded_high, decoded_low);
        
        // decoded_low = n2
        // decoded_high = H^(k-1)(n2)
        n2 = decoded_low;
        uint64_t h_n2_received = decoded_high;
        
        console << "  [FOUR_KEYS Model - Level K" << keyLevel << "]\n";
        console << "  Decoded: n2 = " << n2 << "\n";
        console << "  Decoded: H^(k-1)(n2) = " << h_n2_received << "\n";
        
        // Verify by computing H^(k-1)(n2) ourselves
        uint64_t h_n2_computed = applyHashNTimes(n2, keyLevel - 1);
        if (h_n2_computed != h_n2_received) {
            console << "  ✗ ERROR: H^(k-1)(n2) verification failed!\n";
            console << "    Expected: " << h_n2_computed << "\n";
            console << "    Received: " << h_n2_received << "\n";
            return;
        }
        console << "  ✓ H^(k-1)(n2) verified\n";
        
        // Generate Kx according to key level
        // K1 = H(L(n2 mod Nb))
        // K2 = H(H(K1)), K3 = H(H(K2)), K4 = H(H(K3))
        uint64_t K1_uint64 = H(n2);
        uint64_t Kx = K1_uint64;
        
        for (int i = 1; i < keyLevel; i++) {
            Kx = H(H(Kx));  // Apply H twice for each level
        }
        
        // Convert to vector
        sharedKey.resize(8);
        for (int i = 0; i < 8; i++) {
            sharedKey[i] = (Kx >> (i * 8)) & 0xFF;
        }
        
        console << "  K" << keyLevel << " generated (H applied " << (keyLevel > 1 ? (keyLevel-1)*2 : 0) << " times after K1)\n";
    }
    
    // Store key and update state
    securityInfo.n2 = n2;
    securityInfo.K1 = sharedKey;
    neighborKeys[src] = sharedKey;
    authenticatedNeighbors[src] = true;
    neighborsInStructure[src] = true;
    securityInfo.state = STATE_KEY_ESTABLISHED;
    securityInfo.linksInStructure++;
    
    // For FOUR_KEYS, inherit the key counter from the structure
    if (keyLevel > 0) {
        fourKeysCounter = keyLevel;
    }
    
    // Clean up pending
    pendingN0.erase(src);
    pendingAuthentications.erase(src);
    
    console << "╔═══════════════════════════════════════════════════════════╗\n";
    console << "║  ★★★ AUTHENTICATION SUCCESSFUL ★★★                       ║\n";
    console << "║  Module joined the structure!                            ║\n";
    if (keyLevel > 0) {
        console << "║  Shared key K" << keyLevel << " established (FOUR_KEYS model)        ║\n";
    } else {
        console << "║  Shared key K1 established.                              ║\n";
    }
    console << "╚═══════════════════════════════════════════════════════════╝\n";
    
    // Change color based on key level for visualization
    Catoms3DBlock* catom = (Catoms3DBlock*)hostBlock;
    if (keyLevel == 0 || keyLevel == 1) {
        catom->setColor(GREEN);       // K1 = Green
    } else if (keyLevel == 2) {
        catom->setColor(YELLOW);      // K2 = Yellow
    } else if (keyLevel == 3) {
        catom->setColor(ORANGE);      // K3 = Orange
    } else if (keyLevel == 4) {
        catom->setColor(RED);         // K4 = Red
    }
    
    // Notify other neighbors
    notifyNeighborsStructureReady();
}

// ═══════════════════════════════════════════════════════════════════════════
// ALGORITHM 2: Authentication by Proof of Membership
// Reference: Section VI.C, Algorithm 2 in the article
// Used when two modules already in structure want to connect
// ═══════════════════════════════════════════════════════════════════════════

void AkcpmBlockCode::algorithm2_Initiate(P2PNetworkInterface* targetModule,
                                          P2PNetworkInterface* commonNeighbor) {
    console << "───────────────────────────────────────────────────────────\n";
    console << "Block " << hostBlock->blockId << ": ALGORITHM 2 - INITIATE\n";
    console << "───────────────────────────────────────────────────────────\n";
    
    // This is for Mi-Mj authentication (both already in structure)
    // TODO: Full implementation of Algorithm 2
    console << "  Algorithm 2 (membership proof) - TODO\n";
}

void AkcpmBlockCode::algorithm2_Propagate(P2PNetworkInterface* src, int hops, uint64_t xOrN1) {
    // SF-3: Propagate membership proof
    console << "Algorithm 2 propagate - TODO\n";
}

void AkcpmBlockCode::algorithm2_Complete(P2PNetworkInterface* src, uint64_t x1) {
    // SF-5, SF-6: Complete membership proof
    console << "Algorithm 2 complete - TODO\n";
}

// ═══════════════════════════════════════════════════════════════════════════
// STRUCTURE MANAGEMENT
// ═══════════════════════════════════════════════════════════════════════════

void AkcpmBlockCode::notifyNeighborsStructureReady() {
    Catoms3DBlock* catom = (Catoms3DBlock*)hostBlock;
    
    console << "Block " << hostBlock->blockId << ": Broadcasting STRUCTURE_READY\n";
    
    for (int i = 0; i < 12; i++) {
        P2PNetworkInterface* iface = catom->getInterface(i);
        if (iface && iface->connectedInterface) {
            // Don't notify already authenticated neighbors
            if (authenticatedNeighbors.find(iface) == authenticatedNeighbors.end()) {
                console << "  → Interface " << i << "\n";
                StructureReadyMessage* msg = new StructureReadyMessage();
                sendMessage(msg, iface, 100, 0);
            }
        }
    }
}

void AkcpmBlockCode::checkNewNeighbors() {
    Catoms3DBlock* catom = (Catoms3DBlock*)hostBlock;
    
    for (int i = 0; i < 12; i++) {
        P2PNetworkInterface* iface = catom->getInterface(i);
        if (iface && iface->connectedInterface) {
            // Check for new neighbors not yet in our maps
            if (neighborsInStructure.find(iface) == neighborsInStructure.end() &&
                authenticatedNeighbors.find(iface) == authenticatedNeighbors.end() &&
                pendingAuthentications.find(iface) == pendingAuthentications.end()) {
                
                if (isInStructure()) {
                    // I'm in structure, notify new neighbor
                    console << "Block " << hostBlock->blockId << ": New neighbor detected\n";
                    StructureReadyMessage* msg = new StructureReadyMessage();
                    sendMessage(msg, iface, 100, 0);
                }
            }
        }
    }
}

// ═══════════════════════════════════════════════════════════════════════════
// MESSAGE HANDLERS
// ═══════════════════════════════════════════════════════════════════════════

void AkcpmBlockCode::onAuthRequestReceived(std::shared_ptr<Message> msg, 
                                            P2PNetworkInterface* src) {
    AuthRequestMessage* authMsg = static_cast<AuthRequestMessage*>(msg.get());
    
    console << "Block " << hostBlock->blockId << ": Received AUTH_REQUEST\n";
    console << "  links = " << authMsg->links << "\n";
    
    // Call Algorithm 1 verification
    algorithm1_Verify(src, authMsg->links, authMsg->n1, authMsg->x, authMsg->K0);
}

void AkcpmBlockCode::onKeyChallengeReceived(std::shared_ptr<Message> msg,
                                             P2PNetworkInterface* src) {
    KeyChallengeMessage* keyMsg = static_cast<KeyChallengeMessage*>(msg.get());
    
    console << "Block " << hostBlock->blockId << ": Received KEY_CHALLENGE\n";
    if (keyMsg->keyLevel > 0) {
        console << "  FOUR_KEYS mode, level = " << keyMsg->keyLevel << "\n";
    }
    
    // Call Algorithm 1 completion with all parameters
    algorithm1_Complete(src, keyMsg->x1_high, keyMsg->x1_low, keyMsg->keyLevel);
}

void AkcpmBlockCode::onMembershipProofReceived(std::shared_ptr<Message> msg,
                                                P2PNetworkInterface* src) {
    MembershipProofMessage* proofMsg = static_cast<MembershipProofMessage*>(msg.get());
    
    console << "Block " << hostBlock->blockId << ": Received MEMBERSHIP_PROOF\n";
    
    // Algorithm 2 handling
    algorithm2_Propagate(src, proofMsg->hops, proofMsg->value);
}

void AkcpmBlockCode::onMembershipKeyReceived(std::shared_ptr<Message> msg,
                                              P2PNetworkInterface* src) {
    MembershipKeyMessage* keyMsg = static_cast<MembershipKeyMessage*>(msg.get());
    
    console << "Block " << hostBlock->blockId << ": Received MEMBERSHIP_KEY\n";
    
    algorithm2_Complete(src, keyMsg->x1);
}

void AkcpmBlockCode::onStructureReadyReceived(std::shared_ptr<Message> msg,
                                               P2PNetworkInterface* src) {
    console << "───────────────────────────────────────────────────────────\n";
    console << "Block " << hostBlock->blockId << ": Received STRUCTURE_READY\n";
    
    // Mark this neighbor as being in the structure
    neighborsInStructure[src] = true;
    
    // If I'm not in the structure yet, initiate authentication
    if (!isInStructure()) {
        if (pendingAuthentications.find(src) == pendingAuthentications.end() &&
            authenticatedNeighbors.find(src) == authenticatedNeighbors.end()) {
            
            console << "  → Initiating Algorithm 1 authentication\n";
            algorithm1_Initiate(src);
        }
    } else {
        console << "  Already in structure\n";
    }
}

// ═══════════════════════════════════════════════════════════════════════════
// EVENT HANDLING
// ═══════════════════════════════════════════════════════════════════════════

void AkcpmBlockCode::processLocalEvent(EventPtr pev) {
    MessagePtr message;
    
    switch (pev->eventType) {
        case EVENT_NI_RECEIVE: {
            message = (std::static_pointer_cast<NetworkInterfaceReceiveEvent>(pev))->message;
            P2PNetworkInterface* src = message->destinationInterface;
            
            switch (message->type) {
                case MSG_AUTH_REQUEST:
                    onAuthRequestReceived(message, src);
                    break;
                case MSG_KEY_CHALLENGE:
                    onKeyChallengeReceived(message, src);
                    break;
                case MSG_MEMBERSHIP_PROOF:
                    onMembershipProofReceived(message, src);
                    break;
                case MSG_MEMBERSHIP_KEY:
                    onMembershipKeyReceived(message, src);
                    break;
                case MSG_STRUCTURE_READY:
                    onStructureReadyReceived(message, src);
                    break;
            }
            break;
        }
        
        case EVENT_ADD_NEIGHBOR: {
            checkNewNeighbors();
            break;
        }
        
        case EVENT_INTERRUPTION: {
            // Initial module starts the protocol
            if (securityInfo.isInitialModule) {
                console << "Block " << hostBlock->blockId << ": Starting protocol\n";
                notifyNeighborsStructureReady();
            }
            break;
        }
        
        default:
            break;
    }
}

void AkcpmBlockCode::onMotionEnd() {
    console << "Block " << hostBlock->blockId << ": Motion ended\n";
    checkNewNeighbors();
}

void AkcpmBlockCode::onTap(int face) {
    Catoms3DBlock* catom = (Catoms3DBlock*)hostBlock;
    Cell3DPosition pos = catom->position;
    
    console << "╔═══════════════════════════════════════════════════════════╗\n";
    console << "║ BLOCK " << hostBlock->blockId << " STATUS\n";
    console << "╠═══════════════════════════════════════════════════════════╣\n";
    console << "║ Position: (" << pos[0] << ", " << pos[1] << ", " << pos[2] << ")\n";
    console << "║ State: ";
    switch (securityInfo.state) {
        case STATE_UNLINKED: console << "UNLINKED\n"; break;
        case STATE_AUTHENTICATING: console << "AUTHENTICATING\n"; break;
        case STATE_AUTHENTICATED: console << "AUTHENTICATED\n"; break;
        case STATE_KEY_ESTABLISHED: console << "KEY_ESTABLISHED\n"; break;
    }
    console << "║ Initial Module (iM): " << (securityInfo.isInitialModule ? "YES" : "NO") << "\n";
    console << "║ In Structure: " << (isInStructure() ? "YES" : "NO") << "\n";
    console << "║ Links in Structure: " << securityInfo.linksInStructure << "\n";
    console << "║ Authenticated Neighbors: " << authenticatedNeighbors.size() << "\n";
    console << "║ Key Model: " << (keyModel == SINGLE_KEY ? "SINGLE_KEY" : 
                                   keyModel == FOUR_KEYS ? "FOUR_KEYS" : "MULTI_KEY") << "\n";
    if (keyModel == FOUR_KEYS) {
        console << "║ Next Key Level: K" << fourKeysCounter << "\n";
    }
    console << "╚═══════════════════════════════════════════════════════════╝\n";
}

// ═══════════════════════════════════════════════════════════════════════════
// MESSAGE CLASSES IMPLEMENTATION
// ═══════════════════════════════════════════════════════════════════════════

AuthRequestMessage::AuthRequestMessage(int _links, uint64_t _n1, uint64_t _x,
                                       const std::vector<uint8_t>& _K0)
    : Message(), links(_links), n1(_n1), x(_x), K0(_K0) {
    type = MSG_AUTH_REQUEST;
}
AuthRequestMessage::~AuthRequestMessage() {}

KeyChallengeMessage::KeyChallengeMessage(uint64_t _x1)
    : Message(), x1_high(0), x1_low(_x1), keyLevel(0) {
    type = MSG_KEY_CHALLENGE;
}

KeyChallengeMessage::KeyChallengeMessage(uint64_t _x1_high, uint64_t _x1_low, int _keyLevel)
    : Message(), x1_high(_x1_high), x1_low(_x1_low), keyLevel(_keyLevel) {
    type = MSG_KEY_CHALLENGE;
}

KeyChallengeMessage::~KeyChallengeMessage() {}

MembershipProofMessage::MembershipProofMessage(int _hops, uint64_t _value, bool _isN1)
    : Message(), hops(_hops), value(_value), isN1(_isN1) {
    type = MSG_MEMBERSHIP_PROOF;
}
MembershipProofMessage::~MembershipProofMessage() {}

MembershipKeyMessage::MembershipKeyMessage(uint64_t _x1)
    : Message(), x1(_x1) {
    type = MSG_MEMBERSHIP_KEY;
}
MembershipKeyMessage::~MembershipKeyMessage() {}

StructureReadyMessage::StructureReadyMessage() : Message() {
    type = MSG_STRUCTURE_READY;
}
StructureReadyMessage::~StructureReadyMessage() {}
