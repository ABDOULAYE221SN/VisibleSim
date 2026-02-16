/**
 * @file akcProtocolCode.cpp
 * @brief Implementation of AKC-PM Protocol with SPONGENT-160
 * @author Youssou Faye (adapted for VisibleSim)
 * @date 2025-01-07
 * 
 * This implementation uses SPONGENT-160 for lightweight cryptography
 * to respect the hardware constraints of ATtiny45 modules (8 MHz, 256B RAM, 4KB Flash).
 **/

#include "akcProtocolCode.hpp"
#include <cstring>
#include <cmath>
#include <sstream>
#include <iomanip>

// =================================================================
// CRYPTO UTILITIES IMPLEMENTATION (with SPONGENT-160)
// =================================================================

CryptoUtils::CryptoUtils() {
    std::random_device rd;
    rng.seed(rd());
    initSourceCode();
}

void CryptoUtils::initSourceCode(int nbLines) {
    sourceCode.clear();
    for (int i = 0; i < nbLines; i++) {
        std::stringstream ss;
        ss << "// Line " << std::setfill('0') << std::setw(3) << i 
           << " - AKC-PM Protocol Source Code for Authentication";
        sourceCode.push_back(ss.str());
    }
}

std::vector<uint8_t> CryptoUtils::hash(const std::vector<uint8_t>& data) {
    // Use SPONGENT-160: output is 20 bytes (160 bits)
    return spongent.hash(data);
}

std::vector<uint8_t> CryptoUtils::hash(const std::string& str) {
    // Use SPONGENT-160
    return spongent.hash(str);
}

std::vector<uint8_t> CryptoUtils::hashCodeLine(uint64_t n0) {
    if (sourceCode.empty()) initSourceCode();
    int lineIndex = n0 % sourceCode.size();
    return hash(sourceCode[lineIndex]);
}

std::vector<uint8_t> CryptoUtils::hashIterative(const std::vector<uint8_t>& data, int iterations) {
    std::vector<uint8_t> result = data;
    for (int i = 0; i < iterations; i++) {
        result = hash(result);
    }
    return result;
}

std::vector<uint8_t> CryptoUtils::xorOp(const std::vector<uint8_t>& a, const std::vector<uint8_t>& b) {
    size_t maxSize = std::max(a.size(), b.size());
    std::vector<uint8_t> result(maxSize, 0);
    
    for (size_t i = 0; i < maxSize; i++) {
        uint8_t byteA = (i < a.size()) ? a[i] : 0;
        uint8_t byteB = (i < b.size()) ? b[i] : 0;
        result[i] = byteA ^ byteB;
    }
    return result;
}

std::vector<uint8_t> CryptoUtils::xorOp(uint64_t a, const std::vector<uint8_t>& b) {
    return xorOp(nonceToBytes(a), b);
}

uint64_t CryptoUtils::generateNonce() {
    std::uniform_int_distribution<uint64_t> dist(0, UINT64_MAX);
    return dist(rng);
}

std::vector<uint8_t> CryptoUtils::nonceToBytes(uint64_t nonce) {
    std::vector<uint8_t> bytes(8);
    for (int i = 0; i < 8; i++) {
        bytes[i] = (nonce >> (i * 8)) & 0xFF;
    }
    return bytes;
}

uint64_t CryptoUtils::bytesToNonce(const std::vector<uint8_t>& bytes) {
    uint64_t nonce = 0;
    size_t size = std::min(bytes.size(), size_t(8));
    for (size_t i = 0; i < size; i++) {
        nonce |= (uint64_t(bytes[i]) << (i * 8));
    }
    return nonce;
}

std::string CryptoUtils::bytesToHex(const std::vector<uint8_t>& bytes) {
    std::stringstream ss;
    ss << std::hex << std::setfill('0');
    for (uint8_t byte : bytes) {
        ss << std::setw(2) << static_cast<int>(byte);
    }
    return ss.str();
}

// =================================================================
// AKC PROTOCOL IMPLEMENTATION
// =================================================================

