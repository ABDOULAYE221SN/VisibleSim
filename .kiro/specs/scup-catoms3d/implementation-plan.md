# S-Cup Protocol Implementation Plan for Catoms3D

## Phase 1: Project Setup and Core Infrastructure

### 1.1 Create Project Structure
```
applicationsSrc/scupCatoms3D/
├── scupCatoms3D.cpp          # Main entry point
├── SCupCatoms3DCode.h        # Header file
├── SCupCatoms3DCode.cpp      # Implementation
└── Makefile                  # Build configuration

applicationsBin/scupCatoms3D/
├── config.xml                # Simulation configuration
└── README.md                 # Documentation
```

### 1.2 Define Constants and Parameters
Based on S-Cup paper specifications:
- SCUP_CODE_LINES = 128
- SCUP_LINE_SIZE_BITS = 256
- SCUP_HASH_SIZE_BYTES = 20 (SPONGENT-160)
- SCUP_TIME_DIVISOR = 10
- SCUP_TIME_TOLERANCE = 5
- SCUP_TRANSMISSION_DELAY = 100 µs

### 1.3 Define Enumerations
- ModuleRole: ROLE_UNDEFINED, ROLE_ICH, ROLE_CH, ROLE_CM
- ProtocolPhase: PHASE_WAITING, PHASE_SKELETON, PHASE_CLUSTERING, PHASE_OPERATIONAL
- SecurityState: STATE_UNLINKED, STATE_AUTHENTICATING, STATE_AUTHENTICATED, STATE_KEY_ESTABLISHED
- FormationModel: SERIAL_MODEL, PARALLEL_MODEL

## Phase 2: Cryptographic Primitives Implementation

### 2.1 Hash Function H()

**Purpose**: SPONGENT-160 lightweight hash function
**Input**: vector<uint8_t> data (arbitrary length)
**Output**: vector<uint8_t> hash (20 bytes = 160 bits)
**Implementation**: Use existing Spongent::Spongent160::hash() from simulatorCore

```cpp
vector<uint8_t> H(const vector<uint8_t>& data);
vector<uint8_t> H(uint64_t val);  // Overload for integers
```

### 2.2 Source Code Line Function L(i)
**Purpose**: Generate 256-bit representation of code line i
**Input**: int line (0 to Nb-1)
**Output**: vector<uint8_t> (32 bytes = 256 bits)
**Implementation**: Deterministic pseudo-random generation based on line number

```cpp
vector<uint8_t> L(int line);
```

### 2.3 Composite Hash Function HL(n)
**Purpose**: H(L(n mod Nb)) - hash of code line
**Input**: vector<uint8_t> n or uint64_t n
**Output**: vector<uint8_t> (20 bytes = 160 bits)

```cpp
vector<uint8_t> HL(const vector<uint8_t>& n);
vector<uint8_t> HL(uint64_t n);
```

### 2.4 XOR Operation
**Purpose**: Bitwise XOR for nonce protection and key exchange
**Input**: Two vector<uint8_t> of same size
**Output**: vector<uint8_t> result

```cpp
vector<uint8_t> xorVec(const vector<uint8_t>& a, const vector<uint8_t>& b);
```

### 2.5 Nonce Generation
**Purpose**: Generate cryptographically random 160-bit nonce
**Output**: vector<uint8_t> (20 bytes)

```cpp
vector<uint8_t> generateNonce160();
```

### 2.6 Timestamp Function
**Purpose**: Convert simulation time to protocol timestamp with tolerance
**Input**: uint64_t ts (simulation time in µs)
**Output**: vector<uint8_t> Ts (20 bytes, only first 8 bytes used)
**Formula**: Ts ≈ (ts / 10)

```cpp
vector<uint8_t> timestampToVec(uint64_t ts);
```

## Phase 3: Data Structures

### 3.1 SecurityInfo Structure
```cpp
struct SecurityInfo {
    ModuleRole role;
    ProtocolPhase phase;
    SecurityState state;
    int clusterId;
    
    // Nonces (160 bits each)
    vector<uint8_t> n0;  // Secret, never transmitted
    vector<uint8_t> n1;  // H(n0)
    vector<uint8_t> n2;  // Key generation nonce
    
    // Keys (160 bits each)
    vector<uint8_t> K0;  // Code proof
    vector<uint8_t> K1;  // Shared key
};
```

### 3.2 Message Classes

#### MessageAuthRequest (SF-2)
```cpp
class MessageAuthRequest : public Message {
public:
    vector<uint8_t> n1;   // 160 bits
    vector<uint8_t> x;    // 160 bits
    vector<uint8_t> K0;   // 160 bits
};
```

