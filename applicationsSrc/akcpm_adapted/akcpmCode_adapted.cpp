/**
 * @file akcpmCode_adapted.cpp
 * @brief Implémentation adaptée du protocole AKC-PM selon Faye et al. (2024)
 * @author Abdullah Ndiaye
 * @date 2024-12-30
 * 
 * VERSION CORRIGÉE - Envoi automatique de SF-2 au module initial
 */

#include <iostream>
#include <sstream>
#include <cstdlib>
#include <ctime>
#include <cmath>
#include "akcpmCode_adapted.hpp"

using namespace std;

// ============================================================================
// CONSTRUCTEUR
// ============================================================================

AKCPMCodeAdapted::AKCPMCodeAdapted(BlinkyBlocksBlock *host) 
    : BlinkyBlocksBlockCode(host), module(host) {
    
    currentState = UNINITIALIZED;
    isInitialModule = false;
    keyModel = SINGLE_KEY;  // Par défaut: modèle à clé unique
    neighborDiscovered = 0;
    totalNeighbors = 0;
    linksInStructure = 0;
    startTime = 0;
    
    // ✨ ENREGISTRER LES HANDLERS DE MESSAGES
    addMessageEventFunc2(MSG_HELLO_ID,
                         std::bind(&AKCPMCodeAdapted::onHelloMessage, this,
                         std::placeholders::_1, std::placeholders::_2));
    
    addMessageEventFunc2(MSG_HELLO_REPLY_ID,
                         std::bind(&AKCPMCodeAdapted::onHelloReplyMessage, this,
                         std::placeholders::_1, std::placeholders::_2));
    
    addMessageEventFunc2(MSG_SF2_ID,
                         std::bind(&AKCPMCodeAdapted::onSF2Message, this,
                         std::placeholders::_1, std::placeholders::_2));
    
    addMessageEventFunc2(MSG_SF4_ID,
                         std::bind(&AKCPMCodeAdapted::onSF4Message, this,
                         std::placeholders::_1, std::placeholders::_2));
    
    addMessageEventFunc2(MSG_AUTH_PROOF_ID,
                         std::bind(&AKCPMCodeAdapted::onAuthProofMessage, this,
                         std::placeholders::_1, std::placeholders::_2));
    
    // Initialiser le code source
    initializeSourceCode();
    
    // Initialiser le générateur aléatoire
    srand(time(NULL) + module->blockId);
}

// ============================================================================
// INITIALISATION DU CODE SOURCE
// ============================================================================

void AKCPMCodeAdapted::initializeSourceCode() {
    sourceCode.clear();
    sourceCode.resize(NB_CODE_LINES);
    
    // Initialiser chaque ligne avec un pattern déterministe
    // mais différent pour chaque ligne
    for (int i = 0; i < NB_CODE_LINES; i++) {
        for (int j = 0; j < LINE_SIZE / 8; j++) {
            sourceCode[i].data[j] = (i * 7 + j * 13) % 256;
        }
    }
    
    cout << "[Module " << module->blockId << "] Code source initialisé ("
         << NB_CODE_LINES << " lignes)" << endl;
}

// ============================================================================
// DÉMARRAGE DU MODULE
// ============================================================================

void AKCPMCodeAdapted::startup() {
    printState("DÉMARRAGE AKC-PM");
    
    startTime = scheduler->now();
    
    // Le module 1 est le module initial (iM)
    if (module->blockId == 1) {
        isInitialModule = true;
        currentState = CONFIGURED;
        linksInStructure = 0;
        
        cout << "[Module " << module->blockId 
             << "] Je suis le MODULE INITIAL (iM)" << endl;
        
    } else {
        // Les autres modules démarrent non initialisés
        currentState = UNINITIALIZED;
        
        // Préparer SF-1
        structureFormation_SF1();
        
        // Passer en état de découverte
        currentState = NEIGHBOR_DISCOVERY;
        
        // ✨ CORRECTION : Initier SF-2 vers le module initial
        // Appeler après un court délai pour laisser le système s'initialiser
        initiateSF2ToInitialModule();
    }
    
    updateModuleColor();
    
    cout << "[Module " << module->blockId 
         << "] Démarrage terminé - État: " << currentState << endl;
}

// ============================================================================
// ✨ NOUVELLE FONCTION : INITIER SF-2 VERS LE MODULE INITIAL
// ============================================================================

