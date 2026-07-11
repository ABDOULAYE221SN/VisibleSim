# S-Cup Algorithm 1 - Visual Guide

## Overview

Algorithm 1 implements mutual authentication between two modules (CHn and iCH) with shared key establishment.

## Step-by-Step Visualization

```
Time →

CHn (Initiator)                                    iCH (Verifier)
═══════════════                                    ══════════════

SF-1: Preparation
─────────────────
Generate n0 (160 bits random)
    ↓
Compute n1 = H(n0)
    ↓
Compute K0 = H(L(n0 mod 128))
    ↓
Get timestamp: ts = now()
Compute Ts = ts / 10
    ↓
Protect nonce: x = Ts ⊕ n0
    ↓
Prepare message: (n1, x, K0)


SF-2: Send Request
──────────────────
                    ─────(n1, x, K0)────────────→
                    [Transmission delay: Δt]


                                                   SF-3: Verify
                                                   ─────────────
                                                   Receive at time Tr
                                                       ↓
                                                   Compute Ts ≈ (Tr - Δt) / 10
                                                       ↓
                                                   Try offsets -5 to +5:
                                                   For each offset:
                                                       Ts_test = Ts + offset
                                                       n0' = x ⊕ Ts_test
                                                       if H(n0') == n1:
                                                           SYNC OK!
                                                           break
                                                       ↓
                                                   Verify code:
                                                   if H(L(n0' mod 128)) == K0:
                                                       AUTH OK!
                                                       ↓
                                                   
                                                   SF-4: Challenge
                                                   ───────────────
                                                   Generate/reuse n2
                                                       ↓
                                                   Compute x1 = n2 ⊕ n0'
                                                       ↓
                                                   Compute K1 = H(L(n2 mod 128))
                                                       ↓
                                                   Store K1 for this neighbor
                                                       ↓
                    ←──────────(x1)─────────────
                    [Transmission delay: Δt]


SF-5: Complete
──────────────
Receive x1
    ↓
Recover: n2 = x1 ⊕ n0
    ↓
Compute K1 = H(L(n2 mod 128))
    ↓
Store K1 for this neighbor
    ↓
✓ MUTUAL AUTHENTICATION COMPLETE
✓ SHARED KEY K1 ESTABLISHED
```

## Data Flow Diagram

```
┌─────────────────────────────────────────────────────────────┐
│                    Algorithm 1 Data Flow                     │
└─────────────────────────────────────────────────────────────┘

Input:
  • CHn wants to authenticate with iCH
  • Both have synchronized clocks (±5 tolerance)
  • Both execute same code (128 lines)

Step 1 (SF-1): CHn generates
┌──────────┐
│   n0     │ (160 bits, random, SECRET)
└────┬─────┘
     │
     ├──→ H() ──→ n1 (160 bits, SENT)
     │
     ├──→ L(n0 mod 128) ──→ H() ──→ K0 (160 bits, SENT)
     │
     └──→ ⊕ Ts ──→ x (160 bits, SENT)

Step 2 (SF-2): CHn sends
┌─────────────────────┐
│  n1, x, K0          │ ──→ [Network] ──→ iCH
└─────────────────────┘

Step 3 (SF-3): iCH verifies
┌─────────────────────┐
│  n1, x, K0          │
└──────┬──────────────┘
       │
       ├──→ x ⊕ Ts ──→ n0' (recovered)
       │
       ├──→ H(n0') ?= n1 (sync check)
       │
       └──→ H(L(n0' mod 128)) ?= K0 (auth check)

Step 4 (SF-4): iCH generates
┌──────────┐
│   n2     │ (160 bits, random or reused)
└────┬─────┘
     │
     ├──→ ⊕ n0' ──→ x1 (160 bits, SENT)
     │
     └──→ L(n2 mod 128) ──→ H() ──→ K1 (160 bits, STORED)

┌─────────────────────┐
│       x1            │ ──→ [Network] ──→ CHn
└─────────────────────┘

Step 5 (SF-5): CHn derives
┌─────────────────────┐
│       x1            │
└──────┬──────────────┘
       │
       └──→ x1 ⊕ n0 ──→ n2 (recovered)
                │
                └──→ L(n2 mod 128) ──→ H() ──→ K1 (160 bits, STORED)

Output:
  • CHn and iCH both have K1
  • Mutual authentication complete
```

## Security Properties