#### MessageKeyChallenge (SF-4)
```cpp
class MessageKeyChallenge : public Message {
public:
    vector<uint8_t> x1;   // 160 bits
};
```

#### MessageSkeletonReady
```cpp
class MessageSkeletonReady : public Message {
    // No payload
};
```

#### MessageClusterReady
```cpp
class MessageClusterReady : public Message {
public:
    int clusterId;
};
```

#### MessageStartFormation
```cpp
class MessageStartFormation : public Message {
    // No payload
};
```

### 3.3 Main Class Structure
```cpp
class SCupCatoms3DCode : public Catoms3DBlockCode {
private:
    Catoms3DBlock* module;
    SecurityInfo security;
    
    // Key management
    map<P2PNetworkInterface*, vector<uint8_t>> neighborKeys;
    map<P2PNetworkInterface*, bool> authenticatedNeighbors;
    set<P2PNetworkInterface*> ongoingAuthentications;
    map<P2PNetworkInterface*, vector<uint8_t>> pendingN0;
    
    // Statistics
    int messagesSent;
    int messagesReceived;
    int successfulAuthentications;
    
public:
    // Lifecycle
    void startup() override;
    void processLocalEvent(EventPtr pev) override;
    
    // Cryptographic primitives
    vector<uint8_t> H(const vector<uint8_t>& data);
    vector<uint8_t> L(int line);
    vector<uint8_t> HL(const vector<uint8_t>& n);
    vector<uint8_t> xorVec(const vector<uint8_t>& a, const vector<uint8_t>& b);
    vector<uint8_t> generateNonce160();
    vector<uint8_t> timestampToVec(uint64_t ts);
    
    // Algorithm 1: Skeleton Formation
    void algorithm1_Initiate(P2PNetworkInterface* dest);
    void algorithm1_Verify(P2PNetworkInterface* src, 
                          const vector<uint8_t>& n1,
                          const vector<uint8_t>& x,
                          const vector<uint8_t>& K0);
    void algorithm1_Complete(P2PNetworkInterface* src,
                            const vector<uint8_t>& x1);
    
    // Phase management
    void startSkeletonFormation();
    void startClusterFormation();
    void startOperationalPhase();
    
    // Message handlers
    void onAuthRequestReceived(MessagePtr msg, P2PNetworkInterface* src);
    void onKeyChallengeReceived(MessagePtr msg, P2PNetworkInterface* src);
    void onSkeletonReadyReceived(MessagePtr msg, P2PNetworkInterface* src);
    void onClusterReadyReceived(MessagePtr msg, P2PNetworkInterface* src);
    void onStartFormationReceived(MessagePtr msg, P2PNetworkInterface* src);
    
    // Utilities
    bool isInterfaceValid(P2PNetworkInterface* iface) const;
    bool isCH() const;
    bool isICH() const;
    bool isCM() const;
    bool isInSkeleton() const;
};
```

## Phase 4: Algorithm 1 Implementation (Skeleton Formation)

### 4.1 SF-1 & SF-2: Initiate Authentication (CHn side)
```cpp
void algorithm1_Initiate(P2PNetworkInterface* dest) {
    // Check if already authenticated or in progress
    if (authenticatedNeighbors.count(dest) || 
        ongoingAuthentications.count(dest)) return;
    
    uint64_t ts = scheduler->now();
    
    // SF-1: Generate authentication data
    vector<uint8_t> n0 = generateNonce160();
    vector<uint8_t> n1 = H(n0);
    vector<uint8_t> K0 = HL(n0);
    
    // Protect n0 with timestamp
    vector<uint8_t> Ts_vec = timestampToVec(ts);
    vector<uint8_t> x = xorVec(Ts_vec, n0);
    
    // SF-2: Send authentication request
    sendMessage(new MessageAuthRequest(n1, x, K0), dest, 
                SCUP_TRANSMISSION_DELAY, 0);
    
    // Save n0 for SF-5
    pendingN0[dest] = n0;
    ongoingAuthentications.insert(dest);
    security.state = STATE_AUTHENTICATING;
}
```

