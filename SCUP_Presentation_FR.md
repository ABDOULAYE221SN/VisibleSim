---
marp: true
theme: default
paginate: true
backgroundColor: #fff
---

# Protocole S-CUP
## Protocole de Sécurité pour l'Auto-reconfiguration par Clustering

**Implémentation pour les Robots Modulaires Catoms3D**

Basé sur l'article WINCOM 2025

---

# Agenda

1. Introduction & Contexte
2. Vue d'ensemble du protocole
3. Primitives cryptographiques
4. Architecture en trois phases
5. Algorithme 1 : Authentification mutuelle
6. Gestion des clusters
7. Détails d'implémentation
8. Performance & Statistiques

---

# 1. Introduction

## Qu'est-ce que S-CUP ?

**S-CUP** = **S**ecurity Protocol for Self-reconfiguration by **C**l**u**stering on **P**rogrammable Matter

### Objectifs clés :
- ✅ Authentification sécurisée entre robots modulaires
- ✅ Cryptographie légère (appareils à ressources limitées)
- ✅ Synchronisation temporelle avec tolérance
- ✅ Preuve d'exécution de code identique
- ✅ Organisation basée sur les clusters

---

# 2. Vue d'ensemble du protocole

## Composants de l'architecture

```
┌─────────────────────────────────────────┐
│  Réseau de Robots Modulaires Catoms3D   │
├─────────────────────────────────────────┤
│  Phase 1 : Formation du squelette (CHs) │
│  Phase 2 : Formation des clusters (CMs) │
│  Phase 3 : Opérationnel (Inter-cluster) │
└─────────────────────────────────────────┘
```

### Rôles des modules :
- **iCH** (Initial Cluster Head) - Module 1
- **CH** (Cluster Head) - Leaders des clusters
- **CM** (Cluster Member) - Membres réguliers

---

# 3. Primitives cryptographiques

## Fonction de hachage SPONGENT-160

```cpp
H(data) → 160 bits
```

### Fonctions clés :

| Fonction | Description | Sortie |
|----------|-------------|--------|
| `H(x)` | Hachage SPONGENT-160 | 160 bits |
| `L(i)` | Ligne i du code source | 256 bits |
| `HL(n)` | H(L(n mod Nb)) | 160 bits |
| `XOR` | OU exclusif bit à bit | 160 bits |

### Paramètres :
- **Nb** = 128 lignes de code
- **Taille du hash** = 160 bits (20 octets)
- **Diviseur temporel** = 10 (Ts = ts/10)

---

# 4. Architecture en trois phases

## Phase 1 : Formation du squelette 🔴

**Objectif** : Authentifier tous les Cluster Heads (CHs)

- iCH (Module 1) s'authentifie avec tous les CHs
- Les CHs s'authentifient entre eux
- Forme l'épine dorsale sécurisée

---

## Phase 2 : Formation des clusters 🟢

**Objectif** : Authentifier les Cluster Members (CMs)

- Chaque CH authentifie ses 3 CMs
- 4 modules par cluster (1 CH + 3 CMs)
- Codage couleur par cluster

**Organisation des clusters :**
```
Cluster 1 : Modules 1-4   (ROUGE)
Cluster 2 : Modules 5-8   (VERT)
Cluster 3 : Modules 9-12  (BLEU)
```

---

## Phase 3 : Opérationnel 🔵

**Objectif** : Authentification inter-clusters

- Les modules de différents clusters s'authentifient
- Connectivité complète du réseau
- Système prêt pour la reconfiguration

---

# 5. Algorithme 1 : Authentification mutuelle

## Protocole en cinq étapes (SF-1 à SF-5)

```
CHn (Demandeur)                    iCH (Vérificateur)
───────────────                    ──────────────────
SF-1 : Génère n0, n1, K0
       x = Ts ⊕ n0
                                   
SF-2 : ──────(n1, x, K0)──────────>
                                   SF-3 : Vérifie sync & code
                                          n0' = x ⊕ Ts'
                                          Vérifie : H(n0') = n1
                                          Vérifie : HL(n0' mod Nb) = K0
                                   
                                   SF-4 : Génère n2
                                          x1 = n2 ⊕ n0'
<─────────────(x1)─────────────────
                                   
SF-5 : n2 = x1 ⊕ n0
       K1 = HL(n2 mod Nb)
       ✅ Clé partagée établie
```

