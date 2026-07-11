# S-Cup Protocol Implementation for Catoms3D

## Overview

This specification defines the implementation of the S-Cup (Secure-Cup) security protocol for Catoms3D modular robots in the VisibleSim simulator, based on the WINCOM 2025 paper.

**Paper Reference**: "S-CUP: A Security Protocol for Self-reconfiguration by Clustering on Programmable Matter based Modular Robots"

## What is S-Cup?

S-Cup is a lightweight security protocol designed for programmable matter based on modular robots. It provides:

- **Mutual Authentication**: Modules prove they execute the same code
- **Key Management**: Adaptive key generation (single key or pairwise keys)
- **Confidentiality**: Encrypted communication after authentication
- **No Pre-shared Secrets**: Everything built from scratch
- **No Identifiers**: Anonymous authentication

## Protocol Phases

### Phase 1: Skeleton Formation (Inter-cluster)
- Cluster Heads (CHs) authenticate each other
- Forms the backbone structure
- Two models:
  - **Serial**: One shared key for all CHs (slower, simpler)
  - **Parallel**: Pairwise keys (faster, more complex)

### Phase 2: Cluster Formation (Intra-cluster)
- CHs authenticate Cluster Members (CMs)
- CMs propagate authentication within cluster
- Each cluster can have unique or shared key

### Phase 3: Operational
- Structure is formed and operational
- Secure communication using established keys

## Core Algorithm (Algorithm 1)

Five-step mutual authentication between two modules:

**SF-1**: CHn generates nonce n0, computes n1=H(n0), K0=H(L(n0 mod Nb)), x=Ts⊕n0

**SF-2**: CHn → iCH: (n1, x, K0)

**SF-3**: iCH recovers n0'=x⊕Ts, verifies n1=H(n0') and K0=H(L(n0' mod Nb))

**SF-4**: iCH → CHn: x1=n2⊕n0' (where K1=H(L(n2 mod Nb)))

**SF-5**: CHn computes n2=x1⊕n0, derives K1=H(L(n2 mod Nb))

Result: CHn and iCH share key K1 and are mutually authenticated

## Cryptographic Primitives

- **H()**: SPONGENT-160 hash (160 bits output)
- **L(i)**: Source code line i (256 bits)
- **HL(n)**: H(L(n mod Nb)) - composite hash
- **⊕**: XOR operation for nonce protection
- **Ts**: Timestamp with ±5 tolerance for clock desynchronization

## Security Features

✅ **Replay Attack Protection**: Timestamp-based nonce protection
✅ **Forgery Attack Protection**: Code-based authentication
✅ **Lightweight**: Only 6H + 2Tx + 4XOR per link
✅ **Scalable**: O(w) complexity where w = number of links
✅ **Resource Efficient**: O(1) memory per module

## Catoms3D Specifics

- **12 neighbors** (vs 6 for BlinkyBlocks)
- **3D spherical geometry**
- **Rotation-based movement**
- **Lattice-based positioning**

## Files

- `requirements.md`: Detailed functional and non-functional requirements
- `implementation-plan.md`: Phase-by-phase implementation guide
- `README.md`: This file

## Quick Start

1. Read `requirements.md` to understand all requirements
2. Follow `implementation-plan.md` phases in order
3. Start with cryptographic primitives (Phase 2)
4. Implement Algorithm 1 (Phase 4)
5. Add skeleton formation (Phase 5)
6. Test incrementally

## Key Parameters

```cpp
#define SCUP_CODE_LINES         128    // Number of code lines
#define SCUP_LINE_SIZE_BITS     256    // Bits per line
#define SCUP_HASH_SIZE_BYTES     20    // SPONGENT-160 output
#define SCUP_TIME_DIVISOR        10    // Timestamp divisor
#define SCUP_TIME_TOLERANCE       5    // Clock tolerance
#define SCUP_TRANSMISSION_DELAY 100    // µs
```

## Success Criteria

- [ ] All CHs authenticate and form skeleton
- [ ] All CMs authenticate and join clusters
- [ ] Keys correctly established
- [ ] No replay attacks succeed
- [ ] No forgery attacks succeed
- [ ] Formation time scales linearly
- [ ] Memory usage O(1) per module

## References

- WINCOM 2025 paper (conference-template-a4-WINCOM2025.pdf)
- VisibleSim Catoms3D API documentation
- SPONGENT-160 implementation in simulatorCore

## Notes

- This is a **NEW** implementation, not based on scupBB
- Follow the S-Cup paper specifications exactly
- Use Catoms3D-specific APIs throughout
- Test with 3D topologies appropriate for Catoms3D
