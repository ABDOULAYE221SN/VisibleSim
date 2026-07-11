/**
 * @file scupBBCode.cpp
 * @brief S-CUP Protocol Implementation for BlinkyBlocks
 * 
 * Implementation of the S-CUP security protocol for modular robots
 * as described in WINCOM 2025 paper.
 */

#include "scupBBCode.h"
#include "../../simulatorCore/src/spongent160.h"
#include "../../simulatorCore/src/events/scheduler.h"
#include "../../simulatorCore/src/base/world.h"
#include <algorithm>
#include <random>
#include <iomanip>
#include <iostream>

using namespace std;
using namespace BlinkyBlocks;

// =============================================================================
// GLOBAL VARIABLES FOR STATISTICS
// =============================================================================

static set<bID> globalAuthenticatedModules;
static uint64_t globalFormationStartTime = 0;
static uint64_t globalFormationEndTime = 0;
static int globalTotalMessages = 0;

// =============================================================================
// MESSAGE CONSTRUCTORS
// =============================================================================

MessageAuthRequest::MessageAuthRequest(
    const vector<uint8_t>& _n1,
    const vector<uint8_t>& _x,
    const vector<uint8_t>& _K0)
    : Message(), n1(_n1), x(_x), K0(_K0) {
    type = MSG_SCUP_AUTH_REQUEST;
}

MessageKeyChallenge::MessageKeyChallenge(const vector<uint8_t>& _x1)
    : Message(), x1(_x1) {
    type = MSG_SCUP_KEY_CHALLENGE;
}

MessageSkeletonReady::MessageSkeletonReady() : Message() {
    type = MSG_SCUP_SKELETON_READY;
}

MessageClusterReady::MessageClusterReady(int _clusterId)
    : Message(), clusterId(_clusterId) {
    type = MSG_SCUP_CLUSTER_READY;
}

MessageStartFormation::MessageStartFormation() : Message() {
    type = MSG_SCUP_START_FORMATION;
}

// =============================================================================
// CONSTRUCTOR / DESTRUCTOR
// =============================================================================

SCupBBCode::SCupBBCode(BlinkyBlocksBlock *host)
    : BlinkyBlocksBlockCode(host), module(host) {
}

SCupBBCode::~SCupBBCode() {
}

// =============================================================================
// CRYPTOGRAPHIC PRIMITIVES
// =============================================================================

vector<uint8_t> SCupBBCode::H(const vector<uint8_t>& data) {
    return Spongent::Spongent160::hash(data);
}

vector<uint8_t> SCupBBCode::H(uint64_t val) {
    vector<uint8_t> v(8);
    for (int i = 0; i < 8; i++) 
        v[i] = (uint8_t)((val >> (i*8)) & 0xFF);
    return H(v);
}

vector<uint8_t> SCupBBCode::L(int line) {
    vector<uint8_t> result(SCUP_LINE_SIZE_BITS / 8);
    uint64_t seed = 0xDEADBEEFULL ^ ((uint64_t)line * 0x12345678ULL);
    
    for (size_t i = 0; i < result.size(); i++) {
        seed = seed * 1103515245ULL + 12345ULL;
        result[i] = (uint8_t)((seed >> 16) & 0xFF);
    }
    
    return result;
}

vector<uint8_t> SCupBBCode::HL(const vector<uint8_t>& n) {
    uint64_t val = 0;
    for (int i = 0; i < 8 && i < (int)n.size(); i++)
        val |= ((uint64_t)n[i]) << (i*8);
    return HL(val);
}

vector<uint8_t> SCupBBCode::HL(uint64_t n) {
    return H(L((int)(n % SCUP_CODE_LINES)));
}

vector<uint8_t> SCupBBCode::xorVec(const vector<uint8_t>& a,
                                   const vector<uint8_t>& b) {
    vector<uint8_t> result(a.size());
    for (size_t i = 0; i < a.size(); i++)
        result[i] = a[i] ^ b[i % b.size()];
    return result;
}

vector<uint8_t> SCupBBCode::generateNonce160() {
    static random_device rd;
    static mt19937_64 gen(rd());
    uniform_int_distribution<uint64_t> dis;
    
    vector<uint8_t> nonce(SCUP_HASH_SIZE_BYTES);
    for (int i = 0; i < SCUP_HASH_SIZE_BYTES; i += 8) {
        uint64_t v = dis(gen);
        for (int j = 0; j < 8 && i+j < SCUP_HASH_SIZE_BYTES; j++)
            nonce[i+j] = (uint8_t)((v >> (j*8)) & 0xFF);
    }
    
    return nonce;
}

