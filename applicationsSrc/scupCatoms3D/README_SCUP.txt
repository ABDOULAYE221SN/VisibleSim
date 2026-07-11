============================================================
  Résumé — Implémentation du protocole S-CUP
  S-CUP: A Security Protocol for Self-reconfiguration by
  Clustering on Programmable Matter based Modular Robots
  Faye et al. — WINCOM 2025
  Simulateur : VisibleSim — Catoms3D
============================================================


OBJECTIF
---------
S-CUP sécurise une structure de robots modulaires Catoms3D en
organisant les modules en clusters, puis en établissant des clés
cryptographiques partagées entre eux, sans secret partagé a priori
et sans tiers de confiance. Il fournit : authentification mutuelle,
gestion de clés, et confidentialité des communications.


ORGANISATION EN CLUSTERS
--------------------------
Les modules sont répartis en clusters de taille égale.
Chaque cluster contient :
  - 1 CH  (Cluster Head)   : chef du cluster, premier module du cluster
  - K CMs (Cluster Members): membres du cluster

Le module #1 est toujours l'iCH (Initial Cluster Head), point de
départ de toute la formation.

Convention d'IDs (obligatoire dans config.xml) :
  IDs consécutifs, CH en premier dans chaque cluster.
  Exemple (3 clusters × 3 modules) :
    Cluster 1 : modules 1 (iCH), 2, 3
    Cluster 2 : modules 4 (CH),  5, 6
    Cluster 3 : modules 7 (CH),  8, 9

La taille de cluster est détectée automatiquement au démarrage
(plus petit diviseur ≥ 2 de totalModules donnant ≥ 2 clusters).


PRIMITIVES CRYPTOGRAPHIQUES
-----------------------------
Identiques à AKC-PM, toutes basées sur SPONGENT-160 (160 bits) :

  H(x)   = SPONGENT-160 hash
  L(i)   = représentation déterministe 256 bits de la ligne i du code
  HL(n)  = H(L(n mod 128))   → preuve d'exécution du même code
  Ts     = floor(ts / 10)    → horodatage synchronisé
  XOR    = ou-exclusif 160 bits


ALGORITHME 1 — AUTHENTIFICATION MUTUELLE (SF-1 à SF-5)
--------------------------------------------------------
Exécuté sur chaque lien à sécuriser, entre deux modules voisins directs.

  SF-1 (initiateur) :
    Génère n0 (nonce secret, jamais transmis)
    n1 = H(n0)           → empreinte d'authentification
    K0 = HL(n0 mod Nb)   → preuve d'exécution du même code
    x  = Ts ⊕ n0         → masque n0 avec l'horloge (anti-rejeu)

  SF-2 : envoie (n1, x, K0) au vérificateur

  SF-3 (vérificateur) :
    Reconstruit n0' via Ts' ± tolérance ±5
    Vérifie H(n0') == n1   ✓ synchronisation
    Vérifie HL(n0') == K0  ✓ même code source

  SF-4 : génère ou réutilise n2 (selon modèle et phase)
    x1 = n2 ⊕ n0'
    K1 = HL(n2 mod Nb)   → clé partagée
    Envoie x1

  SF-5 (initiateur) :
    n2 = x1 ⊕ n0
    K1 = HL(n2 mod Nb)
    → Clé K1 établie des deux côtés


LES 3 PHASES DU PROTOCOLE
---------------------------

PHASE 1 — Formation du squelette (CHs uniquement)
  Chaque CH initie l'Algorithme 1 vers ses CHs voisins directs.
  L'iCH joue le rôle de vérificateur (SF-3/SF-4).
  Dès qu'un CH termine SF-5, il passe immédiatement en Phase 2
  sans attendre les autres CHs → les clusters se forment en parallèle.
  Une PAUSE de simulation marque la fin de Phase 1.

PHASE 2 — Formation des clusters (CMs)
  Le CH envoie un message SKELETON_READY à ses CMs directs.
  Chaque CM reçoit ce message et initie l'Algorithme 1 vers l'émetteur.
  Propagation en chaîne : un CM authentifié envoie SKELETON_READY à
  ses propres voisins CMs non encore authentifiés → la chaîne se propage
  jusqu'au dernier CM du cluster.

PHASE 3 — Phase opérationnelle (inter-cluster)
  Chaque module ayant des voisins d'un autre cluster initie
  l'Algorithme 1 avec eux → établissement de clés inter-cluster.


DEUX MODÈLES DE FORMATION
---------------------------
Sélectionné par une constante dans SCupCatoms3DCode.h :

  SERIAL_MODEL (défaut)
    L'iCH génère n2 une seule fois et le réutilise pour tous les liens.
    → Une seule clé K1 globale partagée par toute la structure.
    → Moins de calculs, moins de messages.

  PARALLEL_MODEL
    Phase 1 : nouveau n2 par paire de CHs → K1 différente par paire
    Phase 2 : nouveau n2_cluster par cluster → K1 différente par cluster
    Phase 3 : nouveau n2 par paire inter-cluster
    → Clés compartimentées, meilleure isolation des clusters.
    → Plus de calculs, plus de messages.


MESSAGES DÉFINIS
-----------------
  MSG_SCUP_AUTH_REQUEST   (5001) — SF-2 : (n1, x, K0)
  MSG_SCUP_KEY_CHALLENGE  (5002) — SF-4 : (x1)
  MSG_SCUP_SKELETON_READY (5003) — déclencheur Phase 2 (CH → CMs)


CODES COULEUR VISUELS
----------------------
  Gris    — module non encore authentifié
  Rouge   — Cluster 1 (iCH et ses membres)
  Vert    — Cluster 2
  Bleu    — Cluster 3
  Jaune   — Cluster 4
  Magenta — Cluster 5


CONFIGURATION (config.xml)
---------------------------
  3 clusters × 3 modules = 9 modules
  Cluster 1 (ROUGE) : modules 1 (iCH), 2, 3  — positions (5,5,5) (4,5,5) (5,4,5)
  Cluster 2 (VERT)  : modules 4 (CH),  5, 6  — positions (6,5,5) (7,5,5) (6,4,5)
  Cluster 3 (BLEU)  : modules 7 (CH),  8, 9  — positions (5,6,5) (5,7,5) (4,6,5)


COMPLEXITÉ (conforme à l'article)
-----------------------------------
  Complexité temporelle : O(w)  où w = nombre de liens dans la structure
  Coût par lien         : 6H + 2Tx + 4XOR
  Coût total            : c = (v × m / 2) × (6H + 2Tx + 4XOR)
    v = degré moyen, m = nombre total de modules


DIFFÉRENCES AVEC AKC-PM (MonApplication)
------------------------------------------
  AKC-PM (MonApplication) :
    - Pas de clustering, structure plate
    - Un seul iM, tous les modules s'authentifient vers lui
    - Suivi d'une reconfiguration physique

  S-CUP (scupCatoms3D) :
    - Organisation en clusters avec CHs et CMs
    - 3 phases distinctes (squelette → clusters → inter-cluster)
    - Deux modèles de clés (SERIAL / PARALLEL)
    - Pas de reconfiguration physique, focus sur la sécurité


COMPILATION ET EXÉCUTION
--------------------------
  make scupCatoms3D
  ./applicationsBin/scupCatoms3D/scupCatoms3D \
      applicationsBin/scupCatoms3D/config.xml
============================================================