void AKCPMCodeAdapted::initiateSF2ToInitialModule() {
    printState("RECHERCHE MODULE INITIAL POUR SF-2");
    
    // Trouver l'interface vers le module 1 (iM)
    P2PNetworkInterface* iM_interface = nullptr;
    int iM_id = -1;
    
    // Parcourir toutes les interfaces pour trouver celle connectée au module 1
    for (int i = 0; i < 6; i++) {  // BlinkyBlocks ont 6 interfaces
        P2PNetworkInterface* interface = module->getInterface(i);
        if (interface && interface->connectedInterface) {
            int neighborId = interface->connectedInterface->hostBlock->blockId;
            
            // Si c'est le module 1, c'est notre module initial
            if (neighborId == 1) {
                iM_interface = interface;
                iM_id = neighborId;
                break;
            }
        }
    }
    
    if (iM_interface) {
        cout << "[Module " << module->blockId 
             << "] ✓ Module initial (ID=" << iM_id << ") trouvé directement" << endl;
        
        // Enregistrer le voisin si pas déjà fait
        registerNeighbor(iM_id, iM_interface);
        
        // Envoyer SF-2
        structureFormation_SF2(iM_interface);
        
    } else {
        cout << "[Module " << module->blockId 
             << "] ⚠ Module initial non adjacent - recherche via voisins" << endl;
        
        // Dans un réseau plus grand, il faudrait :
        // 1. Envoyer HELLO à tous les voisins
        // 2. Propager la recherche du module initial
        // 3. Utiliser un routage pour atteindre le module 1
        
        // Pour l'instant, envoyer HELLO aux voisins pour découverte
        sendHelloToAllNeighbors();
    }
}

// ============================================================================
// ✨ NOUVELLE FONCTION : ENVOYER HELLO À TOUS LES VOISINS
// ============================================================================

void AKCPMCodeAdapted::sendHelloToAllNeighbors() {
    printState("ENVOI HELLO À TOUS LES VOISINS");
    
    int neighborsFound = 0;
    
    for (int i = 0; i < 6; i++) {
        P2PNetworkInterface* interface = module->getInterface(i);
        if (interface && interface->connectedInterface) {
            int neighborId = interface->connectedInterface->hostBlock->blockId;
            
            // Enregistrer le voisin
            registerNeighbor(neighborId, interface);
            
            // Créer et envoyer HELLO avec données
            HelloData helloData(module->blockId, currentState);
            sendHelloMessage(helloData, interface);
            
            neighborsFound++;
            
            cout << "[Module " << module->blockId 
                 << "] HELLO envoyé à voisin " << neighborId << endl;
        }
    }
    
    totalNeighbors = neighborsFound;
    
    cout << "[Module " << module->blockId 
         << "] Total voisins: " << totalNeighbors << endl;
}

// ============================================================================
// FONCTIONS CRYPTOGRAPHIQUES (selon l'article)
// ============================================================================

uint64_t AKCPMCodeAdapted::hashCodeLine(int lineNumber) {
    // H(L(lineNumber)) - Hash d'une ligne de code
    // Simuler SPONGENT-160 avec un hash simple mais déterministe
    
    int lineIndex = lineNumber % NB_CODE_LINES;
    uint64_t hash = 0;
    
    // Hash simple sur les données de la ligne
    for (int i = 0; i < LINE_SIZE / 8; i++) {
        hash = ((hash << 5) + hash) + sourceCode[lineIndex].data[i];
    }
    
    // Ajouter le numéro de ligne pour plus d'unicité
    hash ^= (lineNumber * 0x9e3779b97f4a7c15ULL);
    
    metrics.hashOperations++;
    return hash;
}

uint64_t AKCPMCodeAdapted::hashFunction(uint64_t data) {
    // Fonction de hash H() - Simuler SPONGENT-160
    // Hash simple mais déterministe
    
    uint64_t hash = data;
    hash ^= (hash >> 33);
    hash *= 0xff51afd7ed558ccdULL;
    hash ^= (hash >> 33);
    hash *= 0xc4ceb9fe1a85ec53ULL;
    hash ^= (hash >> 33);
    
    metrics.hashOperations++;
    return hash;
}

uint64_t AKCPMCodeAdapted::generateNonce() {
    // Générer un nonce aléatoire de 64 bits
    return ((uint64_t)rand() << 32) | rand();
}

uint64_t AKCPMCodeAdapted::computeTs(uint64_t currentTime) {
    // Ts ≈ (ts/10) - Arrondi à l'entier le plus proche
    // Cela permet de corriger les décalages de synchronisation de ±5
    
    uint64_t ts_divided = currentTime / TIME_QUANTUM;
    double remainder = (double)(currentTime % TIME_QUANTUM) / TIME_QUANTUM;
    
    // Arrondi à l'entier le plus proche
    if (remainder >= 0.5) {
        ts_divided++;
    }
    
    return ts_divided;
}

uint64_t AKCPMCodeAdapted::xorOperation(uint64_t a, uint64_t b) {
    // Opération XOR simple
    metrics.xorOperations++;
    return a ^ b;
}