AkcProtocolCode::AkcProtocolCode(Catoms3DBlock *host)
    : Catoms3DBlockCode(host), module(host) {
    
    // @warning Do not remove block below
    if (not host) return;
    
    // Initialize state
    authState = UNLINKED;
    isInitialModule = false;
    linkCount = 0;
    currentKeyModel = SINGLE_KEY;
    currentKeyIndex = 0;
    storedNonce = 0;
    fourKeys.resize(4);
    
    // Initialize metrics
    totalHashOperations = 0;
    totalXorOperations = 0;
    totalAuthAttempts = 0;
    successfulAuths = 0;
    
    // Register message callbacks
    addMessageEventFunc2(AUTH_REQUEST_MSG_ID,
        std::bind(&AkcProtocolCode::handle_AuthRequest, this,
        std::placeholders::_1, std::placeholders::_2));
    
    addMessageEventFunc2(AUTH_CHALLENGE_MSG_ID,
        std::bind(&AkcProtocolCode::handle_AuthChallenge, this,
        std::placeholders::_1, std::placeholders::_2));
    
    addMessageEventFunc2(AUTH_PROOF_MSG_ID,
        std::bind(&AkcProtocolCode::handle_AuthProof, this,
        std::placeholders::_1, std::placeholders::_2));
    
    addMessageEventFunc2(AUTH_PROOF_PROP_MSG_ID,
        std::bind(&AkcProtocolCode::handle_AuthProofProp, this,
        std::placeholders::_1, std::placeholders::_2));
}

void AkcProtocolCode::startup() {
    console << "=== AKC-PM Protocol Started (SPONGENT-160) ===" << "\n";
    console << "Block ID: " << module->blockId << "\n";
    console << "Hash Function: SPONGENT-160 (20 bytes output)" << "\n";
    console << "Key Model: ";
    switch(currentKeyModel) {
        case SINGLE_KEY: console << "SINGLE_KEY"; break;
        case FOUR_KEYS: console << "FOUR_KEYS"; break;
        case MULTI_KEYS: console << "MULTI_KEYS"; break;
    }
    console << "\n";
    
    if (isInitialModule) {
        console << "*** INITIAL MODULE (iM) ***" << "\n";
        setAsInitialModule();
        setColor(RED);
    } else {
        console << "Waiting to authenticate..." << "\n";
        
        // Schedule authentication attempt after delay
        scheduler->schedule(new ActionStartEvent(scheduler->now() + 100000,
            module, [this]() {
                // Try to find an authenticated neighbor
                P2PNetworkInterface *target = module->getInterface(0);
                if (target && target->connectedInterface) {
                    console << "Attempting authentication..." << "\n";
                    initiateStructureFormation(target);
                }
            }));
    }
    
    printStatus();
}

// =================================================================
// ALGORITHM 1: STRUCTURE FORMATION
// =================================================================

void AkcProtocolCode::initiateStructureFormation(P2PNetworkInterface* target) {
    console << "\n=== SF-1: Initiating Structure Formation ===" << "\n";
    totalAuthAttempts++;
    
    // SF-1: Generate parameters
    uint64_t n0 = crypto.generateNonce();
    
    // Hash with SPONGENT-160 (20 bytes output)
    std::vector<uint8_t> n1 = crypto.hash(crypto.nonceToBytes(n0));
    totalHashOperations++;
    
    std::vector<uint8_t> K0 = crypto.hashCodeLine(n0);
    totalHashOperations++;
    
    // Calculate Ts and protect n0
    uint64_t sendTime = scheduler->now();
    uint64_t Ts = calculateTs(sendTime);
    std::vector<uint8_t> x = crypto.xorOp(Ts, crypto.nonceToBytes(n0));
    totalXorOperations++;
    
    console << "n0: " << n0 << "\n";
    console << "Ts: " << Ts << "\n";
    console << "n1 (SPONGENT-160, hex): " << CryptoUtils::bytesToHex(n1).substr(0, 16) << "...\n";
    console << "K0 (SPONGENT-160, hex): " << CryptoUtils::bytesToHex(K0).substr(0, 16) << "...\n";
    
    // Store for later verification
    pendingAuth_n0[target] = n0;
    pendingAuth_sendTime[target] = sendTime;
    
    // SF-2: Create and send authentication request
    AuthRequestData reqData;
    reqData.linkCount = linkCount;
    reqData.n1 = n1;
    reqData.x = x;
    reqData.K0 = K0;
    reqData.sendTime = sendTime;
    
    sendMessage("AUTH_REQUEST", new MessageOf<AuthRequestData>(AUTH_REQUEST_MSG_ID, reqData),
                target, 100, 1000);
    
    authState = AUTHENTICATING;
    console << "Sent AUTH_REQUEST to interface " << target->getConnectedBlockBId() << "\n";
}