vector<uint8_t> SCupBBCode::timestampToVec(uint64_t ts) {
    uint64_t Ts = ts / SCUP_TIME_DIVISOR;
    vector<uint8_t> v(SCUP_HASH_SIZE_BYTES, 0);
    
    for (int i = 0; i < 8; i++) 
        v[i] = (uint8_t)((Ts >> (i*8)) & 0xFF);
    
    return v;
}

// =============================================================================
// ALGORITHM 1: AUTHENTICATION FOR SKELETON FORMATION
// Conforms to S-CUP paper (WINCOM 2025) - Section VI
// =============================================================================

void SCupBBCode::algorithm1_Initiate(P2PNetworkInterface* dest) {
    if (!isInterfaceValid(dest)) return;
    if (authenticatedNeighbors.count(dest) || ongoingAuthentications.count(dest)) return;
    
    uint64_t ts = scheduler->now();
    
    // SF-1: Generate authentication data
    vector<uint8_t> n0 = generateNonce160();
    vector<uint8_t> n1 = H(n0);
    vector<uint8_t> K0 = HL(n0);
    
    // Protect n0 with Ts
    vector<uint8_t> Ts_vec = timestampToVec(ts);
    vector<uint8_t> x = xorVec(Ts_vec, n0);
    
    console << "[" << ts << "] Module " << module->blockId 
            << " : ALGORITHM 1 - INITIATION (SF-1, SF-2)\n";
    console << "    Role: " << (security.role == ROLE_CH ? "CH" : "CM") << "\n";
    console << "    Phase: SKELETON\n";
    console << "    n0 generated, n1 = H(n0), K0 = HL(n0)\n";
    console << "    Ts = " << (ts / SCUP_TIME_DIVISOR) << ", x = Ts ⊕ n0\n";
    console << "    Sending AUTH_REQUEST\n";
    
    // SF-2: Send message
    sendMessage(new MessageAuthRequest(n1, x, K0), dest, 
                SCUP_TRANSMISSION_DELAY, 0);
    globalTotalMessages++;
    messagesSent++;
    
    // Save n0 for SF-5
    pendingN0[dest] = n0;
    ongoingAuthentications.insert(dest);
    security.state = STATE_AUTHENTICATING;
}