bool AKCPMCodeAdapted::verifySynchronization(uint64_t receivedN1, 
                                             uint64_t receivedX,
                                             uint64_t receptionTime, 
                                             uint64_t& n0_derived) {
    // SF-3: Vérifier la synchronisation selon l'article page 5
    // Calculer Ts ≈ ((Tr - Δt)/10)
    
    uint64_t adjustedTime = (receptionTime > TRANSMISSION_DELAY) ? 
                            (receptionTime - TRANSMISSION_DELAY) : 0;
    uint64_t Ts = computeTs(adjustedTime);
    
    cout << "[Module " << module->blockId << "] SF-3: Vérification synchronisation:" << endl;
    cout << "  Temps réception: " << receptionTime << " µs" << endl;
    cout << "  Temps ajusté: " << adjustedTime << " µs (Tr - Δt)" << endl;
    cout << "  Ts calculé: " << adjustedTime << " / " << TIME_QUANTUM << " ≈ " << Ts << endl;
    cout << "  x reçu: " << receivedX << endl;
    
    // Dériver n0: n0' = x ⊕ Ts
    n0_derived = xorOperation(receivedX, Ts);
    
    cout << "  n0' dérivé: x ⊕ Ts = " << receivedX << " ⊕ " << Ts << " = " << n0_derived << endl;
    
    // Vérifier si n1 = H(n0')
    uint64_t computed_n1 = hashFunction(n0_derived);
    
    cout << "  n1 calculé: H(n0') = " << computed_n1 << endl;
    cout << "  n1 reçu: " << receivedN1 << endl;
    
    if (computed_n1 == receivedN1) {
        cout << "[Module " << module->blockId 
             << "] ✓ Synchronisation vérifiée : n1 = H(n0')" << endl;
        return true;
    } else {
        cout << "[Module " << module->blockId 
             << "] ✗ Échec synchronisation : n1 ≠ H(n0')" << endl;
        return false;
    }
}

// ============================================================================
// ALGORITHM 1: STRUCTURE FORMATION
// ============================================================================

// ---------------------------------------------------------------------------
// SF-1: Préparation par le nouveau module
// ---------------------------------------------------------------------------

void AKCPMCodeAdapted::structureFormation_SF1() {
    printState("SF-1: PRÉPARATION AUTHENTIFICATION");
    
    // Choisir n0
    myCrypto.n0 = generateNonce();
    
    // Calculer n1 = H(n0)
    myCrypto.n1 = hashFunction(myCrypto.n0);
    
    // Calculer K0 = H(L(n0 mod Nb))
    int lineNumber = myCrypto.n0 % NB_CODE_LINES;
    myCrypto.K0 = hashCodeLine(lineNumber);
    
    // ✨ NE PAS calculer Ts et x ici !
    // Selon l'article : "Ts for sending the request"
    // Ts et x seront calculés dans SF-2 au moment de l'envoi
    
    cout << "[Module " << module->blockId << "] SF-1 complété:" << endl;
    cout << "  n0: " << myCrypto.n0 << endl;
    cout << "  n1: " << myCrypto.n1 << endl;
    cout << "  K0: " << myCrypto.K0 << endl;
    cout << "  Ligne hashée: " << lineNumber << endl;
}

// ---------------------------------------------------------------------------
// SF-2: Envoi de la requête au module initial
// ---------------------------------------------------------------------------

void AKCPMCodeAdapted::structureFormation_SF2(
    P2PNetworkInterface *iM_interface) {
    
    printState("SF-2: ENVOI REQUÊTE À iM");
    
    // Préparer SF-1 si pas déjà fait
    if (myCrypto.n0 == 0) {
        structureFormation_SF1();
    }
    
    // ✨ CALCULER Ts AU MOMENT DE L'ENVOI PRÉVU
    // Dans VisibleSim, tous les startups s'exécutent à t=0
    // Les messages sont ensuite livrés selon leur délai planifié
    // On utilise un délai de 1500 µs pour laisser le temps au scheduler de démarrer
    const uint64_t SEND_DELAY = 1500;  // Délai pour que le scheduler soit actif
    uint64_t currentTime = scheduler->now();
    uint64_t actualSendingTime = currentTime + SEND_DELAY;
    
    myCrypto.Ts = computeTs(actualSendingTime);
    
    // Calculer x = Ts ⊕ n0 pour protéger le nonce
    myCrypto.x = xorOperation(myCrypto.Ts, myCrypto.n0);
    
    cout << "[Module " << module->blockId << "] SF-2: Calcul synchronisation:" << endl;
    cout << "  Temps actuel: " << currentTime << " µs" << endl;
    cout << "  Délai d'envoi prévu: +" << SEND_DELAY << " µs" << endl;
    cout << "  Temps d'envoi effectif: " << actualSendingTime << " µs" << endl;
    cout << "  Ts = " << actualSendingTime << " / " << TIME_QUANTUM << " ≈ " << myCrypto.Ts << endl;
    cout << "  x = Ts ⊕ n0 = " << myCrypto.Ts << " ⊕ " << myCrypto.n0 << " = " << myCrypto.x << endl;
    
    // Créer les données SF-2 : (linksInStructure, n1, x, K0)
    SF2Data sf2data(linksInStructure, myCrypto.n1, myCrypto.x, myCrypto.K0);
    
    // Envoyer via la nouvelle fonction
    sendSF2Message(sf2data, iM_interface);
    
    currentState = KEY_EXCHANGE_INIT;
    updateModuleColor();
    
    cout << "[Module " << module->blockId 
         << "] ✓ SF-2: Requête envoyée à iM" << endl;
    cout << "  Liens dans structure: " << linksInStructure << endl;
    cout << "  n1: " << myCrypto.n1 << endl;
    cout << "  x: " << myCrypto.x << endl;
    cout << "  K0: " << myCrypto.K0 << endl;
}