void AkcProtocolCode::handle_AuthRequest(std::shared_ptr<Message> _msg, P2PNetworkInterface *sender) {
    console << "\n=== SF-3: Received AUTH_REQUEST ===" << "\n";
    
    if (!isInStructure()) {
        console << "ERROR: Not in structure, cannot authenticate new module" << "\n";
        return;
    }
    
    MessageOf<AuthRequestData>* msg = static_cast<MessageOf<AuthRequestData>*>(_msg.get());
    AuthRequestData reqData = *msg->getData();
    
    console << "From: " << sender->getConnectedBlockBId() << "\n";
    
    // SF-3: Verify time synchronization
    uint64_t receiveTime = scheduler->now();
    uint64_t Ts_prime = calculateTs(receiveTime - TRANSMISSION_DELAY);
    
    // Derive n0' = x ⊕ Ts'
    std::vector<uint8_t> n0_prime_bytes = crypto.xorOp(reqData.x, crypto.nonceToBytes(Ts_prime));
    totalXorOperations++;
    uint64_t n0_prime = crypto.bytesToNonce(n0_prime_bytes);
    
    console << "Derived n0': " << n0_prime << "\n";
    
    // Verify n1 = H(n0') using SPONGENT-160
    if (!verifyTimeSynchronization(reqData.n1, n0_prime)) {
        console << "ERROR: Time synchronization failed!" << "\n";
        console << "Expected n1: " << CryptoUtils::bytesToHex(reqData.n1).substr(0, 16) << "...\n";
        std::vector<uint8_t> computed_n1 = crypto.hash(crypto.nonceToBytes(n0_prime));
        console << "Computed n1: " << CryptoUtils::bytesToHex(computed_n1).substr(0, 16) << "...\n";
        return;
    }
    
    console << "✓ Time synchronization OK (SPONGENT-160)" << "\n";
    
    // Verify K0 = H(L(n0' mod Nb))
    std::vector<uint8_t> K0_verify = crypto.hashCodeLine(n0_prime);
    totalHashOperations++;
    
    if (reqData.K0 != K0_verify) {
        console << "ERROR: Authentication fingerprint mismatch!" << "\n";
        console << "Expected K0: " << CryptoUtils::bytesToHex(reqData.K0).substr(0, 16) << "...\n";
        console << "Computed K0: " << CryptoUtils::bytesToHex(K0_verify).substr(0, 16) << "...\n";
        return;
    }
    
    console << "✓ Authentication fingerprint verified (SPONGENT-160)" << "\n";
    console << ">>> MODULE AUTHENTICATED <<<" << "\n";
    
    // SF-4: Generate key challenge
    uint64_t n2 = crypto.generateNonce();
    std::vector<uint8_t> x1 = crypto.xorOp(n2, n0_prime_bytes);
    totalXorOperations++;
    
    // Generate key according to model
    std::vector<uint8_t> key = generateKey(n2, n0_prime, sender);
    storeKey(key, sender);
    
    console << "Generated key (SPONGENT-160): " << CryptoUtils::bytesToHex(key).substr(0, 16) << "...\n";
    
    int hashIter = calculateHashIterations();
    
    // Send challenge
    AuthChallengeData challengeData;
    challengeData.x1 = x1;
    challengeData.hashIterations = hashIter;
    challengeData.keyModel = currentKeyModel;
    
    sendMessage("AUTH_CHALLENGE", new MessageOf<AuthChallengeData>(AUTH_CHALLENGE_MSG_ID, challengeData),
                sender, 100, 1000);
    
    console << "Sent AUTH_CHALLENGE" << "\n";
    
    // Mark as authenticated
    authenticatedNeighbors.insert(sender);
    linkCount++;
    successfulAuths++;
    
    onAuthenticationComplete(sender);
}

