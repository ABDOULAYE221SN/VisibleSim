# S-Cup Test Configurations for Catoms3D

## Configuration 1: Minimal Test (4 modules)

**Purpose**: Test basic Algorithm 1 between 2 CHs

```xml
<?xml version="1.0" encoding="ISO-8859-1"?>
<world gridSize="20,20,20" windowSize="1600,900" maxSimulationDate="10000000">
    <camera target="10,10,10" directionSpherical="45,35,60" angle="35"/>
    <spotlight target="10,10,10" directionSpherical="30,50,80" angle="45"/>
    
    <blockList color="128,128,128" blocksize="10.0">
        <!-- iCH (Initial Cluster Head) -->
        <block position="10,10,10" id="1"/>
        
        <!-- CH1 (Cluster Head 1) -->
        <block position="11,10,10" id="2"/>
    </blockList>
</world>
```

**Expected Behavior**:
- Module 1 (iCH) initiates
- Module 2 (CH1) authenticates with iCH
- One shared key K1 established
- Both modules turn green (authenticated)

---

## Configuration 2: Small Skeleton (6 modules)

**Purpose**: Test serial skeleton formation with 3 CHs

```xml
<?xml version="1.0" encoding="ISO-8859-1"?>
<world gridSize="20,20,20" windowSize="1600,900" maxSimulationDate="20000000">
    <camera target="10,10,10" directionSpherical="45,35,60" angle="35"/>
    <spotlight target="10,10,10" directionSpherical="30,50,80" angle="45"/>
    
    <blockList color="128,128,128" blocksize="10.0">
        <!-- Skeleton: 3 CHs in a line -->
        <block position="10,10,10" id="1"/>  <!-- iCH -->
        <block position="11,10,10" id="2"/>  <!-- CH1 -->
        <block position="12,10,10" id="3"/>  <!-- CH2 -->
    </blockList>
</world>
```

**Expected Behavior**:
- iCH (1) authenticates CH1 (2)
- CH1 (2) authenticates CH2 (3)
- All share same key K1 (serial model)
- Formation time: ~2 × (6H + 2Tx + 4XOR)

---

## Configuration 3: Cross Skeleton (5 CHs)

**Purpose**: Test parallel skeleton formation

```xml
<?xml version="1.0" encoding="ISO-8859-1"?>
<world gridSize="20,20,20" windowSize="1600,900" maxSimulationDate="30000000">
    <camera target="10,10,10" directionSpherical="45,35,60" angle="35"/>
    <spotlight target="10,10,10" directionSpherical="30,50,80" angle="45"/>
    
    <blockList color="128,128,128" blocksize="10.0">
        <!-- Cross pattern: iCH at center -->
        <block position="10,10,10" id="1"/>  <!-- iCH (center) -->
        <block position="11,10,10" id="2"/>  <!-- CH1 (right) -->
        <block position="9,10,10" id="3"/>   <!-- CH2 (left) -->
        <block position="10,11,10" id="4"/>  <!-- CH3 (front) -->
        <block position="10,9,10" id="5"/>   <!-- CH4 (back) -->
    </blockList>
</world>
```

**Expected Behavior**:
- iCH (1) at center connects to 4 CHs
- Serial model: All share same K1
- Parallel model: 4 different keys (K1_12, K1_13, K1_14, K1_15)
- Formation can happen in parallel

---

## Configuration 4: Simple Cluster (9 modules)

**Purpose**: Test skeleton + cluster formation

```xml
<?xml version="1.0" encoding="ISO-8859-1"?>
<world gridSize="20,20,20" windowSize="1600,900" maxSimulationDate="50000000">
    <camera target="10,10,10" directionSpherical="45,35,60" angle="35"/>
    <spotlight target="10,10,10" directionSpherical="30,50,80" angle="45"/>
    
    <blockList color="128,128,128" blocksize="10.0">
        <!-- Cluster 1: iCH + 2 CMs -->
        <block position="10,10,10" id="1"/>  <!-- iCH -->
        <block position="10,11,10" id="2"/>  <!-- CM1 -->
        <block position="10,12,10" id="3"/>  <!-- CM2 -->
        
        <!-- Cluster 2: CH + 2 CMs -->
        <block position="11,10,10" id="4"/>  <!-- CH -->
        <block position="11,11,10" id="5"/>  <!-- CM1 -->
        <block position="11,12,10" id="6"/>  <!-- CM2 -->
        
        <!-- Cluster 3: CH + 2 CMs -->
        <block position="12,10,10" id="7"/>  <!-- CH -->
        <block position="12,11,10" id="8"/>  <!-- CM1 -->
        <block position="12,12,10" id="9"/>  <!-- CM2 -->
    </blockList>
</world>
```