// ---------------------------------------------------------------------------
// SF-3: Vérification par le module initial
// ---------------------------------------------------------------------------

void AKCPMCodeAdapted::structureFormation_SF3(uint64_t receivedN1, 
                                              uint64_t receivedX,
                                              uint64_t receivedK0, 
                                              uint64_t receptionTime,
                                              P2PNetworkInterface *sender) {
    
    printState("SF-3: VÉRIFICATION PAR iM");
    
    int senderId = sender->connectedInterface->hostBlock->blockId;
    
    // Calculer Ts ≈ ((Tr - Δt)/10)
    uint64_t n0_derived;
    if (!verifySynchronization(receivedN1, receivedX, receptionTime, 
                              n0_derived)) {
        cout << "[Module " << module->blockId 
             << "] ✗ SF-3: Échec vérification synchronisation" << endl;
        metrics.authFailures++;
        neighbors[senderId].failedAttempts++;
        return;
    }
    
    // Vérifier K0 = H(L(n0' mod Nb))
    int lineNumber = n0_derived % NB_CODE_LINES;
    uint64_t computed_K0 = hashCodeLine(lineNumber);
    
    if (computed_K0 != receivedK0) {
        cout << "[Module " << module->blockId 
             << "] ✗ SF-3: K0 ne correspond pas!" << endl;
        cout << "  Attendu: " << computed_K0 << endl;
        cout << "  Reçu: " << receivedK0 << endl;
        metrics.authFailures++;
        neighbors[senderId].failedAttempts++;
        return;
    }
    
    cout << "[Module " << module->blockId 
         << "] ✓ SF-3: Module " << senderId << " authentifié!" << endl;
    cout << "  n0' dérivé: " << n0_derived << endl;
    cout << "  K0 vérifié: " << computed_K0 << endl;
    
    // Passer à SF-4: Génération de clé
    structureFormation_SF4(n0_derived, sender);
}

// ---------------------------------------------------------------------------
// SF-4: Génération de clé par iM
// ---------------------------------------------------------------------------

void AKCPMCodeAdapted::structureFormation_SF4(uint64_t n0_derived, 
                                              P2PNetworkInterface *sender) {
    
    printState("SF-4: GÉNÉRATION CLÉ PAR iM");
    
    int senderId = sender->connectedInterface->hostBlock->blockId;
    
    // Choisir n2
    myCrypto.n2 = generateNonce();
    
    // Calculer x1 = n2 ⊕ n0
    uint64_t x1 = xorOperation(myCrypto.n2, n0_derived);
    
    // Calculer K1 = H(L(n2 mod Nb))
    int lineNumber = myCrypto.n2 % NB_CODE_LINES;
    myCrypto.K1 = hashCodeLine(lineNumber);
    myCrypto.keyEstablished = true;
    
    // Enregistrer dans les données du voisin
    neighbors[senderId].cryptoData.K1 = myCrypto.K1;
    neighbors[senderId].cryptoData.n2 = myCrypto.n2;
    neighbors[senderId].cryptoData.keyEstablished = true;
    
    // Créer les données SF-4 et envoyer
    SF4Data sf4data(x1, myCrypto.n2);
    sendSF4Message(sf4data, sender);
    
    metrics.keyExchanges++;
    
    // Marquer comme authentifié
    neighbors[senderId].authenticated = true;
    authenticatedNeighbors.insert(senderId);
    linksInStructure++;
    
    cout << "[Module " << module->blockId << "] ✓ SF-4: Clé générée" << endl;
    cout << "  n2: " << myCrypto.n2 << endl;
    cout << "  x1 envoyé: " << x1 << endl;
    cout << "  K1: " << myCrypto.K1 << endl;
    cout << "  Ligne hashée: " << lineNumber << endl;
    cout << "  Liens dans structure: " << linksInStructure << endl;
    
    metrics.authSuccesses++;
}

// ---------------------------------------------------------------------------
// SF-5: Génération de clé par le nouveau module
// ---------------------------------------------------------------------------