void SCupBBCode::algorithm1_Verify(
    P2PNetworkInterface* src,
    const vector<uint8_t>& n1,
    const vector<uint8_t>& x,
    const vector<uint8_t>& K0) {
    
    if (authenticatedNeighbors.count(src)) return;
    if (!isInSkeleton()) {
        console << "[SF-3] Module " << module->blockId 
                << " REJECTED: not in skeleton\n";
        return;
    }
    
    uint64_t Tr = scheduler->now();
    console << "[" << Tr << "] Module " << module->blockId 
            << " : ALGORITHM 1 - VERIFICATION (SF-3)\n";
    console << "    Role: " << (security.role == ROLE_ICH ? "iCH" : "CH") << "\n";
    console << "    Already in skeleton\n";
    
    // SF-3: Recover n0' with synchronization tolerance
    uint64_t Ts_base = (Tr - SCUP_TRANSMISSION_DELAY) / SCUP_TIME_DIVISOR;
    vector<uint8_t> n0_prime;
    bool syncOK = false;
    int offset_found = 0;
    
    for (int off = -SCUP_TIME_TOLERANCE; off <= SCUP_TIME_TOLERANCE && !syncOK; off++) {
        uint64_t Ts_test = Ts_base + (uint64_t)off;
        vector<uint8_t> Ts_vec(SCUP_HASH_SIZE_BYTES, 0);
        for (int i = 0; i < 8; i++)
            Ts_vec[i] = (uint8_t)((Ts_test >> (i * 8)) & 0xFF);
        
        vector<uint8_t> n0_test = xorVec(x, Ts_vec);
        
        if (H(n0_test) == n1) {
            n0_prime = n0_test;
            syncOK = true;
            offset_found = off;
        }
    }
    
    if (!syncOK) {
        console << "    FAILURE: Synchronization impossible (Ts out of tolerance)\n";
        return;
    }
    
    console << "    n0' = x ⊕ Ts' (offset=" << offset_found << ")\n";
    console << "    Ts_base = (Tr - Δt)/10 = " << Ts_base << "\n";
    
    // Verify K0 = H(L(n0' mod Nb)) — proof of same code
    if (HL(n0_prime) != K0) {
        console << "    FAILURE: K0 verification (different code)\n";
        return;
    }
    
    console << "    VERIFICATION SUCCESSFUL\n";
    console << "    - Synchronization OK\n";
    console << "    - Identical code (K0 verified)\n";
    
    // SF-4: Key generation
    vector<uint8_t> n2;
    
    if (SCUP_FORMATION_MODEL == SERIAL_MODEL) {
        // Serial model: reuse n2 to have a single key
        if (security.n2.empty()) {
            // iCH generates n2 for the first time
            n2 = generateNonce160();
            security.n2 = n2;
            console << "    iCH generates n2 (SERIAL_MODEL)\n";
        } else {
            // CH reuses n2
            n2 = security.n2;
            console << "    CH reuses n2 (SERIAL_MODEL)\n";
        }
    } else {
        // Parallel model: generate new n2 for each pair
        n2 = generateNonce160();
        console << "    Generate new n2 (PARALLEL_MODEL)\n";
    }
    
    vector<uint8_t> x1 = xorVec(n2, n0_prime);
    vector<uint8_t> K1 = HL(n2);
    
    // Save the key
    neighborKeys[src] = K1;
    authenticatedNeighbors[src] = true;
    
    console << "    K1 = HL(n2 mod Nb) generated\n";
    console << "    Shared key K1 established\n";
    console << "    Sending KEY_CHALLENGE (x1 = n2 ⊕ n0')\n";
    
    // Send x1
    sendMessage(new MessageKeyChallenge(x1), src, SCUP_TRANSMISSION_DELAY, 0);
    globalTotalMessages++;
    messagesSent++;
}

void SCupBBCode::algorithm1_Complete(
    P2PNetworkInterface* src,
    const vector<uint8_t>& x1) {
    
    if (authenticatedNeighbors.count(src)) return;
    if (!pendingN0.count(src)) {
        console << "[SF-5] Module " << module->blockId 
                << " ERROR: n0 not found\n";
        return;
    }
    
    uint64_t t = scheduler->now();
    console << "[" << t << "] Module " << module->blockId 
            << " : ALGORITHM 1 - COMPLETION (SF-5)\n";
    
    // Recover n0
    vector<uint8_t> n0 = pendingN0[src];
    
    // SF-5: Calculate n2 and K1
    vector<uint8_t> n2 = xorVec(x1, n0);
    vector<uint8_t> K1 = HL(n2);
    
    console << "    n2 = x1 ⊕ n0\n";
    console << "    K1 = HL(n2 mod Nb)\n";
    
    // Save
    security.n2 = n2;
    security.K1 = K1;
    neighborKeys[src] = K1;
    authenticatedNeighbors[src] = true;
    security.state = STATE_KEY_ESTABLISHED;
    
    // Cleanup
    pendingN0.erase(src);
    ongoingAuthentications.erase(src);
    
    console << "    AUTHENTICATION SUCCESSFUL\n";
    console << "    Shared key K1 established\n";
    
    if (SCUP_FORMATION_MODEL == SERIAL_MODEL) {
        console << "    This K1 is identical for entire skeleton (SERIAL_MODEL)\n";
    } else {
        console << "    This K1 is unique for this pair (PARALLEL_MODEL)\n";
    }
    
    // Apply cluster color
    applyClusterColor();
    
    // Register as authenticated
    if (!globalAuthenticatedModules.count(module->blockId)) {
        globalAuthenticatedModules.insert(module->blockId);
        successfulAuthentications++;
        
        int total = (int)BaseSimulator::World::getWorld()->buildingBlocksMap.size();
        console << "    [" << globalAuthenticatedModules.size() << "/" << total
                << "] modules authenticated\n";
    }
    
    // Notify neighbors
    notifyNeighborsSkeletonReady();
    
    // If all authenticated, move to next phase
    int total = (int)BaseSimulator::World::getWorld()->buildingBlocksMap.size();
    if ((int)globalAuthenticatedModules.size() >= total) {
        globalFormationEndTime = scheduler->now();
        displayGlobalStatistics();
        startOperationalPhase();
    }
}