void AkcProtocolCode::handle_AuthChallenge(std::shared_ptr<Message> _msg, P2PNetworkInterface *sender) {
    console << "\n=== SF-5: Received AUTH_CHALLENGE ===" << "\n";
    
    MessageOf<AuthChallengeData>* msg = static_cast<MessageOf<AuthChallengeData>*>(_msg.get());
    AuthChallengeData challengeData = *msg->getData();
    
    // Retrieve stored n0
    if (pendingAuth_n0.find(sender) == pendingAuth_n0.end()) {
        console << "ERROR: No pending authentication for this interface" << "\n";
        return;
    }
    
    uint64_t n0 = pendingAuth_n0[sender];
    std::vector<uint8_t> n0_bytes = crypto.nonceToBytes(n0);
    
    // SF-5: Derive n2 = x1 ⊕ n0
    std::vector<uint8_t> n2_bytes = crypto.xorOp(challengeData.x1, n0_bytes);
    totalXorOperations++;
    uint64_t n2 = crypto.bytesToNonce(n2_bytes);
    
    console << "Derived n2: " << n2 << "\n";
    
    // Generate key using SPONGENT-160
    std::vector<uint8_t> key = generateKey(n2, n0, sender);
    storeKey(key, sender);
    
    console << "Generated key (SPONGENT-160): " << CryptoUtils::bytesToHex(key).substr(0, 16) << "...\n";
    console << ">>> MUTUAL AUTHENTICATION COMPLETE <<<" << "\n";
    
    // Update state
    authState = LINKED;
    authenticatedNeighbors.insert(sender);
    linkCount++;
    storedNonce = n0;
    successfulAuths++;
    
    // Clean up
    pendingAuth_n0.erase(sender);
    pendingAuth_sendTime.erase(sender);
    
    setColor(GREEN);
    onAuthenticationComplete(sender);
}

// =================================================================
// ALGORITHM 2: PROOF OF MEMBERSHIP
// =================================================================

void AkcProtocolCode::initiateProofOfMembership(P2PNetworkInterface* target) {
    console << "\n=== Algorithm 2: Initiating Proof of Membership ===" << "\n";
    
    if (!isInStructure()) {
        console << "ERROR: Not in structure" << "\n";
        return;
    }
    
    // Find an authenticated neighbor (not the target)
    P2PNetworkInterface* authNeighbor = nullptr;
    for (auto neighbor : authenticatedNeighbors) {
        if (neighbor != target) {
            authNeighbor = neighbor;
            break;
        }
    }
    
    if (!authNeighbor) {
        console << "ERROR: No authenticated neighbor for proof propagation" << "\n";
        return;
    }
    
    // Generate n0 and n1 with SPONGENT-160
    uint64_t n0 = crypto.generateNonce();
    std::vector<uint8_t> n1 = crypto.hash(crypto.nonceToBytes(n0));
    totalHashOperations++;
    
    // Get key with authenticated neighbor
    std::vector<uint8_t> key = getKey(authNeighbor);
    std::vector<uint8_t> x = crypto.xorOp(crypto.nonceToBytes(n0), key);
    totalXorOperations++;
    
    // Send proof to target
    AuthProofData proofData;
    proofData.hopsToPropagate = 2;
    proofData.n1 = n1;
    proofData.sourceInterfaceId = authNeighbor->getConnectedBlockBId();
    
    sendMessage("AUTH_PROOF", new MessageOf<AuthProofData>(AUTH_PROOF_MSG_ID, proofData),
                target, 100, 1000);
    
    // Send for propagation
    AuthProofPropData propData;
    propData.hopsRemaining = 2;
    propData.xv = x;
    
    sendMessage("AUTH_PROOF_PROP", new MessageOf<AuthProofPropData>(AUTH_PROOF_PROP_MSG_ID, propData),
                authNeighbor, 100, 1000);
    
    pendingAuth_n0[target] = n0;
    console << "Proof of membership initiated (SPONGENT-160)" << "\n";
}

void AkcProtocolCode::handle_AuthProof(std::shared_ptr<Message> _msg, P2PNetworkInterface *sender) {
    console << "\n=== Received AUTH_PROOF ===" << "\n";
    MessageOf<AuthProofData>* msg = static_cast<MessageOf<AuthProofData>*>(_msg.get());
    AuthProofData proofData = *msg->getData();
    
    console << "Waiting for proof propagation to verify..." << "\n";
}

void AkcProtocolCode::handle_AuthProofProp(std::shared_ptr<Message> _msg, P2PNetworkInterface *sender) {
    console << "\n=== Received AUTH_PROOF_PROPAGATE ===" << "\n";
    
    MessageOf<AuthProofPropData>* msg = static_cast<AuthProofPropData>*>(_msg.get());
    AuthProofPropData propData = *msg->getData();
    
    // Derive n0 = xv ⊕ Kv
    std::vector<uint8_t> key = getKey(sender);
    std::vector<uint8_t> n0_bytes = crypto.xorOp(propData.xv, key);
    totalXorOperations++;
    uint64_t n0 = crypto.bytesToNonce(n0_bytes);
    
    console << "Recovered n0: " << n0 << "\n";
    
    // Propagate if needed
    if (propData.hopsRemaining > 0) {
        propagateProof(n0, sender);
    }
}

