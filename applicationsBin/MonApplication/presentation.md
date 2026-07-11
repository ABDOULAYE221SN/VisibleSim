---
marp: true
theme: default
paginate: true
backgroundColor: #fff
---

# Protocole AKC-PM
## Authentication, Key Management and Confidentiality for Programmable Matter

**Implémentation sur robots modulaires Catoms3D (VisibleSim)**

Faye et al. — INISTA 2025

---

# Plan

1. Contexte & Problématique
2. Vue d'ensemble du protocole
3. Primitives cryptographiques
4. Algorithme 1 : Authentification nM → structure
5. Algorithme 2 : Preuve d'appartenance
6. Reconfiguration
7. Résultats & Statistiques

---

# 1. Contexte

## Matière programmable à base de robots modulaires

- Milliers de modules millimétriques identiques
- Chaque module : calcul, communication, mouvement
- Pas d'identifiant unique, pas de tiers de confiance
- Ressources très limitées (ATtiny45 @ 8 MHz)

## Défi sécurité

> Comment authentifier des modules **sans identifiant**, **sans secret partagé a priori**, dans un environnement **totalement distribué** ?

---

# 2. Vue d'ensemble AKC-PM

## Trois types d'authentification

```
(a) iM ↔ 1M   — Formation initiale de la structure
(b) iM ↔ nM   — Nouveau module rejoint la structure
(c) 1M ↔ 2M   — Deux modules déjà dans la structure
```

## Modèle de clé : SINGLE_KEY

```
iM génère n2 une seule fois
Tous les modules authentifiés partagent la même K1 = HL(n2 mod Nb)
```

## Simulation : 5 modules Catoms3D

```
Forme initiale → Authentification → Reconfiguration → Forme cible
```

---

# 3. Primitives cryptographiques

## Fonctions utilisées

| Fonction | Description | Taille sortie |
|----------|-------------|---------------|
| `H(x)` | SPONGENT-160 (hash léger) | 160 bits |
| `L(i)` | Ligne i du code source | 256 bits |
| `HL(n)` | H(L(n mod Nb)) | 160 bits |
| `XOR` | OU exclusif bit à bit | 160 bits |
| `Ts` | round(ts / 10) | 64 bits |

## Paramètres (conformes article)

- **Nb** = 128 lignes de code
- **Hash** = 160 bits = 20 octets (SPONGENT-160)
- **Tolérance sync** = ±5 unités → ±50 µs
- **Délai transmission** Δt = 100 µs

---

# 4. Algorithme 1 — Authentification nM → structure

## Principe

nM prouve qu'il exécute le **même code** que la structure,
sans jamais révéler son nonce secret n0.

```
nM (nouveau)                        iM (dans la structure)
────────────                        ──────────────────────
SF-1 : génère n0
       n1 = H(n0)
       K0 = HL(n0 mod Nb)
       x  = Ts ⊕ n0

SF-2 : ──── (liens, n1, x, K0) ────────────────────────>

                                    SF-3 : Ts' = (Tr-Δt)/10
                                           n0' = x ⊕ Ts'
                                           vérifie H(n0') = n1  ✓ sync
                                           vérifie HL(n0') = K0 ✓ code
                                    SF-4 : génère n2
                                           x1 = n2 ⊕ n0'
                                           K1 = HL(n2 mod Nb)
       <──────────────── (x1) ─────────────────────────

SF-5 : n2 = x1 ⊕ n0
       K1 = HL(n2 mod Nb)
       ✅ Clé K1 partagée établie
```

---

# Algorithme 1 — Détails SF-1 & SF-2

## Code nM (algorithme1_Initier)

```cpp
// SF-1 : génération des preuves
n0 = genererNonce160()          // nonce secret, jamais transmis
n1 = H(n0)                      // empreinte d'authentification
K0 = HL(n0 mod Nb)              // preuve d'exécution du même code

// Protection temporelle
Ts = round(ts / DIVISEUR_SYNC)
x  = Ts ⊕ n0                    // n0 masqué par l'horloge

// SF-2 : envoi à iM
send(liens, n1, x, K0) → iM
```