// =============================================================================
// UTILITY FUNCTIONS
// =============================================================================

bool SCupBBCode::isCH() const {
    return security.role == ROLE_CH || security.role == ROLE_ICH;
}

bool SCupBBCode::isICH() const {
    return security.role == ROLE_ICH;
}

bool SCupBBCode::isCM() const {
    return security.role == ROLE_CM;
}

bool SCupBBCode::isInSkeleton() const {
    return isICH() || 
           security.state == STATE_AUTHENTICATED || 
           security.state == STATE_KEY_ESTABLISHED;
}

void SCupBBCode::setAsICH() {
    security.role = ROLE_ICH;
    security.phase = PHASE_SKELETON;
    security.state = STATE_AUTHENTICATED;
    
    applyClusterColor();
    
    console << "[iCH] Module " << module->blockId 
            << " : Initial Cluster Head\n";
    console << "    Position: (" << module->position[0] << "," 
            << module->position[1] << "," << module->position[2] << ")\n";
    console << "    Model: S-CUP (" 
            << (SCUP_FORMATION_MODEL == SERIAL_MODEL ? "SERIAL" : "PARALLEL") << ")\n";
    console << "    Role: Authenticate CHs to form skeleton\n";
}

void SCupBBCode::setAsCH() {
    security.role = ROLE_CH;
    security.phase = PHASE_WAITING;
    module->setColor(GREY);
    
    console << "[CH] Module " << module->blockId 
            << " : Cluster Head waiting\n";
}

void SCupBBCode::setAsCM() {
    security.role = ROLE_CM;
    security.phase = PHASE_WAITING;
    module->setColor(SCUP_CM_COLOR);
    
    console << "[CM] Module " << module->blockId 
            << " : Cluster Member waiting\n";
}

// =============================================================================
// CLUSTER MANAGEMENT
// =============================================================================

int SCupBBCode::determineClusterId() const {
    // Determine cluster based on position (cross/T shape)
    int x = module->position[0];
    int y = module->position[1];
    
    // Cluster 1 (ORANGE) - Bras haut: x=5, y∈[6,8]
    if (x == 5 && y >= 6 && y <= 8) return 1;
    
    // Cluster 2 (BLUE) - Bras gauche: y=5, x∈[2,4]
    else if (y == 5 && x >= 2 && x <= 4) return 2;
    
    // Cluster 3 (RED) - Centre: x=5, y=5
    else if (x == 5 && y == 5) return 3;
    
    // Cluster 4 (CYAN) - Bras bas + droit: (x=5, y∈[0,4]) OU (y=5, x∈[6,8])
    else if ((x == 5 && y >= 0 && y <= 4) || (y == 5 && x >= 6 && x <= 8)) return 4;
    
    return -1;
}

Color SCupBBCode::determineClusterColor() const {
    switch (security.clusterId) {
        case 1:  return SCUP_CLUSTER_1_COLOR;
        case 2:  return SCUP_CLUSTER_2_COLOR;
        case 3:  return SCUP_CLUSTER_3_COLOR;
        default: return GREY;
    }
}

void SCupBBCode::applyClusterColor() {
    Color color = determineClusterColor();
    module->setColor(color);
    
    const char* colorName = "UNKNOWN";
    switch (security.clusterId) {
        case 1: colorName = "RED"; break;
        case 2: colorName = "GREEN"; break;
        case 3: colorName = "BLUE"; break;
    }
    
    console << "[Color] Module " << module->blockId 
            << " : Cluster " << security.clusterId 
            << " - " << colorName << "\n";
}

// =============================================================================
// FORMATION MANAGEMENT
// =============================================================================

void SCupBBCode::startSkeletonFormation() {
    console << "[Formation] Module " << module->blockId 
            << " : Starting skeleton formation\n";
    
    security.phase = PHASE_SKELETON;
    
    if (SCUP_FORMATION_MODEL == SERIAL_MODEL) {
        console << "    Model: SERIAL (sequential formation, 1 key)\n";
    } else {
        console << "    Model: PARALLEL (parallel formation, keys per pair)\n";
    }
    
    // Authenticate neighbors (BlinkyBlocks has 6 interfaces: 0-5)
    for (int i = 0; i < 6; i++) {
        P2PNetworkInterface* iface = module->getInterface(i);
        if (isInterfaceValid(iface)) {
            algorithm1_Initiate(iface);
        }
    }
}