void AkcProtocolCode::propagateProof(uint64_t n0, P2PNetworkInterface* excludeInterface) {
    console << "Propagating proof to neighbors..." << "\n";
    
    for (auto neighbor : authenticatedNeighbors) {
        if (neighbor != excludeInterface) {
            std::vector<uint8_t> key = getKey(neighbor);
            std::vector<uint8_t> xv = crypto.xorOp(crypto.nonceToBytes(n0), key);
            totalXorOperations++;
            
            AuthProofPropData propData;
            propData.hopsRemaining = 0;
            propData.xv = xv;
            
            sendMessage("AUTH_PROOF_PROP", 
                       new MessageOf<AuthProofPropData>(AUTH_PROOF_PROP_MSG_ID, propData),
                       neighbor, 100, 1000);
        }
    }
}

// =================================================================
// KEY MANAGEMENT
// =================================================================

std::vector<uint8_t> AkcProtocolCode::generateKey(uint64_t n2, uint64_t n0, P2PNetworkInterface* interface) {
    std::vector<uint8_t> key;
    
    switch(currentKeyModel) {
        case SINGLE_KEY:
            // K1 = H(L(n2 mod Nb)) using SPONGENT-160
            key = crypto.hashCodeLine(n2);
            totalHashOperations++;
            break;
            
        case FOUR_KEYS: {
            // K_i+1 = H^i(K1)
            int keyIndex = std::min(linkCount, 3);
            std::vector<uint8_t> baseKey = crypto.hashCodeLine(n2);
            key = crypto.hashIterative(baseKey, keyIndex);
            totalHashOperations += (1 + keyIndex);
            currentKeyIndex = keyIndex;
            break;
        }
        
        case MULTI_KEYS:
            // K_i+1 = H(L(K_i mod Nb))
            if (linkCount == 0) {
                key = crypto.hashCodeLine(n2);
                totalHashOperations++;
            } else {
                std::vector<uint8_t> K_prev = crypto.hashCodeLine(n2);
                uint64_t K_prev_nonce = crypto.bytesToNonce(K_prev);
                key = crypto.hashCodeLine(K_prev_nonce);
                totalHashOperations += 2;
            }
            break;
    }
    
    return key;
}

void AkcProtocolCode::storeKey(const std::vector<uint8_t>& key, P2PNetworkInterface* interface) {
    switch(currentKeyModel) {
        case SINGLE_KEY:
            singleNetworkKey = key;
            break;
            
        case FOUR_KEYS:
            if (currentKeyIndex < 4) {
                fourKeys[currentKeyIndex] = key;
            }
            break;
            
        case MULTI_KEYS:
            if (interface) {
                pairwiseKeys[interface] = key;
            }
            break;
    }
}

std::vector<uint8_t> AkcProtocolCode::getKey(P2PNetworkInterface* interface) {
    switch(currentKeyModel) {
        case SINGLE_KEY:
            return singleNetworkKey;
            
        case FOUR_KEYS:
            return fourKeys[currentKeyIndex];
            
        case MULTI_KEYS:
            if (interface && pairwiseKeys.find(interface) != pairwiseKeys.end()) {
                return pairwiseKeys[interface];
            }
            break;
    }
    
    return std::vector<uint8_t>();
}

bool AkcProtocolCode::hasKey(P2PNetworkInterface* interface) {
    switch(currentKeyModel) {
        case SINGLE_KEY:
            return !singleNetworkKey.empty();
            
        case FOUR_KEYS:
            return !fourKeys[currentKeyIndex].empty();
            
        case MULTI_KEYS:
            return (interface && pairwiseKeys.find(interface) != pairwiseKeys.end());
    }
    
    return false;
}

int AkcProtocolCode::calculateHashIterations() {
    if (currentKeyModel == FOUR_KEYS) {
        return std::min(linkCount, 3);
    }
    return 0;
}

// =================================================================
// TIME SYNCHRONIZATION
// =================================================================

