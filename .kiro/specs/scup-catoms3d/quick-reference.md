# S-Cup Implementation Quick Reference

## Constants

```cpp
#define SCUP_CODE_LINES         128
#define SCUP_LINE_SIZE_BITS     256
#define SCUP_HASH_SIZE_BYTES     20
#define SCUP_TIME_DIVISOR        10
#define SCUP_TIME_TOLERANCE       5
#define SCUP_TRANSMISSION_DELAY 100

#define SCUP_FORMATION_MODEL SERIAL_MODEL  // or PARALLEL_MODEL
```

## Message Type IDs

```cpp
#define MSG_SCUP_AUTH_REQUEST   5001
#define MSG_SCUP_KEY_CHALLENGE  5002
#define MSG_SCUP_SKELETON_READY 5003
#define MSG_SCUP_CLUSTER_READY  5004
#define MSG_SCUP_START_FORMATION 5005
```

## Enumerations

```cpp
enum ModuleRole {
    ROLE_UNDEFINED = 0,
    ROLE_ICH,    // Initial Cluster Head
    ROLE_CH,     // Cluster Head
    ROLE_CM      // Cluster Member
};

enum ProtocolPhase {
    PHASE_WAITING = 0,
    PHASE_SKELETON,      // CHs authenticate
    PHASE_CLUSTERING,    // CMs join clusters
    PHASE_OPERATIONAL    // Structure formed
};

enum SecurityState {
    STATE_UNLINKED = 0,
    STATE_AUTHENTICATING,
    STATE_AUTHENTICATED,
    STATE_KEY_ESTABLISHED
};

enum FormationModel {
    SERIAL_MODEL = 1,    // One shared key
    PARALLEL_MODEL = 2   // Pairwise keys
};
```

## Core Functions

### Cryptographic Primitives

```cpp
// SPONGENT-160 hash: data → 160 bits
vector<uint8_t> H(const vector<uint8_t>& data);
vector<uint8_t> H(uint64_t val);

// Source code line: i → 256 bits
vector<uint8_t> L(int line);

// Composite hash: n → H(L(n mod Nb))
vector<uint8_t> HL(const vector<uint8_t>& n);
vector<uint8_t> HL(uint64_t n);

// XOR operation
vector<uint8_t> xorVec(const vector<uint8_t>& a, 
                       const vector<uint8_t>& b);

// Generate 160-bit random nonce
vector<uint8_t> generateNonce160();

// Timestamp: ts → Ts ≈ (ts / 10)
vector<uint8_t> timestampToVec(uint64_t ts);
```

### Algorithm 1 Functions

```cpp
// SF-1 & SF-2: Initiate authentication (CHn side)
void algorithm1_Initiate(P2PNetworkInterface* dest);

// SF-3 & SF-4: Verify and challenge (iCH side)
void algorithm1_Verify(P2PNetworkInterface* src,
                      const vector<uint8_t>& n1,
                      const vector<uint8_t>& x,
                      const vector<uint8_t>& K0);

// SF-5: Complete authentication (CHn side)
void algorithm1_Complete(P2PNetworkInterface* src,
                        const vector<uint8_t>& x1);
```

### Phase Management

```cpp
void startSkeletonFormation();   // Phase 1
void startClusterFormation();     // Phase 2
void startOperationalPhase();     // Phase 3
```

### Utility Functions

```cpp
bool isCH() const;
bool isICH() const;
bool isCM() const;
bool isInSkeleton() const;
bool isInterfaceValid(P2PNetworkInterface* iface) const;
```

## Algorithm 1 Pseudocode

### SF-1 & SF-2: Initiate (CHn)
```
n0 ← random(160 bits)
n1 ← H(n0)
K0 ← H(L(n0 mod 128))
ts ← now()
Ts ← ts / 10
x ← Ts ⊕ n0
send (n1, x, K0) to iCH
save n0 for later
```