void SCupBBCode::startClusterFormation() {
    console << "[Formation] Module " << module->blockId 
            << " : Starting cluster formation\n";
    
    security.phase = PHASE_CLUSTERING;
    
    // TODO: Implement cluster formation
    // For now, move directly to operational phase
    startOperationalPhase();
}

void SCupBBCode::startOperationalPhase() {
    console << "[Operation] Module " << module->blockId 
            << " : Structure formed, operational phase\n";
    
    security.phase = PHASE_OPERATIONAL;
    module->setColor(GREEN);
}

// =============================================================================
// NOTIFICATIONS
// =============================================================================

void SCupBBCode::notifyNeighborsFormationStart() {
    uint64_t t = scheduler->now();
    int notifications = 0;
    
    for (int i = 0; i < 6; i++) {
        P2PNetworkInterface* iface = module->getInterface(i);
        if (!isInterfaceValid(iface)) continue;
        
        try {
            sendMessage(new MessageStartFormation(), iface, 
                       SCUP_TRANSMISSION_DELAY, 0);
            globalTotalMessages++;
            messagesSent++;
            notifications++;
        } catch (...) {
            // Ignore errors
        }
    }
    
    if (notifications > 0) {
        console << "[" << t << "] Module " << module->blockId 
                << " : Broadcasting START_FORMATION (" << notifications << " neighbors)\n";
    }
}

void SCupBBCode::notifyNeighborsSkeletonReady() {
    uint64_t t = scheduler->now();
    int notifications = 0;
    
    for (int i = 0; i < 6; i++) {
        P2PNetworkInterface* iface = module->getInterface(i);
        if (!isInterfaceValid(iface)) continue;
        if (authenticatedNeighbors.count(iface) || 
            ongoingAuthentications.count(iface)) continue;
        
        ongoingAuthentications.insert(iface);
        try {
            sendMessage(new MessageSkeletonReady(), iface, 
                       SCUP_TRANSMISSION_DELAY, 0);
            globalTotalMessages++;
            messagesSent++;
            notifications++;
        } catch (...) {
            ongoingAuthentications.erase(iface);
        }
    }
    
    if (notifications > 0) {
        console << "[" << t << "] Module " << module->blockId 
                << " : Broadcasting SKELETON_READY (" << notifications << " neighbors)\n";
    }
}

void SCupBBCode::notifyNeighborsClusterReady() {
    uint64_t t = scheduler->now();
    int notifications = 0;
    
    for (int i = 0; i < 6; i++) {
        P2PNetworkInterface* iface = module->getInterface(i);
        if (!isInterfaceValid(iface)) continue;
        
        try {
            sendMessage(new MessageClusterReady(security.clusterId), 
                       iface, SCUP_TRANSMISSION_DELAY, 0);
            globalTotalMessages++;
            messagesSent++;
            notifications++;
        } catch (...) {
            // Ignore errors
        }
    }
    
    if (notifications > 0) {
        console << "[" << t << "] Module " << module->blockId 
                << " : Broadcasting CLUSTER_READY (" << notifications << " neighbors)\n";
    }
}

// =============================================================================
// STATISTICS
// =============================================================================

void SCupBBCode::displayStatistics() const {
    if (!isICH()) return;
    displayGlobalStatistics();
}

