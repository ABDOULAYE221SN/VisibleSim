============================================================
  Résumé — Gestion des messages dans VisibleSim
============================================================


1. PRINCIPE GÉNÉRAL
--------------------
VisibleSim est un moteur d'événements discrets. Le temps est simulé
en microsecondes. Tout — messages, mouvements, démarrages — est un
événement inséré dans une file triée par date (multimap<Time, Event>).
Le scheduler dépile et exécute les événements dans l'ordre chronologique,
sur un thread dédié.

Il n'y a pas de communication réseau réelle. Tout est simulé via
des événements schedulés à des dates précises.


2. PIPELINE COMPLET D'UN MESSAGE
----------------------------------
De l'appel sendMessage() jusqu'à la réception dans le BlockCode :

  ÉTAPE 1 — sendMessage(msg, iface, t0, dt)
    Vérifie que iface->connectedInterface != nullptr
    (sinon : InterfaceNotConnectedException)
    Schedule un NetworkInterfaceEnqueueOutgoingEvent à now() + t0

  ÉTAPE 2 — NetworkInterfaceEnqueueOutgoingEvent::consume()
    Appelle iface->addToOutgoingBuffer(msg)
    Le message est mis dans outgoingQueue de l'interface source
    Si la file était vide et l'interface libre :
      → schedule NetworkInterfaceStartTransmittingEvent à availabilityDate

  ÉTAPE 3 — NetworkInterfaceStartTransmittingEvent::consume()
    Appelle iface->send()
    Dépile le premier message de outgoingQueue
    Calcule la durée de transmission :
      transmissionDuration = (msg->size() * 8_000_000) / dataRate  [µs]
      dataRate par défaut = 1 Mbit/s
      ex : message de 4 octets → 32 µs de transmission
    Schedule NetworkInterfaceStopTransmittingEvent à now() + durée

  ÉTAPE 4 — NetworkInterfaceStopTransmittingEvent::consume()
    Le message arrive chez le destinataire :
      receivingBlock->scheduleLocalEvent(NetworkInterfaceReceiveEvent)
    Si d'autres messages attendent dans outgoingQueue :
      → démarre immédiatement la transmission suivante

  ÉTAPE 5 — ProcessLocalEvent → processLocalEvent(EVENT_NI_RECEIVE)
    Le bloc destinataire traite l'événement dans son BlockCode
    Dispatch selon msg->type vers le bon gestionnaire :
      case MSG_AUTH_REQUEST  → surReceptionDemandeAuth(...)
      case MSG_KEY_CHALLENGE → surReceptionDefiCle(...)
      etc.

Schéma :
  sendMessage()
    → EnqueueOutgoingEvent
      → addToOutgoingBuffer()
        → StartTransmittingEvent
          → calcul durée
            → StopTransmittingEvent
              → scheduleLocalEvent(ReceiveEvent) chez destinataire
                → processLocalEvent(EVENT_NI_RECEIVE)
                  → gestionnaire applicatif


3. SÉRIALISATION ET PARALLÉLISME
----------------------------------
  - Les messages sont SÉRIALISÉS par interface : si deux messages sont
    envoyés sur la même interface, le second attend la fin du premier
    (géré via availabilityDate).

  - Deux interfaces DIFFÉRENTES du même module transmettent en PARALLÈLE :
    chaque interface a sa propre outgoingQueue indépendante.

  - Un module peut donc envoyer simultanément à plusieurs voisins,
    mais pas deux messages au même voisin en même temps.


4. DURÉE DE TRANSMISSION
--------------------------
  transmissionDuration = (taille_en_octets × 8_000_000) / dataRate

  Avec dataRate = 1 Mbit/s (par défaut) :
    4 octets  →   32 µs
    20 octets →  160 µs   (nonce SPONGENT-160)
    60 octets →  480 µs   (message AUTH_REQUEST : 3 × 20 octets)

  ATTENTION : dans MonApplication, la méthode size() n'est pas
  surchargée dans les sous-classes de Message. Le simulateur utilise
  donc la valeur par défaut (4 octets) pour tous les messages,
  sous-estimant le temps de transmission réel des messages crypto.


5. ENREGISTREMENT DES GESTIONNAIRES DE MESSAGES
-------------------------------------------------
Deux mécanismes coexistent dans VisibleSim :

  Mécanisme 1 — addMessageEventFunc2() (recommandé)
    Enregistre une fonction de callback pour un type de message :
      addMessageEventFunc2(MSG_AUTH_REQUEST,
          bind(&MonApplicationCode::surReceptionDemandeAuth, this, _1, _2));
    Appelé automatiquement par processLocalEvent() de BlockCode.

  Mécanisme 2 — surcharge de processLocalEvent() (utilisé dans MonApplication)
    On surcharge processLocalEvent(), on appelle d'abord la classe parente,
    puis on gère EVENT_NI_RECEIVE manuellement avec un switch sur msg->type.
    Nécessaire quand on veut aussi gérer EVENT_INTERRUPTION, EVENT_ADD_NEIGHBOR, etc.


6. ÉVÉNEMENTS RÉSEAU (types)
------------------------------
  EVENT_NI_ENQUEUE_OUTGOING_MESSAGE  → mise en file sortante
  EVENT_NI_START_TRANSMITTING        → début de transmission
  EVENT_NI_STOP_TRANSMITTING         → fin de transmission, livraison
  EVENT_NI_RECEIVE                   → réception dans le BlockCode


7. LE SCHEDULER
----------------
  - File d'événements : multimap<Time, EventPtr> triée par date
  - Thread dédié avec mutex pour la thread-safety
  - Trois modes d'exécution :
      FASTEST   → aussi vite que possible (mode terminal)
      REALTIME  → pause entre événements pour visualisation
      DEBUG     → pas à pas (non complètement implémenté)
  - Trois modes de terminaison :
      DEFAULT   → s'arrête quand la file est vide
      BOUNDED   → s'arrête à une date maximale fixée
      INFINITE  → ne s'arrête pas automatiquement
  - scheduler->now() retourne la date courante en µs
  - scheduler->schedule(event) insère un événement dans la file


8. POINTS CLÉS À RETENIR
--------------------------
  - Pas de communication sans connexion physique (connecteur branché)
  - La transmission prend du temps simulé (dépend de msg->size())
  - Les messages sont sérialisés par interface, parallèles entre interfaces
  - Pour recevoir des messages, il faut gérer EVENT_NI_RECEIVE dans
    processLocalEvent() ou enregistrer via addMessageEventFunc2()
  - Surcharger size() dans ses classes Message est important pour
    avoir des délais de transmission réalistes
  - Le scheduler tourne sur un thread séparé — ne pas accéder à ses
    structures sans passer par schedule()
============================================================