---

# SF-1 & SF-2 : Initiation

## Actions du demandeur (CHn) :

```cpp
// SF-1 : Génère les données d'authentification
n0 = generateNonce160()           // Nonce secret
n1 = H(n0)                        // Empreinte d'authentification
K0 = HL(n0 mod Nb)                // Preuve de code

// Protège n0 avec l'horodatage
Ts = round(ts / 10)
x = Ts ⊕ n0                       // Nonce protégé temporellement

// SF-2 : Envoie la requête d'authentification
send(n1, x, K0) → iCH
```

**Taille du message** : 3 × 160 bits = 480 bits

---

# SF-3 & SF-4 : Vérification

## Actions du vérificateur (iCH) :

```cpp
// SF-3 : Récupère n0 avec tolérance de synchronisation
Ts' = round((Tr - Δt) / 10)

for offset in [-5, +5]:           // Tolérance ±5
    n0' = x ⊕ (Ts' + offset)
    if H(n0') == n1:              // Vérifie l'identité
        if HL(n0' mod Nb) == K0:  // Vérifie le code
            ✅ Authentification valide
            break

// SF-4 : Génère la clé partagée
n2 = generateNonce160()
x1 = n2 ⊕ n0'
K1 = HL(n2 mod Nb)                // Clé partagée
send(x1) → CHn
```

---

# SF-5 : Finalisation

## Étape finale du demandeur (CHn) :

```cpp
// SF-5 : Calcule la clé partagée
n2 = x1 ⊕ n0
K1 = HL(n2 mod Nb)

✅ Authentification mutuelle complète
✅ Clé partagée K1 établie
```

### Propriétés de sécurité :
- ✅ Authentification mutuelle
- ✅ Synchronisation temporelle vérifiée
- ✅ Code identique prouvé
- ✅ Échange de clés sécurisé

---

# 6. Gestion des clusters

## Organisation des clusters

```
┌──────────────────────────────────────┐
│ Cluster 1 (ROUGE)  - Modules 1-4     │
│   • Module 1 : iCH (CH initial)      │
│   • Modules 2-4 : CMs                │
├──────────────────────────────────────┤
│ Cluster 2 (VERT)   - Modules 5-8     │
│   • Module 5 : CH                    │
│   • Modules 6-8 : CMs                │
├──────────────────────────────────────┤
│ Cluster 3 (BLEU)   - Modules 9-12    │
│   • Module 9 : CH                    │
│   • Modules 10-12 : CMs              │
└──────────────────────────────────────┘
```

**Calcul de l'ID du cluster** : `(blockId - 1) / 4 + 1`

---

# Modèles de formation

## Série vs Parallèle

### Modèle série (par défaut)
```
• Une clé partagée pour tout le squelette
• K1 est identique pour toutes les paires authentifiées
• Formation plus rapide
• Utilisation mémoire réduite
```

### Modèle parallèle
```
• Clé unique par paire
• K1 différente pour chaque connexion
• Sécurité accrue
• Plus de mémoire requise
```

**Configuration** : `#define SCUP_FORMATION_MODEL SERIAL_MODEL`

---

# 7. Détails d'implémentation

## Types de messages

| Type | Code | Description |
|------|------|-------------|
| AUTH_REQUEST | 5001 | (n1, x, K0) - 480 bits |
| KEY_CHALLENGE | 5002 | (x1) - 160 bits |
| SKELETON_READY | 5003 | Notification |
| CLUSTER_READY | 5004 | Notification |
| START_FORMATION | 5005 | Signal de déclenchement |

---

## Classes principales

### SCupCatoms3DCode
```cpp
class SCupCatoms3DCode : public Catoms3DBlockCode {
    // Informations de sécurité
    SecurityInfo security;
    
    // Gestion des voisins
    map<P2PNetworkInterface*, vector<uint8_t>> neighborKeys;
    map<P2PNetworkInterface*, bool> authenticatedNeighbors;
    
    // Fonctions de l'algorithme 1
    void algorithm1_Initiate(P2PNetworkInterface* dest);
    void algorithm1_Verify(...);
    void algorithm1_Complete(...);
    
    // Gestion des phases
    void startSkeletonFormation();
    void startClusterFormation();
    void startOperationalPhase();
};
```