void SCupBBCode::displayGlobalStatistics() const {
    uint64_t duration = (globalFormationEndTime > globalFormationStartTime) ? 
                        (globalFormationEndTime - globalFormationStartTime) : 0;
    int totalModules = (int)BaseSimulator::World::getWorld()->buildingBlocksMap.size();
    
    cout << "\n" << flush;
    cout << "╔════════════════════════════════════════════════════════════════╗" << endl;
    cout << "║         S-CUP STATISTICS (WINCOM 2025)                        ║" << endl;
    cout << "╠════════════════════════════════════════════════════════════════╣" << endl;
    cout << "║ Formation model      : " 
         << (SCUP_FORMATION_MODEL == SERIAL_MODEL ? "SERIAL (1 key)" : "PARALLEL (keys/pair)") 
         << "                  ║" << endl;
    cout << "╠════════════════════════════════════════════════════════════════╣" << endl;
    cout << "║ Authenticated modules: " << setw(3) << globalAuthenticatedModules.size() 
         << " / " << setw(3) << totalModules << "                                ║" << endl;
    cout << "║ Messages exchanged   : " << setw(6) << globalTotalMessages 
         << "                                  ║" << endl;
    cout << "║ Formation time (µs)  : " << setw(10) << duration 
         << "                            ║" << endl;
    
    double msgPerModule = totalModules > 0 ? (double)globalTotalMessages / totalModules : 0;
    cout << "║ Msg per module (avg) : " << fixed << setprecision(2) << setw(6) << msgPerModule 
         << "                                  ║" << endl;
    
    double timePerModule = totalModules > 0 ? (double)duration / totalModules : 0;
    cout << "║ Time per module (µs) : " << fixed << setprecision(2) << setw(10) << timePerModule 
         << "                            ║" << endl;
    
    cout << "╠════════════════════════════════════════════════════════════════╣" << endl;
    cout << "║ Algo1 complexity     : O(w) — w links                         ║" << endl;
    cout << "║ Overhead per link    : 6H + 2Tx + 4XOR                        ║" << endl;
    cout << "╚════════════════════════════════════════════════════════════════╝" << endl;
    cout << "\n" << flush;
}

// =============================================================================
// MESSAGE HANDLERS
// =============================================================================

void SCupBBCode::onAuthRequestReceived(MessagePtr msg, P2PNetworkInterface* src) {
    auto* m = static_cast<MessageAuthRequest*>(msg.get());
    messagesReceived++;
    algorithm1_Verify(src, m->n1, m->x, m->K0);
}

void SCupBBCode::onKeyChallengeReceived(MessagePtr msg, P2PNetworkInterface* src) {
    auto* m = static_cast<MessageKeyChallenge*>(msg.get());
    messagesReceived++;
    algorithm1_Complete(src, m->x1);
}

void SCupBBCode::onSkeletonReadyReceived(MessagePtr /*msg*/, P2PNetworkInterface* src) {
    messagesReceived++;
    
    if (authenticatedNeighbors.count(src) || ongoingAuthentications.count(src)) return;
    
    uint64_t t = scheduler->now();
    console << "[" << t << "] Module " << module->blockId 
            << " : Received SKELETON_READY\n";
    
    if (!isInSkeleton()) {
        console << "    Launching ALGORITHM 1 (authentication)\n";
        algorithm1_Initiate(src);
    }
}

void SCupBBCode::onClusterReadyReceived(MessagePtr msg, P2PNetworkInterface* /*src*/) {
    auto* m = static_cast<MessageClusterReady*>(msg.get());
    messagesReceived++;
    
    console << "[Cluster] Module " << module->blockId 
            << " : Received CLUSTER_READY (cluster " << m->clusterId << ")\n";
}

void SCupBBCode::onStartFormationReceived(MessagePtr /*msg*/, P2PNetworkInterface* /*src*/) {
    messagesReceived++;
    
    uint64_t t = scheduler->now();
    console << "[" << t << "] Module " << module->blockId 
            << " : Received START_FORMATION\n";
    
    if (isCH() && security.phase == PHASE_WAITING) {
        startSkeletonFormation();
    }
}

// =============================================================================
// HELPER FUNCTIONS
// =============================================================================

bool SCupBBCode::isInterfaceValid(P2PNetworkInterface* iface) const {
    if (!iface || !iface->connectedInterface) return false;
    if (!iface->connectedInterface->hostBlock) return false;
    if (iface->connectedInterface->hostBlock == module) return false;
    if (iface->connectedInterface->hostBlock->blockId == module->blockId) return false;
    if (iface->connectedInterface->hostBlock->position == module->position) return false;
    return true;
}

// =============================================================================
// STARTUP
// =============================================================================