### 4.2 SF-3 & SF-4: Verify and Challenge (iCH side)
```cpp
void algorithm1_Verify(P2PNetworkInterface* src,
                      const vector<uint8_t>& n1,
                      const vector<uint8_t>& x,
                      const vector<uint8_t>& K0) {
    // Check if already authenticated
    if (authenticatedNeighbors.count(src)) return;
    
    // Check if in skeleton
    if (!isInSkeleton()) return;
    
    uint64_t Tr = scheduler->now();
    
    // SF-3: Recover n0' with synchronization tolerance
    uint64_t Ts_base = (Tr - SCUP_TRANSMISSION_DELAY) / SCUP_TIME_DIVISOR;
    vector<uint8_t> n0_prime;
    bool syncOK = false;
    
    for (int off = -SCUP_TIME_TOLERANCE; off <= SCUP_TIME_TOLERANCE; off++) {
        uint64_t Ts_test = Ts_base + off;
        vector<uint8_t> Ts_vec = timestampToVec(Ts_test * SCUP_TIME_DIVISOR);
        vector<uint8_t> n0_test = xorVec(x, Ts_vec);
        
        if (H(n0_test) == n1) {
            n0_prime = n0_test;
            syncOK = true;
            break;
        }
    }
    
    if (!syncOK) return;  // Synchronization failed
    
    // Verify code authenticity
    if (HL(n0_prime) != K0) return;  // Authentication failed
    
    // SF-4: Generate key and challenge
    vector<uint8_t> n2;
    if (SCUP_FORMATION_MODEL == SERIAL_MODEL) {
        if (security.n2.empty()) {
            n2 = generateNonce160();
            security.n2 = n2;  // Save for reuse
        } else {
            n2 = security.n2;  // Reuse
        }
    } else {
        n2 = generateNonce160();  // New for each pair
    }
    
    vector<uint8_t> x1 = xorVec(n2, n0_prime);
    vector<uint8_t> K1 = HL(n2);
    
    // Save key
    neighborKeys[src] = K1;
    authenticatedNeighbors[src] = true;
    
    // Send challenge
    sendMessage(new MessageKeyChallenge(x1), src, 
                SCUP_TRANSMISSION_DELAY, 0);
}
```

### 4.3 SF-5: Complete Authentication (CHn side)
```cpp
void algorithm1_Complete(P2PNetworkInterface* src,
                        const vector<uint8_t>& x1) {
    // Check if already authenticated
    if (authenticatedNeighbors.count(src)) return;
    
    // Check if we have pending n0
    if (!pendingN0.count(src)) return;
    
    // Recover n0
    vector<uint8_t> n0 = pendingN0[src];
    
    // SF-5: Calculate n2 and K1
    vector<uint8_t> n2 = xorVec(x1, n0);
    vector<uint8_t> K1 = HL(n2);
    
    // Save
    security.n2 = n2;
    security.K1 = K1;
    neighborKeys[src] = K1;
    authenticatedNeighbors[src] = true;
    security.state = STATE_KEY_ESTABLISHED;
    
    // Cleanup
    pendingN0.erase(src);
    ongoingAuthentications.erase(src);
    
    // Notify neighbors
    notifyNeighborsSkeletonReady();
}
```

## Phase 5: Skeleton Formation Models

### 5.1 Serial Model Implementation
- iCH generates n2 once in first SF-4
- iCH reuses same n2 for all subsequent authentications
- All authenticated CHs also reuse n2 when acting as iCH
- Result: Single shared key K1 for entire skeleton

### 5.2 Parallel Model Implementation
- Each CH pair runs Algorithm 1 independently
- Each execution generates new n2
- Result: Unique pairwise keys

### 5.3 Skeleton Formation Orchestration
```cpp
void startSkeletonFormation() {
    security.phase = PHASE_SKELETON;
    
    // Catoms3D has up to 12 interfaces
    for (int i = 0; i < 12; i++) {
        P2PNetworkInterface* iface = module->getInterface(i);
        if (isInterfaceValid(iface)) {
            // Check if neighbor is a CH
            Catoms3DBlock* neighbor = 
                static_cast<Catoms3DBlock*>(iface->connectedInterface->hostBlock);
            
            if (isNeighborCH(neighbor)) {
                algorithm1_Initiate(iface);
            }
        }
    }
}
```

## Phase 6: Cluster Formation

### 6.1 CH to CM Authentication
- CH acts as iCH
- Directly connected CMs act as CHn
- Use Algorithm 1

### 6.2 CM to CM Authentication
- Authenticated CM acts as iCH for non-connected CMs
- Reuses n2 from CH to maintain cluster key

### 6.3 Key Generation Scenarios

**Case 1**: Reuse skeleton n2
```cpp
// CH uses same n2 from skeleton
// Entire cluster shares skeleton key
```

**Case 2**: Generate new cluster n2
```cpp
// CH generates new n2' for cluster
// Cluster has different key from skeleton
```

