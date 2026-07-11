# Modifications MonApplicationTest

## Objectif
Déplacer le module falsifié de la position (4,1,2) vers (3,3,2), et transformer l'ancien module falsifié en module légitime.

## Changements effectués

### 1. Configuration XML (`applicationsBin/MonApplicationTest/config.xml`)

**Avant :**
```xml
<block position="3,3,2"/>              <!-- légitime -->
<block position="4,1,2" color="128,0,128"/>  <!-- falsifié -->
```

**Après :**
```xml
<block position="4,1,2"/>              <!-- légitime -->
<block position="3,3,2" color="128,0,128"/>  <!-- falsifié -->
```

### 2. Positions et cibles (fichiers .cpp et .hpp)

**Avant :**
```cpp
POS_B(3,3,2)        → TARGET_B(6,2,2)   // légitime
POS_FAKE(4,1,2)     → TARGET_FAKE(6,2,2) // falsifié
```

**Après :**
```cpp
POS_FAKE(3,3,2)     → TARGET_FAKE(6,1,2) // falsifié
POS_B(4,1,2)        → TARGET_B(6,2,2)    // légitime
```

### 3. Ordre de déplacement

**Avant :**
```
Séquence : A → D → [FAKE après index 2] → E → B → F → iM
           POS_A, POS_D, POS_B, POS_E, POS_F
```

**Après :**
```
Séquence : A → [FAKE après index 1] → D → E → F → B → iM
           POS_A, POS_D, POS_E, POS_F, POS_B
```

### 4. Timing d'intercalation

```cpp
// Avant
static const int FAKE_AFTER = 2;  // après POS_D

// Après
static const int FAKE_AFTER = 1;  // après POS_A
```

## Comportement attendu

1. **Module A** (2,3,2) se déplace vers (5,3,2)
2. **Module FAKE** (3,3,2) se déplace vers (6,1,2)
   - Tente l'authentification avec K0 invalide
   - Reçoit AUTH_ECHEC
   - **Retourne à (3,3,2)** en rouge
3. **Module D** (2,2,2) se déplace vers (6,3,2)
4. **Module E** (3,2,2) se déplace vers (6,1,2)
5. **Module F** (4,2,2) se déplace vers (6,0,2)
6. **Module B** (4,1,2) se déplace vers (6,2,2) - **maintenant légitime**
7. **Module iM** (4,3,2) se déplace vers (7,3,2)

## Fichiers modifiés

- `applicationsBin/MonApplicationTest/config.xml`
- `applicationsSrc/MonApplicationTest/MonApplicationTestCode.cpp`
- `applicationsSrc/MonApplicationTest/MonApplicationTestCode.hpp`

## Compilation

```bash
cd applicationsSrc/MonApplicationTest
make clean
make
```

## Exécution

```bash
./applicationsBin/MonApplicationTest/MonApplicationTest \
  applicationsBin/MonApplicationTest/config.xml
```