void AKCPMCodeAdapted::structureFormation_SF5(uint64_t receivedX1) {
    printState("SF-5: GÉNÉRATION CLÉ PAR NOUVEAU MODULE");
    
    // Calculer n2 = x1 ⊕ n0
    myCrypto.n2 = xorOperation(receivedX1, myCrypto.n0);
    
    // Calculer K1 = H(L(n2 mod Nb))
    int lineNumber = myCrypto.n2 % NB_CODE_LINES;
    myCrypto.K1 = hashCodeLine(lineNumber);
    myCrypto.keyEstablished = true;
    
    currentState = AUTHENTICATED;
    updateModuleColor();
    linksInStructure = 1;  // Premier lien établi
    
    cout << "[Module " << module->blockId 
         << "] ✓ SF-5: Clé partagée établie!" << endl;
    cout << "  n2: " << myCrypto.n2 << endl;
    cout << "  K1: " << myCrypto.K1 << endl;
    cout << "  Ligne hashée: " << lineNumber << endl;
}

// ============================================================================
// ALGORITHM 2: AUTHENTICATION BY PROOF OF MEMBERSHIP
// ============================================================================

void AKCPMCodeAdapted::authenticationByProof(int targetModuleId, 
                                             P2PNetworkInterface *targetInterface) {
    printState("ALG-2: PREUVE D'APPARTENANCE à " + to_string(targetModuleId));
    
    // Un module déjà dans la structure authentifie un autre module
    // en prouvant son appartenance via ses voisins
    
    // SF-1: Choisir n0
    uint64_t n0 = generateNonce();
    uint64_t n1 = hashFunction(n0);
    
    // Trouver le voisin par lequel on est connecté à la structure
    P2PNetworkInterface *structureLink = nullptr;
    uint64_t linkKey = 0;
    
    for (auto& pair : neighbors) {
        if (pair.second.authenticated) {
            structureLink = pair.second.interface;
            linkKey = pair.second.cryptoData.K1;
            break;
        }
    }
    
    if (!structureLink) {
        cout << "[Module " << module->blockId 
             << "] ✗ Pas de lien vers la structure!" << endl;
        return;
    }
    
    // SF-2: Envoyer (linksInStructure, x) au voisin de structure
    // et (linksInStructure, n1) au module cible
    
    uint64_t x = xorOperation(linkKey, n0);
    
    MessagePtr msgToStructure = std::make_shared<Message>();
    MessagePtr msgToTarget = std::make_shared<Message>();
    
    sendSecureMessage("AUTH_PROOF_REQ", msgToStructure, structureLink);
    sendSecureMessage("AUTH_PROOF_CHALLENGE", msgToTarget, targetInterface);
    
    metrics.transmissions += 2;
    
    cout << "[Module " << module->blockId << "] ALG-2: Requête envoyée" << endl;
    cout << "  n0: " << n0 << endl;
    cout << "  n1: " << n1 << endl;
    cout << "  x = K ⊕ n0: " << x << endl;
}

// ============================================================================
// GESTION DES MODÈLES DE CLÉS (3 versions de l'article)
// ============================================================================

uint64_t AKCPMCodeAdapted::generateKey(uint64_t baseNonce, int linkNumber) {
    // Générer une clé selon le modèle choisi
    
    switch (keyModel) {
        case SINGLE_KEY:
            return generateSingleKey(baseNonce);
            
        case FOUR_KEYS:
            return generateFourKeys(baseNonce, linkNumber);
            
        case MULTI_KEYS:
            return generatePairwiseKey(baseNonce);
            
        default:
            return generateSingleKey(baseNonce);
    }
}

uint64_t AKCPMCodeAdapted::generateSingleKey(uint64_t n2) {
    int lineNumber = n2 % NB_CODE_LINES;
    uint64_t K1 = hashCodeLine(lineNumber);
    
    cout << "[Module " << module->blockId 
         << "] SINGLE_KEY: K1 = H(L(" << n2 << " mod " << NB_CODE_LINES 
         << "))" << endl;
    
    return K1;
}

uint64_t AKCPMCodeAdapted::generateFourKeys(uint64_t n2, int keyIndex) {
    int lineNumber = n2 % NB_CODE_LINES;
    uint64_t K1 = hashCodeLine(lineNumber);
    
    uint64_t Ki = K1;
    for (int i = 1; i < keyIndex && i <= 4; i++) {
        Ki = hashFunction(hashFunction(Ki));
    }
    
    cout << "[Module " << module->blockId 
         << "] FOUR_KEYS: K" << keyIndex << " généré" << endl;
    
    return Ki;
}

uint64_t AKCPMCodeAdapted::generatePairwiseKey(uint64_t prevKey) {
    int lineNumber = prevKey % NB_CODE_LINES;
    uint64_t K_new = hashCodeLine(lineNumber);
    
    cout << "[Module " << module->blockId 
         << "] MULTI_KEYS: Nouvelle clé pairwise générée" << endl;
    
    return K_new;
}

// ============================================================================
// GESTIONNAIRES DE MESSAGES
// ============================================================================