**Case 3**: Parallel model flexibility
```cpp
// CH chooses which n2 to use or generates new
```

## Phase 7: Message Handling

### 7.1 Event Processing
```cpp
void processLocalEvent(EventPtr pev) {
    Catoms3DBlockCode::processLocalEvent(pev);
    
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
    }
}
```

### 7.2 Message Handlers
Each handler extracts message data and calls appropriate algorithm function.

## Phase 8: Initialization and Role Assignment

### 8.1 Startup Sequence
```cpp
void startup() {
    // Determine cluster ID based on position or configuration
    security.clusterId = determineClusterId();
    
    // Determine role (iCH, CH, or CM)
    if (isInitialClusterHead()) {
        setAsICH();
        // Schedule START_FORMATION broadcast
        scheduler->schedule(new CodeStartEvent(scheduler->now() + 100, module));
    } else if (isClusterHead()) {
        setAsCH();
    } else {
        setAsCM();
    }
}
```

### 8.2 Role Determination
- Based on module ID, position, or XML configuration
- iCH: One module designated as initial
- CH: Cluster leaders (one per cluster)
- CM: All other modules

## Phase 9: Configuration and Testing

### 9.1 XML Configuration
```xml
<world gridSize="20,20,20" windowSize="1600,900">
    <camera target="10,10,10" directionSpherical="45,35,60" angle="35"/>
    <spotlight target="10,10,10" directionSpherical="30,50,80" angle="45"/>
    
    <blockList color="128,128,128" blocksize="10.0">
        <!-- iCH -->
        <block position="10,10,10" id="1"/>
        
        <!-- Cluster 1 CHs -->
        <block position="11,10,10" id="2"/>
        <block position="12,10,10" id="3"/>
        
        <!-- Cluster 1 CMs -->
        <block position="11,11,10" id="4"/>
        <block position="12,11,10" id="5"/>
        
        <!-- Cluster 2 CHs -->
        <block position="10,11,10" id="6"/>
        <block position="10,12,10" id="7"/>
        
        <!-- Cluster 2 CMs -->
        <block position="9,11,10" id="8"/>
        <block position="9,12,10" id="9"/>
    </blockList>
</world>
```

### 9.2 Test Scenarios
1. **Small skeleton (3-4 CHs)**: Verify Algorithm 1 correctness
2. **Serial vs Parallel**: Compare formation time and key distribution
3. **Cluster formation**: Verify CH-CM and CM-CM authentication
4. **Large scale**: Test with 50+ modules
5. **Clock desynchronization**: Test ±5 tolerance
6. **Attack scenarios**: Replay, forgery attempts

## Phase 10: Statistics and Visualization

### 10.1 Metrics Collection
- Messages sent/received per module
- Authentication success/failure count
- Formation time (skeleton, cluster, total)
- Key distribution (unique keys vs shared)

### 10.2 Visualization
- Color coding by cluster
- Color changes during authentication phases
- Statistics display at end of simulation

## Phase 11: Documentation

### 11.1 Code Documentation
- Doxygen comments for all functions
- Reference to S-Cup paper sections
- Algorithm 1 step annotations

### 11.2 User Documentation
- README with compilation instructions
- Configuration guide
- Parameter tuning guide
- Results interpretation

## Implementation Order

1. ✅ Requirements document (completed)
2. Project structure and Makefile
3. Cryptographic primitives (Phase 2)
4. Data structures (Phase 3)
5. Algorithm 1 core (Phase 4)
6. Serial skeleton formation (Phase 5.1)
7. Message handling (Phase 7)
8. Initialization (Phase 8)
9. Basic testing (Phase 9.1)
10. Parallel skeleton formation (Phase 5.2)
11. Cluster formation (Phase 6)
12. Advanced testing (Phase 9.2)
13. Statistics and visualization (Phase 10)
14. Documentation (Phase 11)

## Key Differences from BlinkyBlocks

1. **Neighbor count**: 12 interfaces instead of 6
2. **3D geometry**: Catoms3D spherical vs BlinkyBlocks cubic
3. **Movement**: Rotation-based vs static
4. **API differences**: Catoms3DBlock vs BlinkyBlocksBlock
5. **Interface access**: Different methods for getting neighbors

## Critical Implementation Notes

- **DO NOT** copy from scupBB implementation
- Follow S-Cup paper specifications exactly
- Use Catoms3D-specific APIs
- Test with Catoms3D-appropriate topologies
- Consider 3D spatial relationships for cluster determination