**Expected Behavior**:
1. Phase 1: Skeleton formation (1, 4, 7)
2. Phase 2: Cluster formation
   - Cluster 1: 1 → 2 → 3
   - Cluster 2: 4 → 5 → 6
   - Cluster 3: 7 → 8 → 9
3. All modules authenticated
4. Color coding by cluster

---

## Configuration 5: Cup Structure (15 modules)

**Purpose**: Test realistic 3D structure

```xml
<?xml version="1.0" encoding="ISO-8859-1"?>
<world gridSize="20,20,20" windowSize="1600,900" maxSimulationDate="100000000">
    <camera target="10,10,10" directionSpherical="45,35,60" angle="35"/>
    <spotlight target="10,10,10" directionSpherical="30,50,80" angle="45"/>
    
    <blockList color="128,128,128" blocksize="10.0">
        <!-- Base (Cluster 1): 5 modules -->
        <block position="10,10,10" id="1"/>  <!-- iCH -->
        <block position="11,10,10" id="2"/>  <!-- CM -->
        <block position="9,10,10" id="3"/>   <!-- CM -->
        <block position="10,11,10" id="4"/>  <!-- CM -->
        <block position="10,9,10" id="5"/>   <!-- CM -->
        
        <!-- Left Wall (Cluster 2): 5 modules -->
        <block position="9,10,11" id="6"/>   <!-- CH -->
        <block position="9,11,11" id="7"/>   <!-- CM -->
        <block position="9,9,11" id="8"/>    <!-- CM -->
        <block position="9,10,12" id="9"/>   <!-- CM -->
        <block position="8,10,11" id="10"/>  <!-- CM -->
        
        <!-- Right Wall (Cluster 3): 5 modules -->
        <block position="11,10,11" id="11"/> <!-- CH -->
        <block position="11,11,11" id="12"/> <!-- CM -->
        <block position="11,9,11" id="13"/>  <!-- CM -->
        <block position="11,10,12" id="14"/> <!-- CM -->
        <block position="12,10,11" id="15"/> <!-- CM -->
    </blockList>
</world>
```

**Expected Behavior**:
1. Skeleton: iCH (1) ↔ CH (6) ↔ CH (11)
2. Cluster 1 (base): 1 → 2, 3, 4, 5
3. Cluster 2 (left): 6 → 7, 8, 9, 10
4. Cluster 3 (right): 11 → 12, 13, 14, 15
5. Cup shape visible
6. Color coding: RED (base), GREEN (left), BLUE (right)

---

## Configuration 6: Large Scale (50 modules)

**Purpose**: Test scalability and performance

```xml
<?xml version="1.0" encoding="ISO-8859-1"?>
<world gridSize="30,30,30" windowSize="1920,1080" maxSimulationDate="500000000">
    <camera target="15,15,15" directionSpherical="45,35,80" angle="40"/>
    <spotlight target="15,15,15" directionSpherical="30,50,100" angle="50"/>
    
    <blockList color="128,128,128" blocksize="10.0">
        <!-- Generate 50 modules in a 5x5x2 grid -->
        <!-- Layer 1 (z=10) -->
        <block position="10,10,10" id="1"/>  <!-- iCH -->
        <block position="11,10,10" id="2"/>
        <block position="12,10,10" id="3"/>
        <block position="13,10,10" id="4"/>
        <block position="14,10,10" id="5"/>
        
        <block position="10,11,10" id="6"/>
        <block position="11,11,10" id="7"/>
        <block position="12,11,10" id="8"/>
        <block position="13,11,10" id="9"/>
        <block position="14,11,10" id="10"/>
        
        <!-- ... continue pattern for 5x5 grid ... -->
        <!-- Total: 25 modules in layer 1 -->
        
        <!-- Layer 2 (z=11) -->
        <!-- ... 25 more modules ... -->
        <!-- Total: 50 modules -->
    </blockList>
</world>
```

**Expected Behavior**:
- Test O(w) complexity
- Measure formation time
- Verify memory usage O(1) per module
- Count total messages
- Verify all 50 modules authenticate

---

## Configuration 7: Clock Desynchronization Test

**Purpose**: Test ±5 tolerance

```xml
<?xml version="1.0" encoding="ISO-8859-1"?>
<world gridSize="20,20,20" windowSize="1600,900" maxSimulationDate="10000000">
    <camera target="10,10,10" directionSpherical="45,35,60" angle="35"/>
    <spotlight target="10,10,10" directionSpherical="30,50,80" angle="45"/>
    
    <blockList color="128,128,128" blocksize="10.0">
        <block position="10,10,10" id="1"/>  <!-- iCH -->
        <block position="11,10,10" id="2"/>  <!-- CH1 -->
    </blockList>
    
    <!-- Simulate clock drift in code -->
    <!-- Module 2 clock offset: +40 units (within ±50 tolerance) -->
</world>
```

