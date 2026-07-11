---
marp: true
theme: default
paginate: true
backgroundColor: #fff
---

# S-CUP Protocol
## Security Protocol for Self-reconfiguration by Clustering

**Implementation for Catoms3D Modular Robots**

Based on WINCOM 2025 paper

---

# Agenda

1. Introduction & Context
2. Protocol Overview
3. Cryptographic Primitives
4. Three-Phase Architecture
5. Algorithm 1: Mutual Authentication
6. Cluster Management
7. Implementation Details
8. Performance & Statistics

---

# 1. Introduction

## What is S-CUP?

**S-CUP** = **S**ecurity Protocol for Self-reconfiguration by **C**l**u**stering on **P**rogrammable Matter

### Key Objectives:
- ✅ Secure authentication between modular robots
- ✅ Lightweight cryptography (resource-constrained devices)
- ✅ Time synchronization with tolerance
- ✅ Proof of identical code execution
- ✅ Cluster-based organization

---

# 2. Protocol Overview

## Architecture Components

```
┌─────────────────────────────────────────┐
│  Catoms3D Modular Robots Network        │
├─────────────────────────────────────────┤
│  Phase 1: Skeleton Formation (CHs)      │
│  Phase 2: Cluster Formation (CMs)       │
│  Phase 3: Operational (Inter-cluster)   │
└─────────────────────────────────────────┘
```

### Module Roles:
- **iCH** (Initial Cluster Head) - Module 1
- **CH** (Cluster Head) - Leaders of clusters
- **CM** (Cluster Member) - Regular members

---

# 3. Cryptographic Primitives

## SPONGENT-160 Hash Function

```cpp
H(data) → 160 bits
```

### Key Functions:

| Function | Description | Output |
|----------|-------------|--------|
| `H(x)` | SPONGENT-160 hash | 160 bits |
| `L(i)` | Source code line i | 256 bits |
| `HL(n)` | H(L(n mod Nb)) | 160 bits |
| `XOR` | Bitwise exclusive OR | 160 bits |

### Parameters:
- **Nb** = 128 code lines
- **Hash size** = 160 bits (20 bytes)
- **Time divisor** = 10 (Ts = ts/10)

---

# 4. Three-Phase Architecture

## Phase 1: Skeleton Formation 🔴

**Goal**: Authenticate all Cluster Heads (CHs)

- iCH (Module 1) authenticates with all CHs
- CHs authenticate with each other
- Forms the secure backbone

---

## Phase 2: Cluster Formation 🟢

**Goal**: Authenticate Cluster Members (CMs)

- Each CH authenticates its 3 CMs
- 4 modules per cluster (1 CH + 3 CMs)
- Cluster-based color coding

**Cluster Organization:**
```
Cluster 1: Modules 1-4   (RED)
Cluster 2: Modules 5-8   (GREEN)
Cluster 3: Modules 9-12  (BLUE)
```

---

## Phase 3: Operational 🔵

**Goal**: Inter-cluster authentication

- Modules from different clusters authenticate
- Full network connectivity
- System ready for reconfiguration

---

# 5. Algorithm 1: Mutual Authentication

## Five-Step Protocol (SF-1 to SF-5)

```
CHn (Requester)                    iCH (Verifier)
─────────────────                  ──────────────
SF-1: Generate n0, n1, K0
      x = Ts ⊕ n0
                                   
SF-2: ──────(n1, x, K0)──────────>
                                   SF-3: Verify sync & code
                                         n0' = x ⊕ Ts'
                                         Check: H(n0') = n1
                                         Check: HL(n0' mod Nb) = K0
                                   
                                   SF-4: Generate n2
                                         x1 = n2 ⊕ n0'
<─────────────(x1)─────────────────
                                   
SF-5: n2 = x1 ⊕ n0
      K1 = HL(n2 mod Nb)
      ✅ Shared key established
```

---

# SF-1 & SF-2: Initiation

## Requester (CHn) Actions:

```cpp
// SF-1: Generate authentication data
n0 = generateNonce160()           // Secret nonce
n1 = H(n0)                        // Authentication fingerprint
K0 = HL(n0 mod Nb)                // Code proof

// Protect n0 with timestamp
Ts = round(ts / 10)
x = Ts ⊕ n0                       // Time-protected nonce

// SF-2: Send authentication request
send(n1, x, K0) → iCH
```

**Message size**: 3 × 160 bits = 480 bits

---

# SF-3 & SF-4: Verification

## Verifier (iCH) Actions:

```cpp
// SF-3: Recover n0 with sync tolerance
Ts' = round((Tr - Δt) / 10)

for offset in [-5, +5]:           // ±5 tolerance
    n0' = x ⊕ (Ts' + offset)
    if H(n0') == n1:              // Verify identity
        if HL(n0' mod Nb) == K0:  // Verify code
            ✅ Authentication valid
            break

// SF-4: Generate shared key
n2 = generateNonce160()
x1 = n2 ⊕ n0'
K1 = HL(n2 mod Nb)                // Shared key
send(x1) → CHn
```

