# S-Cup Protocol for Catoms3D - Complete Specification Summary

## 📋 Documentation Overview

This specification provides a complete implementation plan for the S-Cup security protocol on Catoms3D modular robots, based on the WINCOM 2025 paper.

### Available Documents

1. **README.md** - Start here! Overview and quick introduction
2. **requirements.md** - Complete functional and non-functional requirements
3. **implementation-plan.md** - Detailed phase-by-phase implementation guide
4. **algorithm-visual.md** - Visual diagrams and step-by-step algorithm explanation
5. **quick-reference.md** - Quick lookup for constants, functions, and patterns
6. **test-configurations.md** - 10 XML test configurations with expected results
7. **SUMMARY.md** - This file

## 🎯 Project Goal

Implement the S-Cup (Secure-Cup) security protocol for self-reconfiguration by clustering on Catoms3D modular robots in VisibleSim simulator.

## 📖 Paper Reference

**Title**: "S-CUP: A Security Protocol for Self-reconfiguration by Clustering on Programmable Matter based Modular Robots"

**Conference**: WINCOM 2025

**File**: `conference-template-a4-WINCOM2025.pdf`

## 🔑 Key Concepts

### What is S-Cup?

A lightweight security protocol that provides:
- **Mutual Authentication**: Modules prove they execute the same code
- **Key Management**: Adaptive key generation (single or pairwise keys)
- **Confidentiality**: Encrypted communication after authentication
- **Zero Pre-configuration**: No pre-shared secrets or identifiers needed

### Three Phases

1. **Skeleton Formation** (Phase 1)
   - Cluster Heads (CHs) authenticate each other
   - Forms backbone structure
   - Serial model: 1 shared key
   - Parallel model: pairwise keys

2. **Cluster Formation** (Phase 2)
   - CHs authenticate Cluster Members (CMs)
   - CMs propagate authentication
   - Each cluster forms securely

3. **Operational** (Phase 3)
   - Structure is formed
   - Secure communication using established keys

### Core Algorithm (Algorithm 1)

Five-step mutual authentication:

```
SF-1: CHn generates n0, computes n1=H(n0), K0=H(L(n0 mod Nb)), x=Ts⊕n0
SF-2: CHn → iCH: (n1, x, K0)
SF-3: iCH verifies synchronization and code authenticity
SF-4: iCH → CHn: x1=n2⊕n0' (with K1=H(L(n2 mod Nb)))
SF-5: CHn derives K1=H(L(n2 mod Nb))
```

Result: Mutual authentication + shared key K1

## 🔐 Security Features

✅ **Replay Attack Protection**: Timestamp-based with ±5 tolerance
✅ **Forgery Attack Protection**: Code-based authentication
✅ **Lightweight**: Only 6H + 2Tx + 4XOR per link
✅ **Scalable**: O(w) complexity (w = number of links)
✅ **Resource Efficient**: O(1) memory per module
✅ **No Pre-shared Secrets**: Everything generated during protocol
✅ **Anonymous**: No module identifiers required

## 🏗️ Implementation Phases

### Phase 1: Setup (Week 1)
- Create project structure
- Define constants and enumerations
- Set up Makefile

### Phase 2: Cryptography (Week 1-2)
- Implement H() using SPONGENT-160
- Implement L(i) for code lines
- Implement HL(n) composite hash
- Implement XOR, nonce generation, timestamp

### Phase 3: Data Structures (Week 2)
- SecurityInfo structure
- Message classes (5 types)
- Main SCupCatoms3DCode class

### Phase 4: Algorithm 1 (Week 2-3)
- SF-1 & SF-2: Initiate authentication
- SF-3 & SF-4: Verify and challenge
- SF-5: Complete authentication

### Phase 5: Skeleton Formation (Week 3)
- Serial model implementation
- Parallel model implementation
- Orchestration logic

### Phase 6: Cluster Formation (Week 4)
- CH to CM authentication
- CM to CM propagation
- Three key generation scenarios

### Phase 7: Testing (Week 4-5)
- Unit tests (cryptographic primitives)
- Integration tests (Algorithm 1)
- System tests (10 configurations)
- Performance measurements

### Phase 8: Documentation (Week 5)
- Code documentation (Doxygen)
- User guide
- Results analysis

## 📊 Key Parameters

```cpp
SCUP_CODE_LINES = 128          // Number of code lines
SCUP_LINE_SIZE_BITS = 256      // Bits per line
SCUP_HASH_SIZE_BYTES = 20      // SPONGENT-160 output
SCUP_TIME_DIVISOR = 10         // Timestamp divisor
SCUP_TIME_TOLERANCE = 5        // Clock tolerance (±5)
SCUP_TRANSMISSION_DELAY = 100  // µs
```

## 🎨 Catoms3D Specifics

- **12 neighbors** (vs 6 for BlinkyBlocks)
- **3D spherical geometry**
- **Rotation-based movement**
- **Lattice-based positioning**
- **Different API**: Catoms3DBlock, Catoms3DBlockCode

## 📈 Performance Targets

| Metric | Target |
|--------|--------|
| Time complexity | O(w) where w = links |
| Space per module | O(1) |
| Messages per link | 2 (request + challenge) |
| Overhead per link | 6H + 2Tx + 4XOR |
| Message size | 80 bytes total |
| Hash operations | 6 per link |

## ✅ Success Criteria

### Functional
- [ ] All CHs successfully authenticate and form skeleton
- [ ] All CMs successfully authenticate and join clusters
- [ ] Shared keys correctly established
- [ ] Structure becomes operational

### Security
- [ ] No replay attacks succeed
- [ ] No forgery attacks succeed
- [ ] Only legitimate modules authenticate
- [ ] Keys remain secret