**Test Cases**:
1. Offset = 0: Should succeed
2. Offset = +5: Should succeed (at tolerance limit)
3. Offset = -5: Should succeed (at tolerance limit)
4. Offset = +6: Should fail (beyond tolerance)
5. Offset = -6: Should fail (beyond tolerance)

---

## Configuration 8: Attack Scenario Test

**Purpose**: Test replay attack prevention

```xml
<?xml version="1.0" encoding="ISO-8859-1"?>
<world gridSize="20,20,20" windowSize="1600,900" maxSimulationDate="20000000">
    <camera target="10,10,10" directionSpherical="45,35,60" angle="35"/>
    <spotlight target="10,10,10" directionSpherical="30,50,80" angle="45"/>
    
    <blockList color="128,128,128" blocksize="10.0">
        <block position="10,10,10" id="1"/>  <!-- iCH -->
        <block position="11,10,10" id="2"/>  <!-- CH1 (legitimate) -->
        <block position="12,10,10" id="3"/>  <!-- CH2 (attacker) -->
    </blockList>
</world>
```

**Test Scenario**:
1. CH1 (2) authenticates with iCH (1)
2. CH2 (3) captures message from CH1
3. CH2 (3) replays message to iCH
4. Expected: iCH rejects replay (timestamp mismatch)

---

## Configuration 9: Parallel vs Serial Comparison

**Purpose**: Compare formation models

### Serial Model Config
```xml
<!-- In code: #define SCUP_FORMATION_MODEL SERIAL_MODEL -->
<world gridSize="20,20,20" windowSize="1600,900" maxSimulationDate="50000000">
    <blockList color="128,128,128" blocksize="10.0">
        <!-- 10 CHs in a line -->
        <block position="10,10,10" id="1"/>  <!-- iCH -->
        <block position="11,10,10" id="2"/>
        <block position="12,10,10" id="3"/>
        <block position="13,10,10" id="4"/>
        <block position="14,10,10" id="5"/>
        <block position="15,10,10" id="6"/>
        <block position="16,10,10" id="7"/>
        <block position="17,10,10" id="8"/>
        <block position="18,10,10" id="9"/>
        <block position="19,10,10" id="10"/>
    </blockList>
</world>
```

### Parallel Model Config
```xml
<!-- In code: #define SCUP_FORMATION_MODEL PARALLEL_MODEL -->
<!-- Same XML as above -->
```

**Comparison Metrics**:
- Formation time: Serial vs Parallel
- Number of keys: 1 vs 9
- Message count: Same (2 per link)
- Hash operations: Same (6 per link)

---

## Configuration 10: 3D Lattice Structure

**Purpose**: Test Catoms3D-specific 3D geometry

```xml
<?xml version="1.0" encoding="ISO-8859-1"?>
<world gridSize="20,20,20" windowSize="1600,900" maxSimulationDate="100000000">
    <camera target="10,10,10" directionSpherical="45,35,60" angle="35"/>
    <spotlight target="10,10,10" directionSpherical="30,50,80" angle="45"/>
    
    <blockList color="128,128,128" blocksize="10.0">
        <!-- 3D cube: 3x3x3 = 27 modules -->
        <!-- Center layer (z=10) -->
        <block position="9,9,10" id="1"/>
        <block position="10,9,10" id="2"/>
        <block position="11,9,10" id="3"/>
        <block position="9,10,10" id="4"/>
        <block position="10,10,10" id="5"/>  <!-- iCH (center) -->
        <block position="11,10,10" id="6"/>
        <block position="9,11,10" id="7"/>
        <block position="10,11,10" id="8"/>
        <block position="11,11,10" id="9"/>
        
        <!-- Bottom layer (z=9) -->
        <block position="9,9,9" id="10"/>
        <block position="10,9,9" id="11"/>
        <block position="11,9,9" id="12"/>
        <block position="9,10,9" id="13"/>
        <block position="10,10,9" id="14"/>
        <block position="11,10,9" id="15"/>
        <block position="9,11,9" id="16"/>
        <block position="10,11,9" id="17"/>
        <block position="11,11,9" id="18"/>
        
        <!-- Top layer (z=11) -->
        <block position="9,9,11" id="19"/>
        <block position="10,9,11" id="20"/>
        <block position="11,9,11" id="21"/>
        <block position="9,10,11" id="22"/>
        <block position="10,10,11" id="23"/>
        <block position="11,10,11" id="24"/>
        <block position="9,11,11" id="25"/>
        <block position="10,11,11" id="26"/>
        <block position="11,11,11" id="27"/>
    </blockList>
</world>
```