void AKCPMCodeAdapted::handleHelloMessage(MessagePtr msg, 
                                          P2PNetworkInterface *sender) {
    int senderId = sender->connectedInterface->hostBlock->blockId;
    
    printState("REÇU HELLO de " + to_string(senderId));
    
    // Enregistrer le voisin
    registerNeighbor(senderId, sender);
    
    // Si on est configuré ou le module initial, répondre
    if (currentState == CONFIGURED || isInitialModule) {
        MessagePtr reply = std::make_shared<Message>();
        sendSecureMessage("HELLO_REPLY", reply, sender);
    }
}

void AKCPMCodeAdapted::handleHelloReply(MessagePtr msg, 
                                        P2PNetworkInterface *sender) {
    int senderId = sender->connectedInterface->hostBlock->blockId;
    
    printState("REÇU HELLO_REPLY de " + to_string(senderId));
    
    if (neighbors.find(senderId) == neighbors.end()) {
        registerNeighbor(senderId, sender);
    }
    
    neighborDiscovered++;
}

void AKCPMCodeAdapted::handleSF2Message(MessagePtr msg, 
                                        P2PNetworkInterface *sender) {
    int senderId = sender->connectedInterface->hostBlock->blockId;
    printState("REÇU SF-2 de " + to_string(senderId));
    
    if (!isInitialModule && linksInStructure == 0) {
        cout << "[Module " << module->blockId 
             << "] ✗ Je ne suis pas configuré, rejeter SF-2" << endl;
        return;
    }
    
    // Récupérer les données du voisin
    if (neighbors.find(senderId) != neighbors.end()) {
        NeighborInfo& neighbor = neighbors[senderId];
        
        // Dans une vraie implémentation, extraire n1, x, K0 du message
        // Ici on utilise les données stockées lors de SF-1 du voisin
        uint64_t receivedN1 = neighbor.cryptoData.n1;
        uint64_t receivedX = neighbor.cryptoData.x;
        uint64_t receivedK0 = neighbor.cryptoData.K0;
        uint64_t receptionTime = scheduler->now();
        
        // Appeler SF-3 pour vérifier
        structureFormation_SF3(receivedN1, receivedX, receivedK0, 
                              receptionTime, sender);
    }
}

void AKCPMCodeAdapted::handleSF4Message(MessagePtr msg, 
                                        P2PNetworkInterface *sender) {
    int senderId = sender->connectedInterface->hostBlock->blockId;
    printState("REÇU SF-4 de " + to_string(senderId));
    
    // Récupérer x1 des données du voisin
    if (neighbors.find(senderId) != neighbors.end()) {
        uint64_t receivedX1 = neighbors[senderId].cryptoData.x;
        
        // Appeler SF-5 pour générer la clé
        structureFormation_SF5(receivedX1);
        
        // Marquer le voisin comme authentifié
        neighbors[senderId].authenticated = true;
        authenticatedNeighbors.insert(senderId);
        
        cout << "[Module " << module->blockId 
             << "] ✓ Authentification complétée avec " << senderId << endl;
    }
}

void AKCPMCodeAdapted::handleAuthProof(MessagePtr msg, 
                                       P2PNetworkInterface *sender) {
    int senderId = sender->connectedInterface->hostBlock->blockId;
    printState("REÇU AUTH_PROOF de " + to_string(senderId));
    
    cout << "[Module " << module->blockId 
         << "] Vérification preuve d'appartenance de " << senderId << endl;
}

// ============================================================================
// TRAITEMENT DES MESSAGES PRINCIPAL
// ============================================================================

// ============================================================================
// HANDLERS DE MESSAGES (appelés automatiquement par VisibleSim)
// ============================================================================

void AKCPMCodeAdapted::onHelloMessage(std::shared_ptr<Message> msg, 
                                       P2PNetworkInterface *sender) {
    // Extraire les données HELLO
    MessageOf<HelloData>* helloMsg = static_cast<MessageOf<HelloData>*>(msg.get());
    HelloData data = *helloMsg->getData();
    
    int senderId = data.moduleId;
    
    printState("REÇU HELLO de " + to_string(senderId));
    
    // Enregistrer le voisin
    registerNeighbor(senderId, sender);
    
    // Si on est configuré ou le module initial, répondre
    if (currentState == CONFIGURED || isInitialModule) {
        HelloData replyData(module->blockId, currentState);
        MessageOf<HelloData>* reply = new MessageOf<HelloData>(MSG_HELLO_REPLY_ID, replyData);
        sendMessage(reply, sender, 100, 1000);  // ✅ 4 arguments
        metrics.totalMessages++;
        
        cout << "[Module " << module->blockId 
             << "] ➤ HELLO_REPLY envoyé à " << senderId << endl;
    }
}

void AKCPMCodeAdapted::onHelloReplyMessage(std::shared_ptr<Message> msg,
                                            P2PNetworkInterface *sender) {
    // Extraire les données HELLO_REPLY
    MessageOf<HelloData>* helloMsg = static_cast<MessageOf<HelloData>*>(msg.get());
    HelloData data = *helloMsg->getData();
    
    int senderId = data.moduleId;
    
    printState("REÇU HELLO_REPLY de " + to_string(senderId));
    
    if (neighbors.find(senderId) == neighbors.end()) {
        registerNeighbor(senderId, sender);
    }
    
    neighborDiscovered++;
    
    cout << "[Module " << module->blockId 
         << "] État du voisin " << senderId << ": " << data.state << endl;
}