**Taille du message** : 3 × 160 bits = 480 bits

---

# Algorithme 1 — Détails SF-3 & SF-4

## Code iM (algorithme1_Verifier)

```cpp
// SF-3 : récupération de n0 avec tolérance ±5
Ts_base = (Tr - DELAI_TRANSMISSION) / DIVISEUR_SYNC

for offset in [-5 .. +5]:
    n0' = x ⊕ (Ts_base + offset)
    if H(n0') == n1:            // sync OK
        if HL(n0') == K0:       // code identique
            ✅ authentification valide

// SF-4 : SINGLE_KEY — iM génère n2 une seule fois
if n2 vide: n2 = genererNonce160()
x1 = n2 ⊕ n0'
K1 = HL(n2 mod Nb)
send(x1) → nM
```

---

# 5. Algorithme 2 — Preuve d'appartenance

## Cas : 1M (dans structure) veut se connecter à 2M (dans structure)

```
1M                    iM (relai)                  2M
──                    ──────────                  ──
SF-1 : génère n0
       n1 = H(n0)
       x  = K1 ⊕ n0

SF-2 : ── (sauts, x) ──────────>
       ── (sauts, n1) ──────────────────────────>

                      SF-3 : n0 = K1_src ⊕ x
                             xv = n0 ⊕ Kv
                             ── (sauts-1, xv) ──>

                                                  SF-3 : n0 = K2 ⊕ xv
                                                         vérifie H(n0) = n1 ✓
                                                  SF-5 : choisit K2_new
                                                         x1 = K2_new ⊕ n0
                                                         K3 = HL(K2_new)
       <──────────────────────────────── (x1) ───

SF-6 : K2 = x1 ⊕ n0
       K3 = HL(K2)
       ✅ Clé K3 partagée avec 2M
```

---

# Algorithme 2 — Innovation implémentation

## Sélection du relai (correction vs version initiale)

```cpp
// Chercher un voisin commun entre 1M et 2M (le vrai relai iM)
for chaque voisin authentifié candidat:
    if candidat est aussi voisin de 2M:
        ifaceRelai = candidat  // voisin commun trouvé
        break
// Fallback : premier voisin authentifié valide
```

## Compteur de sauts conforme à l'article

```cpp
// sauts = liens de 2M dans la structure
// (transmis via MessageStructurePrete.liensEmetteur)
int sauts = liensVoisinsDansStructure[dest];
```

---

# 6. Reconfiguration

## Forme initiale → Forme cible

```
Initiale                    Cible
─────────────────           ─────────────────
M1 (1,3,2)  ──────────────> (3,3,2)
M2 (2,3,2)  statique        (2,3,2)
M3 (1,2,2)  ──────────────> (2,2,3)
M4 (2,2,2)  statique (iM)   (2,2,2)
M5 (3,2,2)  statique        (3,2,2)
```

## Algorithme greedy

```
1. Calculer toutes les rotations possibles
2. Filtrer : position libre + au moins 1 voisin
3. Priorité aux positions non visitées
4. Choisir la plus proche de la cible (distance euclidienne)
5. Déplacer → onMotionEnd() → module suivant
```

---

# Séquence d'exécution complète

```
startup()
├── Module 1 : planifie les déplacements (moveQueue)
├── Module iM (2,2,2) : se désigne MODULE INITIAL
│   └── schedule(InterruptionEvent) → notifierVoisinsStructurePrete()
│
└── Tous les autres : attente STRUCTURE_READY (couleur GRISE)

Phase authentification
├── iM envoie STRUCTURE_READY à ses voisins
├── Chaque voisin reçoit → Algo1 → couleur ORANGE → VERTE
├── Chaque module authentifié propage STRUCTURE_READY
└── Quand tous authentifiés → afficherStats() → lancerReconfiguration()

Phase reconfiguration
├── M1 se déplace vers (3,3,2)
├── M3 se déplace vers (2,2,3)
└── Chaque arrivée → onMotionEnd() → re-authentification Algo2
```