---

# SF-5: Completion

## Requester (CHn) Final Step:

```cpp
// SF-5: Calculate shared key
n2 = x1 ⊕ n0
K1 = HL(n2 mod Nb)

✅ Mutual authentication complete
✅ Shared key K1 established
```

### Security Properties:
- ✅ Mutual authentication
- ✅ Time synchronization verified
- ✅ Identical code proven
- ✅ Secure key exchange

---

# 6. Cluster Management

## Cluster Organization

```
┌──────────────────────────────────────┐
│ Cluster 1 (RED)    - Modules 1-4     │
│   • Module 1: iCH (Initial CH)       │
│   • Modules 2-4: CMs                 │
├──────────────────────────────────────┤
│ Cluster 2 (GREEN)  - Modules 5-8     │
│   • Module 5: CH                     │
│   • Modules 6-8: CMs                 │
├──────────────────────────────────────┤
│ Cluster 3 (BLUE)   - Modules 9-12    │
│   • Module 9: CH                     │
│   • Modules 10-12: CMs               │
└──────────────────────────────────────┘
```

**Cluster ID Calculation**: `(blockId - 1) / 4 + 1`

---

# Formation Models

## Serial vs Parallel

### Serial Model (Default)
```
• One shared key for entire skeleton
• K1 is identical for all authenticated pairs
• Faster formation
• Lower memory usage
```

### Parallel Model
```
• Unique key per pair
• K1 different for each connection
• Higher security
• More memory required
```

**Configuration**: `#define SCUP_FORMATION_MODEL SERIAL_MODEL`

---

# 7. Implementation Details

## Message Types

| Type | Code | Description |
|------|------|-------------|
| AUTH_REQUEST | 5001 | (n1, x, K0) - 480 bits |
| KEY_CHALLENGE | 5002 | (x1) - 160 bits |
| SKELETON_READY | 5003 | Notification |
| CLUSTER_READY | 5004 | Notification |
| START_FORMATION | 5005 | Trigger signal |

---

## Key Classes

### SCupCatoms3DCode
```cpp
class SCupCatoms3DCode : public Catoms3DBlockCode {
    // Security information
    SecurityInfo security;
    
    // Neighbor management
    map<P2PNetworkInterface*, vector<uint8_t>> neighborKeys;
    map<P2PNetworkInterface*, bool> authenticatedNeighbors;
    
    // Algorithm 1 functions
    void algorithm1_Initiate(P2PNetworkInterface* dest);
    void algorithm1_Verify(...);
    void algorithm1_Complete(...);
    
    // Phase management
    void startSkeletonFormation();
    void startClusterFormation();
    void startOperationalPhase();
};
```

---

## Security States

```cpp
enum SecurityState {
    STATE_UNLINKED,          // Not connected
    STATE_AUTHENTICATING,    // Authentication in progress
    STATE_AUTHENTICATED,     // Identity verified
    STATE_KEY_ESTABLISHED    // Shared key ready
};

enum ProtocolPhase {
    PHASE_WAITING,           // Waiting for trigger
    PHASE_SKELETON,          // Phase 1 active
    PHASE_CLUSTERING,        // Phase 2 active
    PHASE_OPERATIONAL        // Phase 3 active
};
```

---

# 8. Performance & Statistics

## Complexity Analysis

### Time Complexity
- **O(w)** where w = number of links
- Linear with network size

### Overhead per Link
```
6H  : 6 hash operations
2Tx : 2 transmissions
4XOR: 4 XOR operations
```

### Message Overhead
- **Phase 1**: 2 messages per CH pair
- **Phase 2**: 2 messages per CM
- **Phase 3**: 2 messages per inter-cluster link

---

## Statistics Display

```
╔════════════════════════════════════════════════╗
║         S-CUP STATISTICS                       ║
╠════════════════════════════════════════════════╣
║ Formation model      : SERIAL (1 key)          ║
╠════════════════════════════════════════════════╣
║ Authenticated modules:  12 /  12               ║
║ Messages exchanged   :    48                   ║
║ Formation time (us)  :   15000                 ║
║ Msg per module (avg) :   4.00                  ║
║ Time per module (us) : 1250.00                 ║
╠════════════════════════════════════════════════╣
║ Algo1 complexity     : O(w) - w links          ║
║ Overhead per link    : 6H + 2Tx + 4XOR         ║
╚════════════════════════════════════════════════╝
```

---

# Execution Flow

## Startup Sequence

```
1. Module initialization
   ├─ Determine cluster ID
   ├─ Assign role (iCH/CH/CM)
   └─ Set initial color

2. Phase 1: Skeleton Formation
   ├─ iCH waits for CH requests
   ├─ CHs initiate authentication
   └─ Skeleton complete → Pause

3. Phase 2: Cluster Formation
   ├─ CHs wait for CM requests
   ├─ CMs authenticate with their CH
   └─ All clusters complete → Pause

4. Phase 3: Operational
   ├─ Inter-cluster authentication
   └─ System ready
```