### SF-3 & SF-4: Verify (iCH)
```
receive (n1, x, K0) at time Tr
Ts_base ← (Tr - Δt) / 10

// Synchronization check with tolerance
for offset in [-5, +5]:
    Ts_test ← Ts_base + offset
    n0' ← x ⊕ Ts_test
    if H(n0') == n1:
        sync_ok ← true
        break

if not sync_ok:
    reject

// Code authentication
if H(L(n0' mod 128)) ≠ K0:
    reject

// Key generation
if SERIAL_MODEL and n2 not set:
    n2 ← random(160 bits)
    save n2 for reuse
else if SERIAL_MODEL:
    n2 ← saved n2
else:
    n2 ← random(160 bits)

x1 ← n2 ⊕ n0'
K1 ← H(L(n2 mod 128))
save K1 for this neighbor
send x1 to CHn
```

### SF-5: Complete (CHn)
```
receive x1
n0 ← retrieve saved n0
n2 ← x1 ⊕ n0
K1 ← H(L(n2 mod 128))
save K1 for this neighbor
authentication complete
```

## Message Structures

```cpp
class MessageAuthRequest : public Message {
public:
    vector<uint8_t> n1;   // 160 bits
    vector<uint8_t> x;    // 160 bits
    vector<uint8_t> K0;   // 160 bits
    
    MessageAuthRequest(const vector<uint8_t>& _n1,
                      const vector<uint8_t>& _x,
                      const vector<uint8_t>& _K0);
};

class MessageKeyChallenge : public Message {
public:
    vector<uint8_t> x1;   // 160 bits
    
    MessageKeyChallenge(const vector<uint8_t>& _x1);
};

class MessageSkeletonReady : public Message {
    // No payload
};

class MessageClusterReady : public Message {
public:
    int clusterId;
};

class MessageStartFormation : public Message {
    // No payload
};
```

## Event Handling Pattern

```cpp
void processLocalEvent(EventPtr pev) {
    Catoms3DBlockCode::processLocalEvent(pev);
    
    if (pev->eventType == EVENT_NI_RECEIVE) {
        auto msg = static_pointer_cast<NetworkInterfaceReceiveEvent>(pev)->message;
        P2PNetworkInterface* src = msg->destinationInterface;
        
        switch (msg->type) {
            case MSG_SCUP_AUTH_REQUEST:
                onAuthRequestReceived(msg, src);
                break;
            case MSG_SCUP_KEY_CHALLENGE:
                onKeyChallengeReceived(msg, src);
                break;
            // ... other cases
        }
    }
}
```

## Catoms3D Specifics

### Getting Neighbors
```cpp
// Catoms3D has up to 12 interfaces
for (int i = 0; i < 12; i++) {
    P2PNetworkInterface* iface = module->getInterface(i);
    if (isInterfaceValid(iface)) {
        // Process neighbor
    }
}
```

### Interface Validation
```cpp
bool isInterfaceValid(P2PNetworkInterface* iface) const {
    if (!iface || !iface->connectedInterface) return false;
    if (!iface->connectedInterface->hostBlock) return false;
    if (iface->connectedInterface->hostBlock == module) return false;
    return true;
}
```

### Getting Neighbor Module
```cpp
Catoms3DBlock* neighbor = 
    static_cast<Catoms3DBlock*>(iface->connectedInterface->hostBlock);
```

## Startup Pattern

```cpp
void startup() {
    // Determine role
    if (module->blockId == 1) {
        setAsICH();
        // Schedule formation start
        scheduler->schedule(
            new CodeStartEvent(scheduler->now() + 100, module));
    } else if (isClusterHeadPosition()) {
        setAsCH();
    } else {
        setAsCM();
    }
}
```

## Testing Checklist

### Unit Tests
- [ ] H() produces 160-bit output
- [ ] L(i) produces 256-bit output
- [ ] HL(n) = H(L(n mod 128))
- [ ] XOR is reversible: (a ⊕ b) ⊕ b = a
- [ ] Timestamp tolerance works: ±5 units