uint64_t AkcProtocolCode::calculateTs(uint64_t sendTime) {
    // Ts ≈ (ts/10) rounded to nearest integer
    return (uint64_t)std::round((double)sendTime / 10.0);
}

bool AkcProtocolCode::verifyTimeSynchronization(const std::vector<uint8_t>& n1, uint64_t n0_derived) {
    std::vector<uint8_t> n1_computed = crypto.hash(crypto.nonceToBytes(n0_derived));
    totalHashOperations++;
    return (n1 == n1_computed);
}

// =================================================================
// UTILITIES
// =================================================================

void AkcProtocolCode::setAsInitialModule() {
    isInitialModule = true;
    authState = INITIAL_MODULE;
    linkCount = 0;
}

void AkcProtocolCode::onAuthenticationComplete(P2PNetworkInterface* interface) {
    console << "\n";
    console << "╔═══════════════════════════════════════╗" << "\n";
    console << "║   AUTHENTICATION COMPLETE             ║" << "\n";
    console << "╚═══════════════════════════════════════╝" << "\n";
    console << "Link count: " << linkCount << "\n";
    console << "Authenticated neighbors: " << authenticatedNeighbors.size() << "\n";
    printKeys();
    printMetrics();
    printStatus();
}

void AkcProtocolCode::printStatus() {
    console << "\n--- Module Status ---" << "\n";
    console << "ID: " << module->blockId << "\n";
    console << "State: ";
    switch(authState) {
        case UNLINKED: console << "UNLINKED"; break;
        case AUTHENTICATING: console << "AUTHENTICATING"; break;
        case LINKED: console << "LINKED"; break;
        case INITIAL_MODULE: console << "INITIAL_MODULE"; break;
    }
    console << "\n";
    console << "Links: " << linkCount << "\n";
    console << "--------------------" << "\n\n";
}

void AkcProtocolCode::printKeys() {
    console << "--- Keys (SPONGENT-160: 20 bytes) ---" << "\n";
    
    switch(currentKeyModel) {
        case SINGLE_KEY:
            if (!singleNetworkKey.empty()) {
                console << "Network Key: " 
                       << CryptoUtils::bytesToHex(singleNetworkKey).substr(0, 16) 
                       << "... (20 bytes)" << "\n";
            }
            break;
            
        case FOUR_KEYS:
            for (int i = 0; i <= currentKeyIndex && i < 4; i++) {
                if (!fourKeys[i].empty()) {
                    console << "K" << (i+1) << ": " 
                           << CryptoUtils::bytesToHex(fourKeys[i]).substr(0, 16) 
                           << "... (20 bytes)" << "\n";
                }
            }
            break;
            
        case MULTI_KEYS:
            console << "Pairwise keys: " << pairwiseKeys.size() << " (each 20 bytes)" << "\n";
            break;
    }
    
    console << "--------------------------------------" << "\n";
}

void AkcProtocolCode::printMetrics() {
    console << "--- Performance Metrics ---" << "\n";
    console << "Hash operations (SPONGENT-160): " << totalHashOperations << "\n";
    console << "XOR operations: " << totalXorOperations << "\n";
    console << "Authentication attempts: " << totalAuthAttempts << "\n";
    console << "Successful authentications: " << successfulAuths << "\n";
    if (totalAuthAttempts > 0) {
        console << "Success rate: " 
                << (100.0 * successfulAuths / totalAuthAttempts) << "%" << "\n";
    }
    console << "---------------------------" << "\n";
}

void AkcProtocolCode::parseUserBlockElements(TiXmlElement *config) {
    // Check if this module is the initial module (leader)
    const char *attr = config->Attribute("leader");
    if (attr != nullptr) {
        console << "Module " << module->blockId << " is INITIAL MODULE (iM)" << "\n";
        isInitialModule = true;
    }
    
    // Check key model configuration
    const char *keyModelAttr = config->Attribute("keyModel");
    if (keyModelAttr != nullptr) {
        std::string keyModelStr(keyModelAttr);
        if (keyModelStr == "SINGLE_KEY") {
            currentKeyModel = SINGLE_KEY;
        } else if (keyModelStr == "FOUR_KEYS") {
            currentKeyModel = FOUR_KEYS;
        } else if (keyModelStr == "MULTI_KEYS") {
            currentKeyModel = MULTI_KEYS;
        }
        console << "Key model configured: " << keyModelStr << "\n";
    }
}