**Expected Behavior**:
- Center module (5) has up to 6 neighbors in 3D
- Test 3D connectivity
- Verify all 27 modules authenticate
- Test with up to 12 neighbors per Catoms3D

---

## Test Execution Commands

### Compile
```bash
cd applicationsSrc/scupCatoms3D
make clean
make
```

### Run Tests
```bash
cd applicationsBin/scupCatoms3D

# Test 1: Minimal
./scupCatoms3D -c config_minimal.xml -t -s 10000000

# Test 2: Small skeleton
./scupCatoms3D -c config_small.xml -t -s 20000000

# Test 3: Cross skeleton
./scupCatoms3D -c config_cross.xml -t -s 30000000

# Test 4: Simple cluster
./scupCatoms3D -c config_cluster.xml -t -s 50000000

# Test 5: Cup structure
./scupCatoms3D -c config_cup.xml -t -s 100000000

# Test 6: Large scale
./scupCatoms3D -c config_large.xml -t -s 500000000
```

### Flags
- `-c config.xml`: Configuration file
- `-t`: Terminal mode (no GUI)
- `-s N`: Max simulation date (µs)
- `-g`: GUI mode (default)

---

## Expected Output Format

```
=== S-CUP Protocol Initialization ===
Module 1 starting up
Position: (10,10,10)
Role: iCH (Initial Cluster Head)
Cluster ID: 1
Model: SERIAL

[100] Module 1 : Broadcasting START_FORMATION (4 neighbors)
[200] Module 1 : Starting skeleton formation

[300] Module 2 : Received START_FORMATION
[300] Module 2 : ALGORITHM 1 - INITIATION (SF-1, SF-2)
    Role: CH
    Phase: SKELETON
    n0 generated, n1 = H(n0), K0 = HL(n0)
    Ts = 30, x = Ts ⊕ n0
    Sending AUTH_REQUEST

[400] Module 1 : ALGORITHM 1 - VERIFICATION (SF-3)
    Role: iCH
    Already in skeleton
    n0' = x ⊕ Ts' (offset=0)
    Ts_base = (Tr - Δt)/10 = 30
    VERIFICATION SUCCESSFUL
    - Synchronization OK
    - Identical code (K0 verified)
    iCH generates n2 (SERIAL_MODEL)
    K1 = HL(n2 mod Nb) generated
    Shared key K1 established
    Sending KEY_CHALLENGE (x1 = n2 ⊕ n0')

[500] Module 2 : ALGORITHM 1 - COMPLETION (SF-5)
    n2 = x1 ⊕ n0
    K1 = HL(n2 mod Nb)
    AUTHENTICATION SUCCESSFUL
    Shared key K1 established
    This K1 is identical for entire skeleton (SERIAL_MODEL)
    [2/10] modules authenticated

...

╔════════════════════════════════════════════════════════════════╗
║         S-CUP STATISTICS (WINCOM 2025)                        ║
╠════════════════════════════════════════════════════════════════╣
║ Formation model      : SERIAL (1 key)                         ║
╠════════════════════════════════════════════════════════════════╣
║ Authenticated modules:  10 /  10                              ║
║ Messages exchanged   :     18                                 ║
║ Formation time (µs)  :   45000                                ║
║ Msg per module (avg) :   1.80                                 ║
║ Time per module (µs) :  4500.00                               ║
╠════════════════════════════════════════════════════════════════╣
║ Algo1 complexity     : O(w) — w links                         ║
║ Overhead per link    : 6H + 2Tx + 4XOR                        ║
╚════════════════════════════════════════════════════════════════╝
```

---

## Success Criteria per Configuration

| Config | Modules | Expected Time | Expected Messages | Success Criteria |
|--------|---------|---------------|-------------------|------------------|
| 1 | 2 | ~1000 µs | 2 | Both authenticate |
| 2 | 3 | ~2000 µs | 4 | All share K1 |
| 3 | 5 | ~4000 µs | 8 | Cross formed |
| 4 | 9 | ~16000 µs | 16 | All clusters formed |
| 5 | 15 | ~28000 µs | 28 | Cup structure |
| 6 | 50 | ~98000 µs | 98 | All authenticate |
| 7 | 2 | Varies | 2 | Tolerance works |
| 8 | 3 | ~2000 µs | 4 | Replay rejected |
| 9 | 10 | Varies | 18 | Compare models |
| 10 | 27 | ~52000 µs | 52 | 3D structure |