### Integration Tests
- [ ] SF-1 to SF-5 complete successfully
- [ ] Synchronization check works
- [ ] Code authentication works
- [ ] Key derivation matches on both sides
- [ ] Serial model produces same key
- [ ] Parallel model produces different keys

### System Tests
- [ ] Small skeleton (3-4 CHs) forms
- [ ] Large skeleton (10+ CHs) forms
- [ ] Cluster formation works
- [ ] All modules authenticate
- [ ] No replay attacks succeed
- [ ] Formation time is O(w)

## Common Pitfalls

❌ **Don't** transmit n0 directly
✅ **Do** protect with XOR: x = Ts ⊕ n0

❌ **Don't** use exact timestamp
✅ **Do** use tolerance: try ±5 offsets

❌ **Don't** store all keys
✅ **Do** store only n0, generate keys on demand

❌ **Don't** skip synchronization check
✅ **Do** verify n1 = H(n0') before authentication

❌ **Don't** reuse n0
✅ **Do** generate new n0 for each authentication

❌ **Don't** forget to save n0 before SF-2
✅ **Do** save in pendingN0 map for SF-5

## Performance Targets

| Metric | Target |
|--------|--------|
| Time complexity | O(w) where w = links |
| Space per module | O(1) |
| Messages per link | 2 (request + challenge) |
| Overhead per link | 6H + 2Tx + 4XOR |
| Message size | 80 bytes total |
| Hash operations | 6 per link |

## Debug Output Format

```cpp
console << "[" << scheduler->now() << "] Module " << module->blockId 
        << " : ALGORITHM 1 - INITIATION (SF-1, SF-2)\n";
console << "    Role: " << roleToString(security.role) << "\n";
console << "    Phase: " << phaseToString(security.phase) << "\n";
console << "    n0 generated, n1 = H(n0), K0 = HL(n0)\n";
console << "    Ts = " << (ts / SCUP_TIME_DIVISOR) << ", x = Ts ⊕ n0\n";
console << "    Sending AUTH_REQUEST\n";
```

## Statistics to Track

```cpp
// Global statistics
static set<bID> globalAuthenticatedModules;
static uint64_t globalFormationStartTime;
static uint64_t globalFormationEndTime;
static int globalTotalMessages;

// Per-module statistics
int messagesSent;
int messagesReceived;
int successfulAuthentications;
```

## Color Coding (for visualization)

```cpp
#define SCUP_ICH_COLOR    YELLOW   // Initial CH
#define SCUP_CH_COLOR     ORANGE   // CH waiting
#define SCUP_CM_COLOR     GREY     // CM waiting
#define SCUP_AUTH_COLOR   CYAN     // During auth
#define SCUP_CLUSTER_1    RED      // Cluster 1
#define SCUP_CLUSTER_2    GREEN    // Cluster 2
#define SCUP_CLUSTER_3    BLUE     // Cluster 3
#define SCUP_OPERATIONAL  WHITE    // Operational
```

## File Structure

```
applicationsSrc/scupCatoms3D/
├── scupCatoms3D.cpp              # main()
├── SCupCatoms3DCode.h            # Class declaration
├── SCupCatoms3DCode.cpp          # Implementation
└── Makefile

applicationsBin/scupCatoms3D/
├── config.xml                    # Test configuration
├── scupCatoms3D                  # Executable
└── README.md
```

## Makefile Template

```makefile
SRCS = scupCatoms3D.cpp SCupCatoms3DCode.cpp
OUT = $(APPDIR)/scupCatoms3D
MODULELIB = -lsimCatoms3D
```

## Next Steps

1. Create project structure
2. Implement cryptographic primitives
3. Implement Algorithm 1
4. Test with small configuration
5. Add cluster formation
6. Test with large configuration
7. Measure performance
8. Document results