---

## États de sécurité

```cpp
enum SecurityState {
    STATE_UNLINKED,          // Non connecté
    STATE_AUTHENTICATING,    // Authentification en cours
    STATE_AUTHENTICATED,     // Identité vérifiée
    STATE_KEY_ESTABLISHED    // Clé partagée prête
};

enum ProtocolPhase {
    PHASE_WAITING,           // En attente de déclenchement
    PHASE_SKELETON,          // Phase 1 active
    PHASE_CLUSTERING,        // Phase 2 active
    PHASE_OPERATIONAL        // Phase 3 active
};
```

---

# 8. Performance & Statistiques

## Analyse de complexité

### Complexité temporelle
- **O(w)** où w = nombre de liens
- Linéaire avec la taille du réseau

### Surcharge par lien
```
6H   : 6 opérations de hachage
2Tx  : 2 transmissions
4XOR : 4 opérations XOR
```

### Surcharge de messages
- **Phase 1** : 2 messages par paire de CH
- **Phase 2** : 2 messages par CM
- **Phase 3** : 2 messages par lien inter-cluster

---

## Affichage des statistiques

```
╔════════════════════════════════════════════════╗
║         STATISTIQUES S-CUP                     ║
╠════════════════════════════════════════════════╣
║ Modèle de formation  : SÉRIE (1 clé)          ║
╠════════════════════════════════════════════════╣
║ Modules authentifiés :  12 /  12               ║
║ Messages échangés    :    48                   ║
║ Temps de formation   :   15000 µs              ║
║ Msg par module (moy) :   4.00                  ║
║ Temps par module     : 1250.00 µs              ║
╠════════════════════════════════════════════════╣
║ Complexité Algo1     : O(w) - w liens          ║
║ Surcharge par lien   : 6H + 2Tx + 4XOR         ║
╚════════════════════════════════════════════════╝
```

---

# Flux d'exécution

## Séquence de démarrage

```
1. Initialisation des modules
   ├─ Détermine l'ID du cluster
   ├─ Assigne le rôle (iCH/CH/CM)
   └─ Définit la couleur initiale

2. Phase 1 : Formation du squelette
   ├─ iCH attend les requêtes des CHs
   ├─ Les CHs initient l'authentification
   └─ Squelette complet → Pause

3. Phase 2 : Formation des clusters
   ├─ Les CHs attendent les requêtes des CMs
   ├─ Les CMs s'authentifient avec leur CH
   └─ Tous les clusters complets → Pause

4. Phase 3 : Opérationnel
   ├─ Authentification inter-clusters
   └─ Système prêt
```

---

# Synchronisation temporelle

## Mécanisme de tolérance

```cpp
// Paramètres de synchronisation
#define SCUP_TIME_DIVISOR      10    // Ts = ts/10
#define SCUP_TIME_TOLERANCE     5    // Tolérance ±5
#define SCUP_TRANSMISSION_DELAY 100  // Δt = 100 µs

// Vérification avec tolérance
Ts_base = (Tr - Δt) / 10

for offset in [-5, +5]:
    Ts_test = Ts_base + offset
    n0_test = x ⊕ Ts_test
    if H(n0_test) == n1:
        ✅ Synchronisation réussie
```

**Fenêtre de tolérance** : ±50 µs (5 × 10)

---

# Mécanisme de preuve de code

## Garantir un code identique

```cpp
// Chaque module a Nb = 128 lignes de code
// Chaque ligne L(i) fait 256 bits

// Génération de la preuve (SF-1)
K0 = H(L(n0 mod Nb))

// Vérification de la preuve (SF-3)
if HL(n0' mod Nb) == K0:
    ✅ Même code en exécution
else:
    ❌ Code différent - rejet
```

### Avantage sécuritaire :
- Empêche l'injection de code malveillant
- Garantit que tous les modules exécutent le même firmware
- Vérification légère (un seul hachage)

---

# Retour visuel

## Système de codage couleur

