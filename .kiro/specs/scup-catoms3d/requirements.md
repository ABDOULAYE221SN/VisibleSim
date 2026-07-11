# S-Cup Protocol Implementation for Catoms3D - Requirements

## 1. Project Overview

Implementation of the S-Cup (Secure-Cup) security protocol for self-reconfiguration by clustering on Catoms3D modular robots in VisibleSim simulator.

**Reference**: WINCOM 2025 paper - "S-CUP: A Security Protocol for Self-reconfiguration by Clustering on Programmable Matter based Modular Robots"

## 2. System Context

### 2.1 Catoms3D Characteristics
- **Mobility**: Mobile robots that can move around their neighbors
- **Degree of freedom**: 3D movement with rotation around pivot neighbors
- **Number of neighbors**: Up to 12 neighbors (spherical geometry)
- **Communication**: Neighbor-to-neighbor (contact and/or wireless)
- **Architecture**: Distributed system with no central control

### 2.2 Key Differences from Other Platforms
- No unique identifiers for modules
- Highly dynamic topology during autoconfiguration
- Very large number of modules (scalability)
- Anonymous particles
- No routing (only direct neighbor communication)
- Not connected to Internet

## 3. Functional Requirements

### 3.1 System Architecture (FR-1)

**FR-1.1**: The system SHALL support clustering-based architecture
- Modules organized in clusters
- Each cluster has a Cluster Head (CH)
- Cluster Members (CM) belong to one cluster
- One Initial Cluster Head (iCH) initiates the process

**FR-1.2**: The system SHALL support three communication phases
- Phase 1: Inter-cluster communication (CH to CH) for skeleton formation
- Phase 2: Intra-cluster communication (CH to CM) for cluster formation
- Phase 3: Operational communication (any neighbor to neighbor)

**FR-1.3**: Modules SHALL communicate only with direct neighbors (one-hop)

**FR-1.4**: All modules SHALL execute identical source code

### 3.2 Cryptographic Primitives (FR-2)

**FR-2.1**: The system SHALL implement SPONGENT-160 hash function
- Input: arbitrary length data
- Output: 160-bit hash value (20 bytes)

**FR-2.2**: The system SHALL implement source code line function L(i)
- Input: line number i
- Output: 256-bit representation of code line
- Total lines: Nb = 128

**FR-2.3**: The system SHALL implement composite hash function HL(n)
- HL(n) = H(L(n mod Nb))
- Used for authentication fingerprints and key generation

**FR-2.4**: The system SHALL implement XOR operation
- For nonce protection: x = Ts ⊕ n0
- For key exchange: x1 = n2 ⊕ n0

**FR-2.5**: The system SHALL implement timestamp function
- Ts ≈ (ts / 10) rounded to nearest integer
- Allows ±5 units tolerance for clock desynchronization

### 3.3 Skeleton Formation - Serial Model (FR-3)

**FR-3.1**: Algorithm 1 SHALL implement mutual authentication between CHs

**SF-1**: CHn initiates authentication
- Generate random nonce n0 (160 bits)
- Compute n1 = H(n0)
- Compute authentication fingerprint K0 = H(L(n0 mod Nb))
- Compute x = Ts ⊕ n0 to protect nonce
- Prepare message (n1, x, K0)

**SF-2**: CHn sends authentication request to iCH
- Message: (n1, x||K0)
- Transmission delay: Δt

