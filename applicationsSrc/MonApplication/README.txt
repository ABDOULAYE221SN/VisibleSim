============================================================
  MonApplication — Résumé de l'implémentation
  Protocole AKC-PM sur robots modulaires Catoms3D (VisibleSim)
  Faye et al. — INISTA 2025
============================================================

OBJECTIF
--------
Implémenter le protocole AKC-PM (Authentication, Key Management and
Confidentiality for Programmable Matter) dans le simulateur VisibleSim,
sur une structure de 5 modules Catoms3D, puis déclencher une
reconfiguration physique une fois l'authentification terminée.


STRUCTURE DU CODE
-----------------
  MonApplication.cpp       — point d'entrée, lance le simulateur
  MonApplicationCode.hpp   — déclaration de la classe, messages, états
  MonApplicationCode.cpp   — implémentation complète (~924 lignes)
  ../../simulatorCore/src/spongent160.{h,cpp}  — SPONGENT-160 réel


PRIMITIVES CRYPTOGRAPHIQUES
----------------------------
Toutes les opérations de hachage utilisent SPONGENT-160 (160 bits = 20
octets), fonction légère conforme à l'article CHES 2011.

  H(x)     = Spongent::Spongent160::hash(x)   — hachage principal
  L(i)     = simulation déterministe de la ligne i du code source (256 bits)
  HL(n)    = H(L(n mod Nb))                   — preuve d'exécution du même code
  Ts       = round(ts / 10)                   — horodatage synchronisé
  XOR      = ou-exclusif 160 bits

Note : L(i) utilise un LCG déterministe pour simuler le code source.
En production, ce serait une lecture réelle du binaire en mémoire.


DÉROULEMENT DE LA SIMULATION
------------------------------

1. DÉMARRAGE (startup)
   - Le module #1 calcule les assignations de déplacement (qui va où).
   - Le module en position (2,2,2) se désigne MODULE INITIAL (iM), coloré en bleu.
   - Tous les autres modules attendent, colorés en gris.

2. PHASE D'AUTHENTIFICATION

   Algorithme 1 — un nouveau module nM rejoint la structure :

     nM génère un nonce secret n0 (160 bits), puis calcule :
       n1 = H(n0)          → empreinte, prouve la connaissance de n0
       K0 = HL(n0 mod Nb)  → prouve l'exécution du même code source
       x  = Ts XOR n0      → masque n0 avec l'horloge (anti-rejeu)
     nM envoie (liens, n1, x, K0) à iM.

     iM reçoit, reconstruit n0 via Ts' ± tolérance (±5 unités = ±50 µs),
     vérifie H(n0') == n1 (sync) et HL(n0') == K0 (même code).
     Si OK, iM génère n2 (une seule fois, modèle SINGLE_KEY),
     calcule x1 = n2 XOR n0', K1 = HL(n2), et envoie x1 à nM.

     nM dérive n2 = x1 XOR n0, K1 = HL(n2).
     → nM et iM partagent la clé K1. Module coloré en vert.

   Algorithme 2 — deux modules déjà dans la structure se connectent :

     1M envoie (n1, x=K1 XOR n0) à iM (relai) et (n1) directement à 2M.
     iM propage n0 chiffré avec la clé de 2M.
     2M vérifie H(n0) == n1, génère K2, calcule K3 = HL(K2), envoie x1.
     1M dérive K3.
     → 1M et 2M partagent K3 sans repasser par l'Algo1.

   Modèle SINGLE_KEY : iM génère n2 une seule fois.
   Tous les modules authentifiés partagent la même clé K1 = HL(n2).

3. STATISTIQUES (affichées automatiquement en fin d'authentification)
   - Nombre de modules authentifiés
   - Nombre total de messages échangés
   - Durée totale de la phase d'authentification (µs)
   - Moyenne messages/module et temps/module
   - Complexité : O(m) pour Algo1, overhead 6H + 2Tx + 4XOR par lien

4. RECONFIGURATION (lancée après authentification complète)

   Forme initiale → Forme cible :
     M1 (1,3,2)  →  (3,3,2)
     M2 (2,3,2)  →  statique
     M3 (1,2,2)  →  (2,2,3)
     M4 (2,2,2)  →  statique (iM)
     M5 (3,2,2)  →  statique

   Algorithme glouton séquentiel :
     - Calcul de toutes les rotations possibles (Catoms3DMotionEngine)
     - Filtre : position libre + au moins 1 voisin (connectivité maintenue)
     - Priorité aux positions non visitées, puis la plus proche de la cible
     - Après chaque arrivée, re-authentification des nouveaux voisins (Algo2)


MESSAGES DÉFINIS
-----------------
  MSG_AUTH_REQUEST     (2001) — nM → iM : (liens, n1, x, K0)
  MSG_KEY_CHALLENGE    (2002) — iM → nM : (x1)
  MSG_STRUCTURE_READY  (2003) — diffusion : un module notifie ses voisins
  MSG_PROOF_MEMBERSHIP (2004) — 1M → 2M : (liens, n1)   [Algo2]
  MSG_PROOF_RELAY      (2005) — iM → 2M : (liens, x)    [Algo2 relai]


CODES COULEUR VISUELS
----------------------
  Bleu   — module initial iM
  Gris   — en attente d'authentification
  Orange — authentification en cours (Algo1)
  Vert   — authentifié, clé K1 établie


CONFORMITÉ À L'ARTICLE
------------------------
  ✓ SPONGENT-160 réel (pas simulé)
  ✓ Tolérance de synchronisation ±5 avec boucle d'offset
  ✓ Modèle SINGLE_KEY : n2 constant chez iM, K1 identique pour tous
  ✓ Relai Algo2 : recherche du voisin commun entre 1M et 2M
  ✓ Compteur de sauts = liens de 2M (transmis via STRUCTURE_READY)
  ✓ Purge des interfaces déconnectées après chaque mouvement
  ✓ Protection anti-doublon (authentificationsEnCours)

  Point de divergence : ~~L(i) utilise un LCG au lieu du vrai code binaire.~~
  L(i) lit maintenant les vrais octets du binaire en cours d'exécution via
  /proc/self/exe (Linux) ou _NSGetExecutablePath (macOS). Le fichier est
  découpé en NB_LIGNES_CODE blocs de BYTES_PER_LINE = 32 octets. La
  conformité avec l'article est désormais totale.


COMPILATION ET EXÉCUTION
--------------------------
  make MonApplication
  ./applicationsBin/MonApplication/MonApplication applicationsBin/MonApplication/config.xml