void SCupBBCode::startup() {
    console << "=== S-CUP Protocol Initialization ===\n";
    console << "Module " << module->blockId << " starting up\n";
    console << "Position: (" << module->position[0] << "," 
            << module->position[1] << "," << module->position[2] << ")\n";
    
    // Determine cluster based on position
    security.clusterId = determineClusterId();
    
    // Determine role based on blockId and position
    // Cup shape topology:
    // - Module 1 (6,7,0) : iCH - Initial Cluster Head (Cluster 1 - Base)
    // - Module 2 (6,8,0) : CH1 - Cluster Head 1 (Cluster 1 - Base)
    // - Module 3 (3,9,0) : CH2 - Cluster Head 2 (Cluster 2 - Left wall)
    // - Module 4 (9,9,0) : CH3 - Cluster Head 3 (Cluster 3 - Right wall)
    
    Cell3DPosition pos = module->position;
    
    bool isCH_position = (module->blockId == 1 ||  // iCH at center base
                         module->blockId == 2 ||  // CH1 at top of base
                         module->blockId == 3 ||  // CH2 at left wall
                         module->blockId == 4);   // CH3 at right wall
    
    if (isCH_position) {
        // This module is a CH
        if (module->blockId == 1) {
            // Module 1 is the iCH (Initial Cluster Head)
            setAsICH();
            globalAuthenticatedModules.insert(module->blockId);
            globalFormationStartTime = scheduler->now();
            
            const char* clusterName = "Base (RED)";
            console << "    Cluster ID: " << security.clusterId << " - " << clusterName << "\n";
            console << "    Cup Shape: Center of the base\n";
            
            // Start formation after all modules have started
            scheduler->schedule(
                new InterruptionEvent<unsigned int>(scheduler->now() + 100, 
                                                   module, 0));
        } else {
            // Modules 2, 3, 4 are CHs
            setAsCH();
            
            const char* clusterName = "UNKNOWN";
            const char* position = "UNKNOWN";
            
            if (module->blockId == 2) {
                clusterName = "Base (RED)";
                position = "Top of base";
            } else if (module->blockId == 3) {
                clusterName = "Left Wall (GREEN)";
                position = "Left wall of cup";
            } else if (module->blockId == 4) {
                clusterName = "Right Wall (BLUE)";
                position = "Right wall of cup";
            }
            
            console << "    Cluster ID: " << security.clusterId << " - " << clusterName << "\n";
            console << "    Cup Shape: " << position << "\n";
            console << "    Waiting for skeleton formation...\n";
            
            // Apply cluster color
            applyClusterColor();
        }
    } else {
        // This module is a CM (Cluster Member)
        setAsCM();
        
        const char* clusterName = "UNKNOWN";
        const char* position = "UNKNOWN";
        
        if (security.clusterId == 1) {
            clusterName = "Base (RED)";
            position = "Base of cup";
        } else if (security.clusterId == 2) {
            clusterName = "Left Wall (GREEN)";
            position = "Left wall of cup";
        } else if (security.clusterId == 3) {
            clusterName = "Right Wall (BLUE)";
            position = "Right wall of cup";
        }
        
        console << "    Cluster ID: " << security.clusterId << " - " << clusterName << "\n";
        console << "    Cup Shape: " << position << "\n";
        console << "    Waiting for cluster formation...\n";
        
        // Apply cluster color
        applyClusterColor();
    }
}

// =============================================================================
// EVENT PROCESSING
// =============================================================================

void SCupBBCode::processLocalEvent(EventPtr pev) {
    BlinkyBlocksBlockCode::processLocalEvent(pev);
    
    switch (pev->eventType) {
        case EVENT_NI_RECEIVE: {
            auto msg = static_pointer_cast<NetworkInterfaceReceiveEvent>(pev)->message;
            P2PNetworkInterface* src = msg->destinationInterface;
            
            switch (msg->type) {
                case MSG_SCUP_AUTH_REQUEST:
                    onAuthRequestReceived(msg, src);
                    break;
                case MSG_SCUP_KEY_CHALLENGE:
                    onKeyChallengeReceived(msg, src);
                    break;
                case MSG_SCUP_SKELETON_READY:
                    onSkeletonReadyReceived(msg, src);
                    break;
                case MSG_SCUP_CLUSTER_READY:
                    onClusterReadyReceived(msg, src);
                    break;
                case MSG_SCUP_START_FORMATION:
                    onStartFormationReceived(msg, src);
                    break;
            }
            break;
        }
        case EVENT_INTERRUPTION: {
            auto id = static_pointer_cast<InterruptionEvent<unsigned int>>(pev)->data;
            if (id == 0 && isICH()) {
                // Initial broadcast of START_FORMATION
                notifyNeighborsFormationStart();
                // Start skeleton formation
                startSkeletonFormation();
            }
            break;
        }
        default:
            break;
    }
}

void SCupBBCode::parseUserBlockElements(TiXmlElement* /*config*/) {
    // Parse user-defined configuration if needed
}