void AKCPMCodeAdapted::onSF2Message(std::shared_ptr<Message> msg,
                                     P2PNetworkInterface *sender) {
    int senderId = sender->connectedInterface->hostBlock->blockId;
    printState("REÇU SF-2 de " + to_string(senderId));
    
    if (!isInitialModule && linksInStructure == 0) {
        cout << "[Module " << module->blockId 
             << "] ✗ Je ne suis pas configuré, rejeter SF-2" << endl;
        return;
    }
    
    // ✨ EXTRAIRE LES VRAIES DONNÉES du message
    MessageOf<SF2Data>* sf2msg = static_cast<MessageOf<SF2Data>*>(msg.get());
    SF2Data data = *sf2msg->getData();
    
    uint64_t receivedN1 = data.n1;
    uint64_t receivedX = data.x;
    uint64_t receivedK0 = data.K0;
    uint64_t receptionTime = scheduler->now();
    
    cout << "[Module " << module->blockId << "] SF-2 données reçues:" << endl;
    cout << "  n1: " << receivedN1 << endl;
    cout << "  x: " << receivedX << endl;
    cout << "  K0: " << receivedK0 << endl;
    cout << "  Liens: " << data.linksInStructure << endl;
    
    // Appeler SF-3 pour vérifier
    structureFormation_SF3(receivedN1, receivedX, receivedK0, 
                          receptionTime, sender);
}

void AKCPMCodeAdapted::onSF4Message(std::shared_ptr<Message> msg,
                                     P2PNetworkInterface *sender) {
    int senderId = sender->connectedInterface->hostBlock->blockId;
    printState("REÇU SF-4 de " + to_string(senderId));
    
    // ✨ EXTRAIRE LES VRAIES DONNÉES du message
    MessageOf<SF4Data>* sf4msg = static_cast<MessageOf<SF4Data>*>(msg.get());
    SF4Data data = *sf4msg->getData();
    
    uint64_t receivedX1 = data.x1;
    
    cout << "[Module " << module->blockId << "] SF-4 données reçues:" << endl;
    cout << "  x1: " << receivedX1 << endl;
    cout << "  n2 (debug): " << data.n2 << endl;
    
    // Appeler SF-5 pour générer la clé
    structureFormation_SF5(receivedX1);
    
    // Marquer le voisin comme authentifié
    neighbors[senderId].authenticated = true;
    authenticatedNeighbors.insert(senderId);
    
    cout << "[Module " << module->blockId 
         << "] ✓ Authentification complétée avec " << senderId << endl;
}

void AKCPMCodeAdapted::onAuthProofMessage(std::shared_ptr<Message> msg,
                                           P2PNetworkInterface *sender) {
    int senderId = sender->connectedInterface->hostBlock->blockId;
    printState("REÇU AUTH_PROOF de " + to_string(senderId));
    
    cout << "[Module " << module->blockId 
         << "] Vérification preuve d'appartenance de " << senderId << endl;
}

// ============================================================================
// UTILITAIRES
// ============================================================================

void AKCPMCodeAdapted::updateModuleColor() {
    // Codes couleur selon l'état du protocole
    switch (currentState) {
        case UNINITIALIZED:
            module->setColor(Color(64, 64, 64));        // Gris foncé
            break;
        case INITIALIZED:
            module->setColor(Color(128, 128, 128));     // Gris
            break;
        case NEIGHBOR_DISCOVERY:
            module->setColor(Color(255, 255, 0));       // Jaune
            break;
        case KEY_EXCHANGE_INIT:
        case KEY_EXCHANGE_RESP:
            module->setColor(Color(255, 165, 0));       // Orange
            break;
        case KEY_CONFIRMATION:
            module->setColor(Color(0, 255, 255));       // Cyan
            break;
        case AUTHENTICATED:
            module->setColor(Color(0, 255, 0));         // Vert
            break;
        case CONFIGURED:
            module->setColor(Color(0, 0, 255));         // Bleu
            break;
        case FAILED:
            module->setColor(Color(255, 0, 0));         // Rouge
            break;
    }
}

void AKCPMCodeAdapted::registerNeighbor(int neighborId, 
                                        P2PNetworkInterface *interface) {
    if (neighbors.find(neighborId) == neighbors.end()) {
        NeighborInfo info;
        info.moduleId = neighborId;
        info.interface = interface;
        info.authState = NEIGHBOR_DISCOVERY;
        info.authenticated = false;
        neighbors[neighborId] = info;
        
        cout << "[Module " << module->blockId 
             << "] Voisin " << neighborId << " enregistré" << endl;
    }
}