**SF-3**: iCH verifies synchronization and authenticates CHn
- Compute Ts ≈ ((Tr - Δt) / 10)
- Recover n0' = x ⊕ Ts
- Verify synchronization: n1 == H(n0')
- Verify code authenticity: K0 == H(L(n0' mod Nb))

**SF-4**: iCH generates key and sends challenge
- Generate nonce n2 (or reuse for serial model)
- Compute x1 = n2 ⊕ n0'
- Compute shared key K1 = H(L(n2 mod Nb))
- Send x1 to CHn

**SF-5**: CHn completes authentication and derives key
- Compute n2 = x1 ⊕ n0
- Compute shared key K1 = H(L(n2 mod Nb))
- Mutual authentication complete

**FR-3.2**: Serial model SHALL generate ONE shared key for entire skeleton
- iCH generates n2 once
- iCH reuses same n2 for all CHs
- All CHs share same K1

**FR-3.3**: Authenticated CHs SHALL propagate authentication
- CH1, CH2, CH4 act as iCH for remaining CHs
- Always reuse same n2 to maintain single key

### 3.4 Skeleton Formation - Parallel Model (FR-4)

**FR-4.1**: Parallel model SHALL allow simultaneous authentication
- Multiple CH pairs authenticate concurrently
- Reduces skeleton formation time

**FR-4.2**: Each CH pair SHALL establish unique pairwise key
- Each execution of Algorithm 1 uses new nonces
- Results in different keys per pair

**FR-4.3**: Connection initiator SHALL act as CHn (SF-1, SF-2)

**FR-4.4**: Connection recipient SHALL act as iCH (SF-3, SF-4)

### 3.5 Cluster Formation (FR-5)

**FR-5.1**: After skeleton formation, cluster formation SHALL begin

**FR-5.2**: CH SHALL authenticate CMs using Algorithm 1
- CH acts as iCH
- Directly connected CMs act as CHn

**FR-5.3**: Three key generation scenarios SHALL be supported

**Case 1**: CH reuses n2 from serial skeleton formation
- Entire cluster shares same key as skeleton

**Case 2**: CH generates new n2' for cluster
- Cluster has different key from skeleton

**Case 3**: CH reuses or generates n2 for parallel model
- Flexible key management

**FR-5.4**: Authenticated CMs SHALL propagate authentication
- MC1 acts as iCH for MC2
- Reuses n2 received from CH
- Maintains cluster key consistency

### 3.6 Operational Phase (FR-6)

**FR-6.1**: After structure formation, modules SHALL communicate securely
- Use established keys for encryption
- Support inter-cluster communication

**FR-6.2**: Modules from different clusters MAY authenticate
- Use Algorithm 1 if no shared key exists

### 3.7 Message Types (FR-7)

**FR-7.1**: AUTH_REQUEST message
- Fields: n1 (160 bits), x (160 bits), K0 (160 bits)
- Purpose: Initial authentication request

**FR-7.2**: KEY_CHALLENGE message
- Fields: x1 (160 bits)
- Purpose: Key establishment challenge

**FR-7.3**: SKELETON_READY message
- Purpose: Notify neighbors skeleton is formed

**FR-7.4**: CLUSTER_READY message
- Fields: clusterId
- Purpose: Notify cluster formation complete

**FR-7.5**: START_FORMATION message
- Purpose: Initiate skeleton formation process

## 4. Non-Functional Requirements

### 4.1 Security (NFR-1)

**NFR-1.1**: SHALL resist replay attacks
- Timestamp-based nonce protection
- Synchronized clocks with tolerance

**NFR-1.2**: SHALL resist forgery attacks
- Code-based authentication (K0 verification)
- Mutual authentication required

**NFR-1.3**: SHALL provide confidentiality
- Shared key encryption after authentication

**NFR-1.4**: SHALL NOT require pre-shared secrets
- All keys generated during protocol execution

**NFR-1.5**: SHALL NOT require module identifiers
- Anonymous authentication based on code

### 4.2 Performance (NFR-2)

**NFR-2.1**: Time complexity SHALL be O(w) where w = number of links
- Linear with structure size

**NFR-2.2**: Space complexity SHALL be O(1) per module
- Constant memory per module

**NFR-2.3**: Storage complexity SHALL be O(1)
- Only store nonce n0, generate keys on demand

**NFR-2.4**: Overhead per link SHALL be: 6H + 2Tx + 4XOR
- H: hash operation
- Tx: transmission
- XOR: XOR operation

### 4.3 Scalability (NFR-3)

**NFR-3.1**: SHALL support hundreds to thousands of modules

**NFR-3.2**: SHALL support up to 12 neighbors per Catoms3D module

**NFR-3.3**: SHALL adapt to dynamic topology changes

### 4.4 Resource Constraints (NFR-4)

**NFR-4.1**: SHALL use lightweight cryptography
- SPONGENT-160 suitable for constrained devices

**NFR-4.2**: SHALL minimize memory usage
- No key storage, only nonce storage

**NFR-4.3**: SHALL minimize communication overhead
- Only 2 messages per authentication (request + challenge)

## 5. Assumptions

### 5.1 Clock Synchronization
- All clocks reset at start of autoconfiguration
- Tolerance: ±5 units of desynchronization
- Timestamp divisor: 10

### 5.2 Transmission Delay
- Fixed transmission delay: Δt = 100 µs (configurable)

### 5.3 Code Integrity
- All legitimate modules execute identical source code
- Code has Nb = 128 lines
- Each line is 256 bits

### 5.4 Initial Configuration
- Modules pre-divided into cluster groups
- One module designated as iCH (by position, election, or external signal)

### 5.5 Communication
- Modules can communicate by contact or wirelessly
- Communication range limited to direct neighbors

## 6. Constraints

### 6.1 Platform Constraints
- Catoms3D specific: 12 possible neighbors
- 3D lattice-based movement
- Rotation-based locomotion

### 6.2 Simulator Constraints
- VisibleSim environment
- Event-driven simulation
- Message passing via P2PNetworkInterface

### 6.3 Implementation Constraints
- C++ language
- Must integrate with VisibleSim Catoms3D API
- Must use existing SPONGENT-160 implementation

## 7. Success Criteria

### 7.1 Functional Success
- [ ] All CHs successfully authenticate and form skeleton
- [ ] All CMs successfully authenticate and join clusters
- [ ] Shared keys correctly established
- [ ] Structure becomes operational

### 7.2 Security Success
- [ ] No replay attacks succeed
- [ ] No forgery attacks succeed
- [ ] Only legitimate modules authenticate
- [ ] Keys remain secret

### 7.3 Performance Success
- [ ] Formation time scales linearly with links
- [ ] Memory usage remains constant per module
- [ ] Overhead matches theoretical analysis (6H + 2Tx + 4XOR)

## 8. Out of Scope

- Physical reconfiguration movements (focus on security protocol only)
- Cluster leader election algorithms
- Fault tolerance and module failure handling
- Dynamic re-authentication after structure formation
- Integration with actual hardware

## 9. Future Enhancements

- Formal verification using AVISPA simulator
- Energy consumption measurements
- Comparison of serial vs parallel models
- Support for structure reconfiguration
- Handling of malicious modules
