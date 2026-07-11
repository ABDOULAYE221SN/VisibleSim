============================================================
  Résumé — Modules Catoms3D dans VisibleSim
============================================================


1. QU'EST-CE QU'UN CATOM3D ?
------------------------------
Un Catom3D est un robot modulaire quasi-sphérique simulé dans VisibleSim.
Il est placé sur une grille FCC (Face-Centered Cubic), ce qui signifie que
chaque cellule de la grille peut avoir jusqu'à 12 voisins directs.

Chaque module possède :
  - 12 connecteurs physiques (numérotés 0 à 11)
  - Une position dans la grille (Cell3DPosition x,y,z)
  - Un code d'orientation (0 à 11, indique quel connecteur est aligné avec l'axe X)
  - Un identifiant unique (blockId)
  - Un code applicatif (BlockCode) qui définit son comportement


2. CONNECTEURS ET GÉOMÉTRIE
-----------------------------
Les 12 connecteurs sont répartis selon deux types de faces :

  HexaFace  — face hexagonale, rotation de 60°
  OctaFace  — face octagonale, rotation de 90°

Les positions des connecteurs en coordonnées locales sont fixes
(tabConnectorPositions[12][3]) et les orientations possibles du module
sont définies par tabOrientationAngles[12][3].

Un module connaît ses voisins via ses connecteurs :
  getInterface(int id)          → connecteur par numéro
  getInterface(Cell3DPosition)  → connecteur adjacent à une position
  getNeighborBlock(conId)       → pointeur vers le voisin sur ce connecteur


3. MOUVEMENT
-------------
Un Catom3D se déplace en ROULANT sur la surface d'un voisin (le pivot).
Il ne peut donc PAS bouger s'il est isolé — un pivot est obligatoire.

Règle fondamentale : pour aller de la position A vers la position B,
il doit exister un module voisin commun entre A et B qui sert de pivot.

Le moteur de mouvement (Catoms3DMotionEngine) expose :

  getAllRotationsForModule(m)
    → liste toutes les rotations possibles depuis la position actuelle
    → retourne des paires (MotionRulesLink, Catoms3DRotation)

  canMoveTo(m, tPos)
    → vérifie si un pivot existe pour aller vers tPos

  findMotionPivot(m, tPos)
    → retourne le pivot à utiliser pour aller vers tPos

  getAllReachablePositions(m)
    → liste toutes les positions atteignables en une rotation

Pour déclencher un mouvement depuis le code applicatif :
  module->moveTo(pos)   → planifie et exécute la rotation
  module->canMoveTo(pos) → vérifie sans bouger

Un mouvement est décomposé en événements dans le scheduler :
  RotationStartEvent → N × RotationStepEvent → RotationStopEvent
                     → RotationEndEvent → onMotionEnd() appelé

onMotionEnd() est la méthode à surcharger dans son BlockCode pour
réagir à la fin d'un mouvement.

Contrainte de connectivité : après un mouvement, le module doit rester
connecté à la structure (au moins 1 voisin). Le moteur ne vérifie pas
cela automatiquement — c'est à l'application de le vérifier.


4. COMMUNICATION
-----------------
La communication est STRICTEMENT locale : un module ne peut envoyer
un message qu'à un voisin DIRECT (connecteur physiquement branché).

  sendMessage(msg, interface, t0, dt)
    → envoie un message à travers un connecteur connecté
    → lève InterfaceNotConnectedException si l'interface n'est pas branchée

  sendMessageToAllNeighbors(msg, t0, dt, nexcept, ...)
    → diffuse à tous les voisins directs (avec liste d'exceptions possible)

Il n'existe PAS de communication à distance, pas d'adressage global,
pas de réseau sans fil. Deux modules distants ne peuvent pas se parler
directement.

Pour communiquer à distance, il faut implémenter un protocole de
FLOODING (relais de proche en proche) : chaque module reçoit le message
et le retransmet à ses voisins, jusqu'à atteindre la destination.
C'est le principe de l'application e1_flooding.

Les messages reçus déclenchent EVENT_NI_RECEIVE dans processLocalEvent(),
qui appelle le gestionnaire enregistré via addMessageEventFunc2().


5. CYCLE DE VIE D'UN MODULE
-----------------------------
  createSimulator()         → charge config.xml, instancie les blocs
  startup()                 → appelé au démarrage de chaque module
  processLocalEvent(pev)    → boucle principale de traitement des événements
  onMotionEnd()             → fin d'un mouvement
  onNeighborChanged(face)   → un voisin est apparu ou disparu (EVENT_ADD/REMOVE_NEIGHBOR)
  onTap(face)               → clic utilisateur sur une face (interface graphique)


6. ÉVÉNEMENTS IMPORTANTS
--------------------------
  EVENT_NI_RECEIVE          → réception d'un message
  EVENT_ADD_NEIGHBOR        → un nouveau voisin s'est connecté
  EVENT_REMOVE_NEIGHBOR     → un voisin s'est déconnecté
  EVENT_ROTATION3D_END      → fin d'une rotation (→ onMotionEnd)
  EVENT_INTERRUPTION        → interruption programmée (timer interne)
  EVENT_TAP                 → clic sur une face dans l'interface graphique


7. STRUCTURE DU CODE (fichiers)
---------------------------------
  catoms3DBlock.h/cpp         → classe Catoms3DBlock, connecteurs, position, orientation
  catoms3DBlockCode.h/cpp     → classe de base pour le code applicatif
  catoms3DMotionEngine.h/cpp  → planification des mouvements (haut niveau)
  catoms3DMotionRules.h/cpp   → graphe des rotations autorisées (bas niveau)
  catoms3DRotationEvents.h/cpp→ événements d'animation de rotation
  catoms3DWorld.h/cpp         → monde simulé, grille FCC, map des blocs
  catoms3DSimulator.h/cpp     → point d'entrée, chargement config.xml


8. POINTS CLÉS À RETENIR
--------------------------
  - Un module isolé NE PEUT PAS bouger (pas de pivot = pas de rotation)
  - La communication est UNIQUEMENT entre voisins directs (connecteurs branchés)
  - La communication à distance nécessite un protocole de relais (flooding)
  - Chaque module a 12 connecteurs, donc jusqu'à 12 voisins simultanés
  - L'orientation du module change après chaque rotation
  - onMotionEnd() n'est appelé que si Catoms3DBlockCode::processLocalEvent()
    est appelé en premier dans la surcharge de processLocalEvent()
  - Les interfaces déconnectées après un mouvement doivent être purgées
    manuellement dans le code applicatif
============================================================