bool AKCPMCodeAdapted::allNeighborsAuthenticated() {
    if (totalNeighbors == 0) return false;
    return authenticatedNeighbors.size() >= (size_t)totalNeighbors;
}

void AKCPMCodeAdapted::printState(const std::string& action) {
    cout << "[Module " << module->blockId << "] " << action << endl;
}

void AKCPMCodeAdapted::sendSecureMessage(const std::string& type, 
                                         MessagePtr msg,
                                         P2PNetworkInterface *dest) {
    // Cette fonction est conservée pour compatibilité
    // Les nouvelles fonctions spécialisées sont préférées
    int msgId = MSG_HELLO_ID;
    
    if (type == "HELLO") msgId = MSG_HELLO_ID;
    else if (type == "HELLO_REPLY") msgId = MSG_HELLO_REPLY_ID;
    else if (type == "SF2") msgId = MSG_SF2_ID;
    else if (type == "SF4") msgId = MSG_SF4_ID;
    else if (type == "AUTH_PROOF_REQ" || type == "AUTH_PROOF_CHALLENGE") 
        msgId = MSG_AUTH_PROOF_ID;
    
    MessageOf<int>* realMsg = new MessageOf<int>(msgId, 0);
    sendMessage(realMsg, dest, 100, 1000);  // ✅ 4 arguments
    
    metrics.totalMessages++;
    metrics.transmissions++;
    
    cout << "[Module " << module->blockId 
         << "] ➤ Message " << type << " (ID=" << msgId << ") envoyé" << endl;
}

void AKCPMCodeAdapted::sendSF2Message(const SF2Data& data, 
                                       P2PNetworkInterface *dest) {
    // Créer le message avec les données SF-2
    MessageOf<SF2Data>* msg = new MessageOf<SF2Data>(MSG_SF2_ID, data);
    
    // ✨ ENVOYER avec un délai FIXE de 1500 µs
    // Cela correspond au délai calculé dans SF-2
    sendMessage(msg, dest, 1500, 1500);  // t_start = t_end = 1500 µs
    
    metrics.totalMessages++;
    metrics.transmissions++;
    
    cout << "[Module " << module->blockId 
         << "] ➤ Message SF2 envoyé (ID=" << MSG_SF2_ID << ") à t=" << (scheduler->now() + 1500) << " µs" << endl;
    cout << "    n1=" << data.n1 << ", x=" << data.x << ", K0=" << data.K0 << endl;
}

void AKCPMCodeAdapted::sendSF4Message(const SF4Data& data,
                                       P2PNetworkInterface *dest) {
    // Créer le message avec les données SF-4
    MessageOf<SF4Data>* msg = new MessageOf<SF4Data>(MSG_SF4_ID, data);
    sendMessage(msg, dest, 100, 1000);  // ✅ 4 arguments
    
    metrics.totalMessages++;
    metrics.transmissions++;
    
    cout << "[Module " << module->blockId 
         << "] ➤ Message SF4 envoyé (ID=" << MSG_SF4_ID << ")" << endl;
    cout << "    x1=" << data.x1 << endl;
}

void AKCPMCodeAdapted::sendHelloMessage(const HelloData& data,
                                         P2PNetworkInterface *dest) {
    // Créer le message HELLO avec données
    MessageOf<HelloData>* msg = new MessageOf<HelloData>(MSG_HELLO_ID, data);
    sendMessage(msg, dest, 100, 1000);  // ✅ 4 arguments
    
    metrics.totalMessages++;
    metrics.transmissions++;
    
    cout << "[Module " << module->blockId 
         << "] ➤ Message HELLO envoyé (ID=" << MSG_HELLO_ID << ")" << endl;
}

void AKCPMCodeAdapted::printConfigurationSummary() {
    printState("RÉSUMÉ DE CONFIGURATION");
    
    cout << "\n" << string(60, '=') << endl;
    cout << "Module " << module->blockId << " - Configuration AKC-PM" << endl;
    cout << string(60, '=') << endl;
    
    cout << "Modèle de clés: ";
    switch (keyModel) {
        case SINGLE_KEY:
            cout << "SINGLE_KEY (1 clé pour tout le réseau)" << endl;
            break;
        case FOUR_KEYS:
            cout << "FOUR_KEYS (4 clés)" << endl;
            break;
        case MULTI_KEYS:
            cout << "MULTI_KEYS (clés pairwise)" << endl;
            break;
    }
    
    cout << "État: " << (currentState == CONFIGURED ? "CONFIGURÉ" : "EN COURS") << endl;
    cout << "Liens dans structure: " << linksInStructure << endl;
    cout << "Voisins authentifiés: " << authenticatedNeighbors.size() 
         << "/" << totalNeighbors << endl;
    cout << "Clé partagée: " << (myCrypto.keyEstablished ? "OUI" : "NON") << endl;
    
    cout << string(60, '=') << endl;
    
    // Afficher les métriques
    metrics.totalTime = scheduler->now() - startTime;
    metrics.printMetrics(module->blockId);
}