---

# Time Synchronization

## Tolerance Mechanism

```cpp
// Synchronization parameters
#define SCUP_TIME_DIVISOR      10    // Ts = ts/10
#define SCUP_TIME_TOLERANCE     5    // ±5 units
#define SCUP_TRANSMISSION_DELAY 100  // Δt = 100 µs

// Verification with tolerance
Ts_base = (Tr - Δt) / 10

for offset in [-5, +5]:
    Ts_test = Ts_base + offset
    n0_test = x ⊕ Ts_test
    if H(n0_test) == n1:
        ✅ Synchronization successful
```

**Tolerance window**: ±50 µs (5 × 10)

---

# Code Proof Mechanism

## Ensuring Identical Code

```cpp
// Each module has Nb = 128 code lines
// Each line L(i) is 256 bits

// Proof generation (SF-1)
K0 = H(L(n0 mod Nb))

// Proof verification (SF-3)
if HL(n0' mod Nb) == K0:
    ✅ Same code running
else:
    ❌ Different code - reject
```

### Security Benefit:
- Prevents malicious code injection
- Ensures all modules run identical firmware
- Lightweight verification (one hash)

---

# Visual Feedback

## Color Coding System

| Color | Meaning |
|-------|---------|
| 🔴 RED | Cluster 1 (authenticated) |
| 🟢 GREEN | Cluster 2 (authenticated) |
| 🔵 BLUE | Cluster 3 (authenticated) |
| ⚪ GREY | Unauthenticated module |
| 🟠 ORANGE | Cluster Head (before auth) |
| 🔷 CYAN | Authentication in progress |

**Real-time visualization** of protocol progress

---

# Key Features Summary

## ✅ Security
- Mutual authentication
- Lightweight cryptography (SPONGENT-160)
- Proof of code execution
- Secure key exchange

## ✅ Robustness
- Time synchronization with tolerance
- Cluster-based organization
- Phase-by-phase progression
- Error handling

## ✅ Efficiency
- O(w) complexity
- Minimal message overhead
- Parallel authentication support
- Resource-friendly

---

# Configuration Options

## Customizable Parameters

```cpp
// Cryptographic
#define SCUP_CODE_LINES         128
#define SCUP_LINE_SIZE_BITS     256
#define SCUP_HASH_SIZE_BYTES     20

// Timing
#define SCUP_TIME_DIVISOR        10
#define SCUP_TIME_TOLERANCE       5
#define SCUP_TRANSMISSION_DELAY 100

// Formation
#define SCUP_FORMATION_MODEL SERIAL_MODEL
// Options: SERIAL_MODEL, PARALLEL_MODEL

// Colors
#define SCUP_CLUSTER_1_COLOR  RED
#define SCUP_CLUSTER_2_COLOR  GREEN
#define SCUP_CLUSTER_3_COLOR  BLUE
```

---

# Use Cases

## Applications

1. **Secure Reconfiguration**
   - Shape morphing with authentication
   - Verified topology changes

2. **Distributed Systems**
   - Secure sensor networks
   - Authenticated robot swarms

3. **Critical Infrastructure**
   - Medical nanorobots
   - Space exploration modules

4. **Research**
   - Protocol validation
   - Performance benchmarking

---

# Advantages

## Why S-CUP?

### 🚀 Performance
- Fast authentication (microseconds)
- Low message overhead
- Scalable to large networks

### 🔒 Security
- Cryptographically secure
- Resistant to replay attacks
- Code integrity verification

### 💡 Practicality
- Easy to implement
- Configurable parameters
- Visual feedback

---

# Future Enhancements

## Potential Improvements

1. **Dynamic Clustering**
   - Adaptive cluster sizes
   - Runtime reorganization

2. **Advanced Cryptography**
   - Post-quantum algorithms
   - Hybrid encryption

3. **Fault Tolerance**
   - Byzantine fault handling
   - Automatic recovery

4. **Optimization**
   - Reduced latency
   - Energy efficiency

---

# Conclusion

## S-CUP Protocol Summary

✅ **Secure** - Mutual authentication with proof of code
✅ **Efficient** - O(w) complexity, minimal overhead
✅ **Robust** - Time synchronization with tolerance
✅ **Practical** - Implemented for Catoms3D robots
✅ **Scalable** - Cluster-based architecture

### Key Innovation:
**Lightweight security for resource-constrained modular robots**

---

# References & Resources

## Implementation Files
- `scupCatoms3D.cpp` - Main entry point
- `SCupCatoms3DCode.h` - Protocol definitions
- `SCupCatoms3DCode.cpp` - Core implementation

## Documentation
- WINCOM 2025 paper
- VisibleSim framework
- SPONGENT-160 specification

## Contact
For questions about this implementation, refer to the source code documentation.

---

# Thank You!

## Questions?

**S-CUP Protocol**
Security Protocol for Self-reconfiguration by Clustering

*Implementation for Catoms3D Modular Robots*

---