### Nonce Protection
```
n0 is NEVER transmitted directly

Instead:
  x = Ts ⊕ n0  (sent in SF-2)
  x1 = n2 ⊕ n0  (sent in SF-4)

To recover n0:
  n0 = x ⊕ Ts  (requires correct timestamp)
  n0 = x1 ⊕ n2  (requires x1 from iCH)
```

### Synchronization Check
```
CHn sends: n1 = H(n0)

iCH tries offsets -5 to +5:
  Ts_test = Ts_base + offset
  n0' = x ⊕ Ts_test
  if H(n0') == n1:
      Clocks are synchronized!

This tolerates ±5 units of clock drift
```

### Code Authentication
```
CHn sends: K0 = H(L(n0 mod 128))

iCH verifies:
  K0' = H(L(n0' mod 128))
  if K0' == K0:
      CHn has same code!

This proves CHn executes legitimate code
```

### Replay Attack Prevention
```
Old message: (n1_old, x_old, K0_old)

Attacker replays at time Tr_new:
  Ts_new = (Tr_new - Δt) / 10
  n0' = x_old ⊕ Ts_new
  H(n0') ≠ n1_old  (FAIL!)

Timestamp mismatch prevents replay
```

## Key Generation Models

### Serial Model (Single Key)
```
iCH generates n2 ONCE:
  n2 = random()

For all CHs:
  iCH reuses same n2
  All CHs derive same K1

Result:
  ┌─────┐
  │ iCH │ ─── K1 ─── CH1
  └─────┘
     │
     K1
     │
    CH2 ─── K1 ─── CH3

One shared key for entire skeleton
```

### Parallel Model (Pairwise Keys)
```
Each pair generates new n2:
  CH1-CH2: n2_12 → K1_12
  CH1-CH3: n2_13 → K1_13
  CH2-CH3: n2_23 → K1_23

Result:
  ┌─────┐
  │ CH1 │ ─── K1_12 ─── CH2
  └─────┘
     │
   K1_13
     │
    CH3 ─── K1_23 ─── CH2

Unique key per pair
```

## Cluster Formation Extension

### CH to CM Authentication
```
CH (acts as iCH)
  │
  │ Algorithm 1
  │
  ↓
CM (acts as CHn)

CH can:
  • Reuse n2 from skeleton (Case 1)
  • Generate new n2' for cluster (Case 2)
  • Choose flexibly (Case 3)
```

### CM to CM Authentication
```
CH ─── Algorithm 1 ──→ CM1 (receives n2)
                        │
                        │ Algorithm 1
                        │ (reuses n2)
                        ↓
                       CM2

CM1 acts as iCH for CM2
CM1 reuses n2 from CH
Result: CH, CM1, CM2 share same K1
```

## Complexity Analysis

### Per Link
```
Operations:
  • 6 Hash operations (H)
  • 2 Transmissions (Tx)
  • 4 XOR operations (⊕)

Cost: 6H + 2Tx + 4XOR
```

### Total Structure
```
Given:
  • m modules
  • v neighbors per module
  • w = (v × m) / 2 links

Total cost: w × (6H + 2Tx + 4XOR)

Time complexity: O(w)
Space complexity: O(1) per module
```

## Message Sizes

```
AUTH_REQUEST (SF-2):
  ┌─────────────────────────┐
  │ n1:  160 bits (20 bytes)│
  │ x:   160 bits (20 bytes)│
  │ K0:  160 bits (20 bytes)│
  └─────────────────────────┘
  Total: 480 bits (60 bytes)

KEY_CHALLENGE (SF-4):
  ┌─────────────────────────┐
  │ x1:  160 bits (20 bytes)│
  └─────────────────────────┘
  Total: 160 bits (20 bytes)

Total per authentication: 640 bits (80 bytes)
```

## Implementation Checklist

- [ ] Implement H() using SPONGENT-160
- [ ] Implement L(i) for 128 lines
- [ ] Implement HL(n) = H(L(n mod 128))
- [ ] Implement XOR operation
- [ ] Implement nonce generation
- [ ] Implement timestamp function with tolerance
- [ ] Implement SF-1: Initiate
- [ ] Implement SF-2: Send request
- [ ] Implement SF-3: Verify with tolerance loop
- [ ] Implement SF-4: Challenge
- [ ] Implement SF-5: Complete
- [ ] Test synchronization tolerance
- [ ] Test code authentication
- [ ] Test key derivation
- [ ] Test replay attack prevention
- [ ] Test serial model (single key)
- [ ] Test parallel model (pairwise keys)
- [ ] Test cluster formation
