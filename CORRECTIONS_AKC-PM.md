# Corrections du Protocole AKC-PM

## 📚 Référence
Article: "A Lightweight Distributed Security Protocol with Adaptive Key Management for Programmable Matter Based on Modular Robots"
Auteurs: Youssou Faye, Abdallah Makhoul, Serigne Mbacke Diene, Mohammed Ouzzif

## ✅ Corrections Appliquées

### 1. **Problème de Ré-authentification** (CORRIGÉ)

**Symptôme**: Les modules se ré-authentifiaient plusieurs fois, causant un comptage incorrect.

**Cause**: Aucune vérification de l'état d'authentification avant de traiter les messages.

**Solution**:
- Ajout de vérifications dans `algorithme1_Initier()`, `algorithme1_Verifier()`, `algorithme1_Completer()`
- Utilisation d'un `set<bID> authenticatedModules` pour éviter les doubles comptages
- Chaque module est compté exactement une fois

### 2. **Problème de Gestion de n2 pour SINGLE_KEY** (CORRIGÉ)

**Symptôme**: Utilisation d'une variable globale `n2Global` partagée entre tous les modules.

**Problème selon l'article**:
- Dans le modèle SINGLE_KEY, le module initial (iM) génère n2
- Ce n2 doit être **propagé** aux modules qui s'authentifient
- Chaque module **stocke localement** son n2 pour l'utiliser avec de nouveaux voisins

**Solution**:
- Suppression des variables statiques globales `n2Global` et `n2GlobalDefini`
- Utilisation de `infoSecurite.n2` (variable locale à chaque module)
- Le module initial génère n2 une seule fois
- Les autres modules reçoivent n2 lors de leur authentification (via x1)
- Chaque module utilise son n2 local pour authentifier de nouveaux voisins

### 3. **Amélioration du Debug** (AJOUTÉ)

**Ajouts**:
- Messages de debug détaillés dans `tryMoveToward()`
- Messages de debug dans `onMotionEnd()`
- Affichage des rotations possibles, candidats valides, distances
- Détection et signalement des blocages

## 📋 Conformité avec l'Article

### Algorithme 1 : Formation de Structure (SF-1 à SF-5)

✅ **SF-1 & SF-2**: Module nM → iM
- Génère n0, calcule n1 = H(n0)
- Calcule K0 = H(L(n0 mod Nb))
- Calcule x = Ts ⊕ n0
- Envoie (liens, n1, x||K0)

✅ **SF-3 & SF-4**: iM vérifie et répond
- Calcule Ts ≈ ((Tr - Δt)/10)
- Récupère n0' = x ⊕ Ts
- Vérifie synchronisation: n1 = H(n0')
- Vérifie authentification: K0 = H(L(n0' mod Nb))
- Génère/utilise n2, calcule x1 = n2 ⊕ n0'
- Calcule K1 = H(L(n2 mod Nb))
- Envoie x1

✅ **SF-5**: nM complète
- Calcule n2 = x1 ⊕ n0
- Calcule K1 = H(L(n2 mod Nb))
- **Stocke n2 localement** pour usage futur

### Modèle SINGLE_KEY (Fig. 3 de l'article)

✅ **Implémentation correcte**:
- Module initial (iM) génère n2 une seule fois
- n2 est propagé via x1 lors de l'authentification
- Chaque module stocke n2 dans `infoSecurite.n2`
- Les modules authentifiés utilisent leur n2 local pour authentifier de nouveaux voisins
- Tous les modules partagent la même clé K1 = H(L(n2 mod Nb))

## 🔐 Propriétés de Sécurité

✅ **Authentification mutuelle**: Les deux modules prouvent qu'ils exécutent le même code
✅ **Protection contre replay**: Synchronisation temporelle avec tolérance
✅ **Protection contre fabrication**: Utilisation de nonces et hash
✅ **Confidentialité**: Clé partagée K1 pour chiffrement
✅ **Pas de double authentification**: Vérifications avant chaque étape

## 🚀 Prochaines Étapes

1. Tester la simulation avec les corrections
2. Vérifier que la reconfiguration fonctionne correctement
3. Analyser les logs pour confirmer:
   - Authentification unique de chaque module
   - Comptage correct (5/5)
   - Propagation correcte de n2
   - Démarrage de la reconfiguration au bon moment

## 📊 Paramètres Actuels

- `NB_LIGNES_CODE = 128`
- `TAILLE_LIGNE_BITS = 256`
- `DIVISEUR_SYNC = 10`
- `TOLERANCE_SYNC = 5` (permet ±5 unités de désynchronisation)
- `DELAI_TRANSMISSION = 100`
- `TOTAL_MODULES = 5`
- `MAX_STEPS = 20` (pour la reconfiguration)

## 📝 Notes

- Le protocole implémente le modèle SINGLE_KEY (une clé pour tout le réseau)
- Le module initial est à la position (2,2,2)
- La reconfiguration démarre après authentification complète (5/5)
- Deux modules doivent se déplacer: M1 (1,3,2)→(3,3,2) et M3 (1,2,2)→(2,2,3)