---

# Codes couleur visuels

| Couleur | Signification |
|---------|---------------|
| 🔵 BLEU | Module initial iM |
| ⚪ GRIS | En attente d'authentification |
| 🟠 ORANGE | Authentification en cours (Algo1) |
| 🟢 VERT | Authentifié, clé K1 établie |

---

# 7. Statistiques

## Métriques collectées

```cpp
static uint64_t timeAuthStart;   // début de la simulation
static uint64_t timeAuthEnd;     // fin de la dernière authentification
static int      totalMessages;   // total messages échangés
static set<bID> authenticatedModules; // modules authentifiés
```

## Affichage automatique en fin d'authentification

```
╔════════════════════════════════════════════════════════════════╗
║         STATISTIQUES D'AUTHENTIFICATION AKC-PM                 ║
╠════════════════════════════════════════════════════════════════╣
║ Modèle de clé        : SINGLE_KEY                              ║
╠════════════════════════════════════════════════════════════════╣
║ Modules authentifiés :   5 /   5                               ║
║ Messages échangés    :     16                                  ║
║ Durée authentif.(µs) :    800                                  ║
║ Msg par module (moy) :   3.20                                  ║
║ Temps par module(µs) : 160.00                                  ║
╠════════════════════════════════════════════════════════════════╣
║ Complexité Algo1     : O(m) — 5 exécutions                     ║
║ Overhead par lien    : 6H + 2Tx + 4XOR                         ║
╚════════════════════════════════════════════════════════════════╝
```

---

# Analyse de complexité

## Conforme à l'article (Table II)

| Métrique | AKC-PM | PROLISEAN V1 | V3/V4 |
|----------|--------|--------------|-------|
| Complexité temps | O(1) | O(1) | O(n) |
| Complexité espace | O(1) | O(1) | O(1) |
| Overhead par lien | 6H+2Tx+4XOR | 2H+2Tx | 4H+4Tx |
| Protection replay | ✅ Oui | ❌ Non | ❌ Non |
| Preuve de code | ✅ Oui | ✅ Oui | ✅ Oui |

## Coût global

```
C1 = m × (6TH + 2Tx + 4XOR)    // Algo1 : m modules
C2 = (v·m/2 - m) × (4TH + 4Tx + 6XOR)  // Algo2
```

---

# Points clés de l'implémentation

## Conformité article

- ✅ SPONGENT-160 réel (pas simulé)
- ✅ Tolérance sync ±5 avec boucle offset
- ✅ SINGLE_KEY : n2 constant chez iM, K1 identique pour tous
- ✅ Relai Algo2 : voisin commun entre 1M et 2M
- ✅ Compteur de sauts = liens de 2M (via STRUCTURE_READY)
- ✅ liensStructure recalculé après chaque mouvement

## Robustesse

- Vérification validité interface avant tout envoi
- Protection contre les doublons (authentificationsEnCours)
- Purge des interfaces déconnectées après mouvement
- Fallback si voisin commun introuvable

---

# Conclusion

## Ce qui a été implémenté

✅ Protocole AKC-PM complet (Algo1 + Algo2, modèle SINGLE_KEY)
✅ Primitives SPONGENT-160 réelles
✅ Synchronisation temporelle avec tolérance
✅ Reconfiguration greedy (forme initiale → forme cible)
✅ Statistiques automatiques conformes à l'article
✅ Simulation sur 5 modules Catoms3D (VisibleSim)

## Perspectives

- Tester les modèles 4-KEY et MULTI-KEY
- Mesurer sur structures plus grandes (50, 100, 500 modules)
- Comparer avec PROLISEAN sur VisibleSim

---

# Merci

## Questions ?

**Protocole AKC-PM**
*Authentication, Key Management and Confidentiality for Programmable Matter*

Faye, Makhoul, Diene, Ouzzif — INISTA 2025

> Implémentation : `applicationsSrc/MonApplication/`