### Performance
- [ ] Formation time scales linearly with links
- [ ] Memory usage remains constant per module
- [ ] Overhead matches theoretical analysis

## 🧪 Test Configurations

10 test configurations provided:

1. **Minimal** (2 modules): Basic Algorithm 1
2. **Small Skeleton** (3 modules): Serial formation
3. **Cross Skeleton** (5 modules): Parallel formation
4. **Simple Cluster** (9 modules): Skeleton + clusters
5. **Cup Structure** (15 modules): Realistic 3D
6. **Large Scale** (50 modules): Scalability test
7. **Clock Desync** (2 modules): Tolerance test
8. **Attack Scenario** (3 modules): Replay prevention
9. **Model Comparison** (10 modules): Serial vs Parallel
10. **3D Lattice** (27 modules): Full 3D geometry

## 📁 File Structure

```
.kiro/specs/scup-catoms3D/
├── README.md                    # Start here
├── requirements.md              # All requirements
├── implementation-plan.md       # Phase-by-phase guide
├── algorithm-visual.md          # Visual diagrams
├── quick-reference.md           # Quick lookup
├── test-configurations.md       # Test configs
└── SUMMARY.md                   # This file

applicationsSrc/scupCatoms3D/    # To be created
├── scupCatoms3D.cpp
├── SCupCatoms3DCode.h
├── SCupCatoms3DCode.cpp
└── Makefile

applicationsBin/scupCatoms3D/    # To be created
├── config.xml
├── scupCatoms3D (executable)
└── README.md
```

## 🚀 Getting Started

### Step 1: Read Documentation
1. Start with `README.md` for overview
2. Read `requirements.md` for complete requirements
3. Study `algorithm-visual.md` to understand Algorithm 1
4. Review `implementation-plan.md` for detailed steps

### Step 2: Setup Project
```bash
mkdir -p applicationsSrc/scupCatoms3D
mkdir -p applicationsBin/scupCatoms3D
cd applicationsSrc/scupCatoms3D
```

### Step 3: Implement Phase by Phase
Follow `implementation-plan.md` phases 1-11 in order

### Step 4: Test Incrementally
Use configurations from `test-configurations.md`

### Step 5: Measure and Document
Collect statistics and document results

## 🔍 Quick Reference

### Message Types
- `MSG_SCUP_AUTH_REQUEST` (5001): SF-2 authentication request
- `MSG_SCUP_KEY_CHALLENGE` (5002): SF-4 key challenge
- `MSG_SCUP_SKELETON_READY` (5003): Skeleton formed notification
- `MSG_SCUP_CLUSTER_READY` (5004): Cluster formed notification
- `MSG_SCUP_START_FORMATION` (5005): Start formation signal

### Module Roles
- `ROLE_ICH`: Initial Cluster Head (starts formation)
- `ROLE_CH`: Cluster Head (cluster leader)
- `ROLE_CM`: Cluster Member

### Protocol Phases
- `PHASE_WAITING`: Waiting for formation start
- `PHASE_SKELETON`: CHs authenticating
- `PHASE_CLUSTERING`: CMs joining clusters
- `PHASE_OPERATIONAL`: Structure formed

### Security States
- `STATE_UNLINKED`: Not authenticated
- `STATE_AUTHENTICATING`: Authentication in progress
- `STATE_AUTHENTICATED`: Authenticated
- `STATE_KEY_ESTABLISHED`: Key established

## 📚 Additional Resources

### Paper Sections
- Section II: Context (modular robots characteristics)
- Section III: Background (comparison with WSN/IoT)
- Section IV: Assumptions and Architecture
- Section V: Security Challenges
- Section VI: Secure-Cup Protocol (MAIN)
- Section VII: Security Analysis
- Section VIII: Conclusion

### VisibleSim Resources
- Catoms3D API documentation
- SPONGENT-160 implementation: `simulatorCore/src/spongent160.h`
- Example applications: `applicationsSrc/`

## ⚠️ Important Notes

1. **DO NOT** copy from scupBB implementation
2. **DO** follow S-Cup paper specifications exactly
3. **DO** use Catoms3D-specific APIs
4. **DO** test with 3D topologies
5. **DO** implement incrementally and test each phase

## 🎓 Learning Path

### Beginner
1. Read README.md
2. Study algorithm-visual.md
3. Understand the 5 steps of Algorithm 1
4. Run minimal test (2 modules)

### Intermediate
1. Read requirements.md completely
2. Study implementation-plan.md
3. Implement cryptographic primitives
4. Implement Algorithm 1
5. Test with small configurations

### Advanced
1. Implement both formation models
2. Add cluster formation
3. Test with large configurations
4. Measure and optimize performance
5. Write comprehensive documentation

## 📞 Support

If you need clarification:
1. Re-read the relevant documentation section
2. Check quick-reference.md for syntax
3. Review algorithm-visual.md for algorithm details
4. Consult the original paper (conference-template-a4-WINCOM2025.pdf)

## 🏁 Final Checklist

Before considering implementation complete:

- [ ] All 5 SF steps implemented correctly
- [ ] Both serial and parallel models work
- [ ] Cluster formation works
- [ ] All 10 test configurations pass
- [ ] Security tests pass (replay, forgery)
- [ ] Performance meets targets (O(w), O(1))
- [ ] Code is documented (Doxygen)
- [ ] User guide written
- [ ] Results analyzed and documented

## 📝 Version History

- v1.0 (2025-04-23): Initial specification based on WINCOM 2025 paper

---

**Ready to start?** Begin with `README.md` and follow the implementation plan!

**Questions?** Refer to the appropriate documentation file above.

**Good luck with your implementation! 🚀**