| Couleur | Signification |
|---------|---------------|
| 🔴 ROUGE | Cluster 1 (authentifié) |
| 🟢 VERT | Cluster 2 (authentifié) |
| 🔵 BLEU | Cluster 3 (authentifié) |
| ⚪ GRIS | Module non authentifié |
| 🟠 ORANGE | Cluster Head (avant auth) |
| 🔷 CYAN | Authentification en cours |

**Visualisation en temps réel** de la progression du protocole

---

# Résumé des fonctionnalités clés

## ✅ Sécurité
- Authentification mutuelle
- Cryptographie légère (SPONGENT-160)
- Preuve d'exécution de code
- Échange de clés sécurisé

## ✅ Robustesse
- Synchronisation temporelle avec tolérance
- Organisation basée sur les clusters
- Progression phase par phase
- Gestion des erreurs

## ✅ Efficacité
- Complexité O(w)
- Surcharge de messages minimale
- Support de l'authentification parallèle
- Économe en ressources

---

# Options de configuration

## Paramètres personnalisables

```cpp
// Cryptographie
#define SCUP_CODE_LINES         128
#define SCUP_LINE_SIZE_BITS     256
#define SCUP_HASH_SIZE_BYTES     20

// Temporisation
#define SCUP_TIME_DIVISOR        10
#define SCUP_TIME_TOLERANCE       5
#define SCUP_TRANSMISSION_DELAY 100

// Formation
#define SCUP_FORMATION_MODEL SERIAL_MODEL
// Options : SERIAL_MODEL, PARALLEL_MODEL

// Couleurs
#define SCUP_CLUSTER_1_COLOR  RED
#define SCUP_CLUSTER_2_COLOR  GREEN
#define SCUP_CLUSTER_3_COLOR  BLUE
```

---

# Cas d'utilisation

## Applications

1. **Reconfiguration sécurisée**
   - Morphing de forme avec authentification
   - Changements de topologie vérifiés

2. **Systèmes distribués**
   - Réseaux de capteurs sécurisés
   - Essaims de robots authentifiés

3. **Infrastructure critique**
   - Nanorobots médicaux
   - Modules d'exploration spatiale

4. **Recherche**
   - Validation de protocoles
   - Benchmarking de performance

---

# Avantages

## Pourquoi S-CUP ?

### 🚀 Performance
- Authentification rapide (microsecondes)
- Faible surcharge de messages
- Évolutif pour les grands réseaux

### 🔒 Sécurité
- Cryptographiquement sécurisé
- Résistant aux attaques par rejeu
- Vérification de l'intégrité du code

### 💡 Praticité
- Facile à implémenter
- Paramètres configurables
- Retour visuel

---

# Améliorations futures

## Améliorations potentielles

1. **Clustering dynamique**
   - Tailles de clusters adaptatives
   - Réorganisation à l'exécution

2. **Cryptographie avancée**
   - Algorithmes post-quantiques
   - Chiffrement hybride

3. **Tolérance aux pannes**
   - Gestion des pannes byzantines
   - Récupération automatique

4. **Optimisation**
   - Latence réduite
   - Efficacité énergétique

---

# Conclusion

## Résumé du protocole S-CUP

✅ **Sécurisé** - Authentification mutuelle avec preuve de code
✅ **Efficace** - Complexité O(w), surcharge minimale
✅ **Robuste** - Synchronisation temporelle avec tolérance
✅ **Pratique** - Implémenté pour les robots Catoms3D
✅ **Évolutif** - Architecture basée sur les clusters

### Innovation clé :
**Sécurité légère pour robots modulaires à ressources limitées**

---

# Références & Ressources

## Fichiers d'implémentation
- `scupCatoms3D.cpp` - Point d'entrée principal
- `SCupCatoms3DCode.h` - Définitions du protocole
- `SCupCatoms3DCode.cpp` - Implémentation principale

## Documentation
- Article WINCOM 2025
- Framework VisibleSim
- Spécification SPONGENT-160

## Contact
Pour toute question sur cette implémentation, consultez la documentation du code source.

---

# Merci !

## Questions ?

**Protocole S-CUP**
Protocole de Sécurité pour l'Auto-reconfiguration par Clustering

*Implémentation pour les Robots Modulaires Catoms3D*

---
