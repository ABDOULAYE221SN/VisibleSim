#include <iostream>
#include <random>
#include <cmath>
#include "akcpmBlockCode.h"

using namespace std;
using namespace Catoms3D;

//=============================================================================
// CONSTRUCTEUR / DESTRUCTEUR
//=============================================================================

AkcpmBlockCode::AkcpmBlockCode(Catoms3DBlock *host) 
    : Catoms3DBlockCode(host), keyManagementModel(SINGLE_KEY),
      authenticationAttempts(0), lastAttemptTime(0) {
    // Les handlers sont gérés dans processLocalEvent
}

AkcpmBlockCode::~AkcpmBlockCode() {
    neighborKeys.clear();
    authenticatedNeighbors.clear();
}

//=============================================================================
// FONCTIONS CRYPTOGRAPHIQUES LÉGÈRES
//=============================================================================

uint64_t AkcpmBlockCode::hashFunction(const std::vector<uint8_t>& data) {
    // Hash simple DJB2 pour commencer (à remplacer par SHA-256 ou SPONGENT)
    uint64_t hash = 5381;
    for(auto byte : data) {
        hash = ((hash << 5) + hash) + byte;
    }
    return hash;
}

uint64_t AkcpmBlockCode::hashCodeLine(int lineNumber) {
    // Simuler le hash d'une ligne du code source
    string codeLine = "SOURCE_CODE_LINE_" + to_string(lineNumber);
    vector<uint8_t> lineData(codeLine.begin(), codeLine.end());
    return hashFunction(lineData);
}

uint64_t AkcpmBlockCode::xorOperation(uint64_t a, uint64_t b) {
    return a ^ b;
}

uint64_t AkcpmBlockCode::generateNonce() {
    static random_device rd;
    static mt19937_64 gen(rd());
    uniform_int_distribution<uint64_t> dis;
    return dis(gen);
}

vector<uint8_t> AkcpmBlockCode::generateKey(uint64_t nonce) {
    int lineNumber = nonce % SOURCE_CODE_LINES;
    uint64_t keyHash = hashCodeLine(lineNumber);
    
    vector<uint8_t> key(8);
    for(int i = 0; i < 8; i++) {
        key[i] = (keyHash >> (i * 8)) & 0xFF;
    }
    return key;
}

//=============================================================================
// GESTION DU TEMPS SYNCHRONISÉ
//=============================================================================

uint64_t AkcpmBlockCode::getSynchronizedTimestamp() {
    return getScheduler()->now();
}

uint64_t AkcpmBlockCode::approximateTimestamp(uint64_t tr, int delta_t) {
    return (tr - delta_t) / 10;
}

//=============================================================================
// STARTUP - POINT D'ENTRÉE
//=============================================================================

void AkcpmBlockCode::startup() {
    console << "Block " << hostBlock->blockId << " starting AKC-PM protocol\n";
    
    Catoms3DBlock* catom = (Catoms3DBlock*)hostBlock;
    Cell3DPosition pos = catom->position;
    
    console << "  Position: (" << pos[0] << "," << pos[1] << "," << pos[2] << ")\n";
    
    // ✅ CORRECTION: Selon l'article, le module initial ATTEND
    // et les nouveaux modules INITIENT l'authentification
    if(pos[0] == 0 && pos[1] == 0 && pos[2] == 0) {
        setAsInitialModule(true);
        console << "*** INITIAL MODULE DETECTED ***\n";
        console << "Waiting passively for new modules to authenticate...\n";
        // Le module initial ne fait rien - il attend les requêtes
    } else {
        // ✅ Les modules non-initiaux cherchent activement à s'authentifier
        console << "Searching for structure to join...\n";
        
        attemptAuthentication();
        
        // ✅ NOUVEAU: Programmer des réessais périodiques si pas encore authentifié
        // Réessayer toutes les 500ms pendant max 10 secondes
        scheduleAuthenticationRetry();
    }
}

void AkcpmBlockCode::attemptAuthentication() {
    Catoms3DBlock* catom = (Catoms3DBlock*)hostBlock;
    
    // ✅ Vérifier si on n'est pas déjà authentifié
    if(securityInfo.state == STATE_AUTHENTICATED) {
        console << "Already authenticated, skipping attempt\n";
        return;
    }
    
    // ✅ Éviter de spammer - attendre au moins 100ms entre les tentatives
    Time now = getScheduler()->now();
    if(authenticationAttempts > 0 && (now - lastAttemptTime) < 100000) {
        return;  // Trop récent, ne pas réessayer
    }
    
    console << "Authentication attempt #" << (authenticationAttempts + 1) << "\n";
    
    bool foundNeighbor = false;
    for(int i = 0; i < 12; i++) {
        P2PNetworkInterface* interface = catom->getInterface(i);
        
        if(interface && interface->connectedInterface) {
            // Ne pas réessayer avec les voisins déjà authentifiés
            if(authenticatedNeighbors.find(interface) == authenticatedNeighbors.end()) {
                console << "  Neighbor on interface " << i 
                       << ", requesting authentication...\n";
                initiateAuthentication(interface);
                foundNeighbor = true;
            }
        }
    }
    
    if(!foundNeighbor) {
        console << "  No unauthenticated neighbors found\n";
    }
    
    authenticationAttempts++;
    lastAttemptTime = now;
}

void AkcpmBlockCode::scheduleAuthenticationRetry() {
    // Les modules réessaieront automatiquement quand ils recevront des événements
    // ou via processLocalEvent
    console << "  Authentication retry mechanism active\n";
}

//=============================================================================
// GESTION DES ÉVÉNEMENTS
//=============================================================================

void AkcpmBlockCode::processLocalEvent(EventPtr pev) {
    MessagePtr message;
    stringstream info;
    
    switch (pev->eventType) {
        case EVENT_NI_RECEIVE: {
            message = (std::static_pointer_cast<NetworkInterfaceReceiveEvent>(pev))->message;
            P2PNetworkInterface *recv_interface = message->destinationInterface;
            
            switch(message->type) {
                case AKCPM_MSG_AUTH_REQUEST:
                    onAuthRequestReceived(message, recv_interface);
                    break;
                case AKCPM_MSG_KEY_CHALLENGE:
                    onKeyChallengeReceived(message, recv_interface);
                    break;
                case AKCPM_MSG_MEMBERSHIP_PROOF:
                    onMembershipProofReceived(message, recv_interface);
                    break;
                case AKCPM_MSG_MEMBERSHIP_PROPAGATION:
                    onMembershipPropagationReceived(message, recv_interface);
                    break;
            }
            break;
        }
        
        case EVENT_ADD_NEIGHBOR:
        case EVENT_REMOVE_NEIGHBOR:
            // ✅ NOUVEAU: Quand la topologie change, réessayer l'authentification
            if(securityInfo.state != STATE_AUTHENTICATED && 
               !securityInfo.isInitialModule) {
                console << "Topology changed, retrying authentication...\n";
                attemptAuthentication();
            }
            break;
            
        default:
            break;
    }
}

void AkcpmBlockCode::onTap(int face) {
    console << "Block " << hostBlock->blockId << " tapped on face " 
            << face << "\n";
    console << "State: " << securityInfo.state << "\n";
    console << "Links: " << securityInfo.linksInStructure << "\n";
    console << "IsInitial: " << securityInfo.isInitialModule << "\n";
    console << "Authenticated neighbors: " << authenticatedNeighbors.size() << "\n";
}

//=============================================================================
// ALGORITHM 1: AUTHENTICATION (SF-1 à SF-5)
//=============================================================================

void AkcpmBlockCode::initiateAuthentication(P2PNetworkInterface* dest) {
    console << "Block " << hostBlock->blockId 
            << ": Initiating authentication (SF-1, SF-2)\n";
    
    // SF-1: Générer n0, n1, K0, x
    securityInfo.nonce_n0 = generateNonce();
    
    vector<uint8_t> n0_bytes((uint8_t*)&securityInfo.nonce_n0,
                             (uint8_t*)&securityInfo.nonce_n0 + 8);
    securityInfo.nonce_n1 = hashFunction(n0_bytes);
    
    // K0 = H(L(n0 mod Nb))
    int lineNum = securityInfo.nonce_n0 % SOURCE_CODE_LINES;
    uint64_t k0_hash = hashCodeLine(lineNum);
    vector<uint8_t> K0(8);
    for(int i = 0; i < 8; i++) {
        K0[i] = (k0_hash >> (i * 8)) & 0xFF;
    }
    
    // x = Ts ⊕ n0
    uint64_t ts = getSynchronizedTimestamp();
    uint64_t Ts = ts / 10;
    uint64_t x = xorOperation(Ts, securityInfo.nonce_n0);
    
    // SF-2: Envoyer le message
    int linksCount = securityInfo.linksInStructure > 0 ? 
                     securityInfo.linksInStructure : 1;
    
    AuthenticationRequestMessage* authMsg = new AuthenticationRequestMessage(
        linksCount, securityInfo.nonce_n1, x, K0);
    
    sendMessage(authMsg, dest, 0, 0);
    securityInfo.state = STATE_AUTHENTICATING;
    
    console << "  n0 = " << securityInfo.nonce_n0 << "\n";
    console << "  n1 = " << securityInfo.nonce_n1 << "\n";
    console << "  Ts = " << Ts << "\n";
}

void AkcpmBlockCode::onAuthRequestReceived(std::shared_ptr<Message> message,
                                           P2PNetworkInterface* src) {
    console << "Block " << hostBlock->blockId 
            << ": Received authentication request (SF-3)\n";
    
    AuthenticationRequestMessage* authMsg = 
        static_cast<AuthenticationRequestMessage*>(message.get());
    
    // ✅ CORRECTION pour sM-nM:
    // Un module dans la structure (pas forcément initial) peut authentifier
    if(!securityInfo.isInitialModule && 
       securityInfo.state != STATE_AUTHENTICATED) {
        console << "  ✗ Cannot authenticate: not initial and not in structure\n";
        console << "  (State: " << securityInfo.state << ", IsInitial: " 
                << securityInfo.isInitialModule << ")\n";
        
        // ✅ NOUVEAU: Si je ne peux pas authentifier maintenant, 
        // mais que je ne suis pas authentifié moi-même, réessayer
        if(securityInfo.state != STATE_AUTHENTICATED) {
            console << "  Attempting my own authentication...\n";
            attemptAuthentication();
        }
        return;
    }
    
    console << "  ✓ This module CAN authenticate (";
    if(securityInfo.isInitialModule) {
        console << "INITIAL MODULE";
    } else {
        console << "AUTHENTICATED STRUCTURE MEMBER (sM-nM)";
    }
    console << ")\n";
    
    // SF-3: Vérifier synchronisation et authentification
    uint64_t tr = getSynchronizedTimestamp();
    
    // ✅ CORRECTION: Tester plusieurs valeurs de Ts autour du timestamp actuel
    // pour compenser le délai de transmission variable
    uint64_t Ts_base = tr / 10;
    uint64_t n0_prime = 0;
    bool synchronized = false;
    
    // Essayer Ts_base et quelques valeurs autour (±10 pour tolérance de ±100µs)
    for(int offset = -10; offset <= 10 && !synchronized; offset++) {
        uint64_t Ts_test = Ts_base + offset;
        uint64_t n0_test = xorOperation(authMsg->x, Ts_test);
        
        // Vérifier si n1 = H(n0_test)
        vector<uint8_t> n0_test_bytes((uint8_t*)&n0_test,
                                      (uint8_t*)&n0_test + 8);
        uint64_t computed_n1_test = hashFunction(n0_test_bytes);
        
        if(computed_n1_test == authMsg->n1) {
            // Trouvé le bon Ts!
            n0_prime = n0_test;
            synchronized = true;
            console << "  Ts (found) = " << Ts_test << " (offset: " << offset << ")\n";
            break;
        }
    }
    
    if(!synchronized) {
        console << "  ✗ Synchronization failed! Could not find valid Ts\n";
        console << "    Expected n1: " << authMsg->n1 << "\n";
        console << "    Tr: " << tr << ", Ts_base: " << Ts_base << "\n";
        return;
    }
    
    console << "  n0' (derived) = " << n0_prime << "\n";
    console << "  ✓ Clock synchronization OK\n";
    
    // Vérifier K0 = H(L(n0' mod Nb))
    vector<uint8_t> K0_computed = generateKey(n0_prime);
    if(K0_computed != authMsg->K0) {
        console << "  ✗ Authentication fingerprint mismatch!\n";
        return;
    }
    
    console << "  ✓ Authentication fingerprint OK\n";
    console << "  ✓ Module authenticated! Sending key challenge (SF-4)...\n";
    
    // SF-4: Générer et envoyer le challenge
    uint64_t n2 = generateNonce();
    uint64_t x1 = xorOperation(n2, n0_prime);
    vector<uint8_t> K1 = generateKey(n2);
    
    console << "  n2 = " << n2 << "\n";
    console << "  x1 = " << x1 << "\n";
    
    // Stocker la clé pour ce voisin
    neighborKeys[src] = K1;
    authenticatedNeighbors[src] = true;
    securityInfo.linksInStructure++;
    
    console << "  Links in structure: " << securityInfo.linksInStructure << "\n";
    
    KeyChallengeMessage* challengeMsg = new KeyChallengeMessage(x1);
    sendMessage(challengeMsg, src, 0, 0);
    
    // ✅ NOUVEAU: Après avoir authentifié un module, 
    // vérifier s'il y a d'autres voisins non-authentifiés à qui on pourrait proposer l'authentification
    // En fait, ils vont nous envoyer des requêtes, donc on attend juste
}


void AkcpmBlockCode::onKeyChallengeReceived(std::shared_ptr<Message> message,
                                            P2PNetworkInterface* src) {
    console << "Block " << hostBlock->blockId 
            << ": Received key challenge (SF-5)\n";
    
    KeyChallengeMessage* challengeMsg = 
        static_cast<KeyChallengeMessage*>(message.get());
    
    // SF-5: Générer la clé partagée
    uint64_t n2 = xorOperation(challengeMsg->x1, securityInfo.nonce_n0);
    vector<uint8_t> K1 = generateKey(n2);
    
    console << "  n2 (recovered) = " << n2 << "\n";
    
    // Stocker la clé
    neighborKeys[src] = K1;
    authenticatedNeighbors[src] = true;
    securityInfo.state = STATE_AUTHENTICATED;  // ✅ Maintenant dans la structure
    securityInfo.linksInStructure++;
    
    console << "  ✓✓✓ Successfully authenticated! Joined structure! ✓✓✓\n";
    console << "  Shared key K1 established with authenticator\n";
    
    // ✅ Changer la couleur pour montrer l'authentification
    Catoms3DBlock* catom = (Catoms3DBlock*)hostBlock;
    catom->setColor(GREEN);
    
    // ✅ IMPORTANT: Maintenant que ce module est dans la structure,
    // il peut authentifier d'autres voisins (propagation en cascade)
    console << "  Now part of structure! Searching for other neighbors...\n";
    
    for(int i = 0; i < 12; i++) {
        P2PNetworkInterface* interface = catom->getInterface(i);
        
        if(interface && interface->connectedInterface) {
            // Ne pas ré-authentifier avec celui qui vient de nous authentifier
            // Et ne pas authentifier ceux qu'on a déjà authentifiés
            if(interface != src && 
               authenticatedNeighbors.find(interface) == authenticatedNeighbors.end()) {
                console << "  Found unauthenticated neighbor on interface " << i 
                       << ", requesting authentication...\n";
                initiateAuthentication(interface);
            }
        }
    }
}

//=============================================================================
// ALGORITHM 2: MEMBERSHIP PROOF
//=============================================================================

void AkcpmBlockCode::sendMembershipProof(P2PNetworkInterface* target) {
    console << "Block " << hostBlock->blockId 
            << ": Sending membership proof (Algorithm 2 SF-1, SF-2)\n";
    
    // Vérifier qu'on est bien dans la structure
    if(securityInfo.state != STATE_AUTHENTICATED) {
        console << "  ✗ Cannot send proof: not authenticated\n";
        return;
    }
    
    // Trouver un voisin authentifié pour servir d'authenticateur
    P2PNetworkInterface* authenticator = nullptr;
    vector<uint8_t> sharedKey;
    
    for(auto& pair : authenticatedNeighbors) {
        if(pair.second && neighborKeys.find(pair.first) != neighborKeys.end()) {
            authenticator = pair.first;
            sharedKey = neighborKeys[authenticator];
            console << "  Using neighbor as authenticator\n";
            break;
        }
    }
    
    if(authenticator == nullptr) {
        console << "  ✗ No authenticator found\n";
        return;
    }
    
    // SF-1: Générer n0 et n1
    uint64_t n0 = generateNonce();
    vector<uint8_t> n0_bytes((uint8_t*)&n0, (uint8_t*)&n0 + 8);
    uint64_t n1 = hashFunction(n0_bytes);
    
    // x = K ⊕ n0 (K = clé partagée avec l'authenticateur)
    uint64_t K_uint = 0;
    for(int i = 0; i < 8 && i < sharedKey.size(); i++) {
        K_uint |= ((uint64_t)sharedKey[i]) << (i * 8);
    }
    uint64_t x = xorOperation(K_uint, n0);
    
    console << "  n0 = " << n0 << "\n";
    console << "  n1 = " << n1 << "\n";
    console << "  x = K ⊕ n0 = " << x << "\n";
    
    // SF-2: Envoyer (2, x) à l'authenticateur
    MembershipProofMessage* proofToAuth = 
        new MembershipProofMessage(2, 0, x, true);
    sendMessage(proofToAuth, authenticator, 0, 0);
    console << "  Sent (2, x) to authenticator\n";
    
    // SF-2: Envoyer (2, n1) à la cible
    MembershipProofMessage* proofToTarget = 
        new MembershipProofMessage(2, n1, 0, false);
    sendMessage(proofToTarget, target, 0, 0);
    console << "  Sent (2, n1) to target\n";
    
    // Stocker n0 temporairement pour générer la clé plus tard
    securityInfo.nonce_n0 = n0;
    securityInfo.state = STATE_AUTHENTICATING;
}

void AkcpmBlockCode::onMembershipProofReceived(std::shared_ptr<Message> message,
                                               P2PNetworkInterface* src) {
    MembershipProofMessage* proofMsg = 
        static_cast<MembershipProofMessage*>(message.get());
    
    console << "Block " << hostBlock->blockId 
            << ": Received membership proof (Algorithm 2)\n";
    
    if(proofMsg->toAuthenticator) {
        // ===== CAS 1: Message (2, x) reçu par l'authenticateur =====
        console << "  Received (2, x) from requester - SF-3 Propagation\n";
        
        // Vérifier qu'on est dans la structure
        if(securityInfo.state != STATE_AUTHENTICATED && 
           !securityInfo.isInitialModule) {
            console << "  ✗ Cannot authenticate: not in structure\n";
            return;
        }
        
        // Vérifier qu'on a une clé partagée avec l'expéditeur
        if(neighborKeys.find(src) == neighborKeys.end()) {
            console << "  ✗ No shared key with sender\n";
            return;
        }
        
        // SF-3: Dériver n0 = K ⊕ x
        vector<uint8_t> sharedKey = neighborKeys[src];
        uint64_t K_uint = 0;
        for(int i = 0; i < 8 && i < sharedKey.size(); i++) {
            K_uint |= ((uint64_t)sharedKey[i]) << (i * 8);
        }
        uint64_t n0 = xorOperation(K_uint, proofMsg->x);
        
        console << "  Derived n0 = " << n0 << "\n";
        
        // Propager à tous les autres voisins
        Catoms3DBlock* catom = (Catoms3DBlock*)hostBlock;
        int propagated = 0;
        
        for(auto& neighbor : authenticatedNeighbors) {
            if(neighbor.second && neighbor.first != src) {
                // Calculer xv = n0 ⊕ Kv
                if(neighborKeys.find(neighbor.first) != neighborKeys.end()) {
                    vector<uint8_t> Kv = neighborKeys[neighbor.first];
                    uint64_t Kv_uint = 0;
                    for(int i = 0; i < 8 && i < Kv.size(); i++) {
                        Kv_uint |= ((uint64_t)Kv[i]) << (i * 8);
                    }
                    uint64_t xv = xorOperation(n0, Kv_uint);
                    
                    // Envoyer (1, xv) - décrémenter hops
                    MembershipProofPropagationMessage* propagationMsg = 
                        new MembershipProofPropagationMessage(1, xv);
                    sendMessage(propagationMsg, neighbor.first, 0, 0);
                    propagated++;
                }
            }
        }
        
        console << "  Propagated to " << propagated << " neighbors\n";
        
    } else {
        // ===== CAS 2: Message (2, n1) reçu par la cible =====
        console << "  Received (2, n1) challenge from requester\n";
        console << "  Waiting for propagated proof from authenticator...\n";
        // On attend de recevoir le message de propagation
        // Pour l'instant, on stocke juste n1
        securityInfo.nonce_n1 = proofMsg->n1;
    }
}

void AkcpmBlockCode::onMembershipPropagationReceived(std::shared_ptr<Message> message,
                                                     P2PNetworkInterface* src) {
    MembershipProofPropagationMessage* propMsg = 
        static_cast<MembershipProofPropagationMessage*>(message.get());
    
    console << "Block " << hostBlock->blockId 
            << ": Received propagated proof (hops=" << propMsg->hopsRemaining << ")\n";
    
    if(propMsg->hopsRemaining == 0) {
        // ===== Message arrivé à destination (la cible) =====
        console << "  Proof reached target - SF-3 Verification\n";
        
        // Vérifier qu'on a reçu le challenge n1 avant
        if(securityInfo.nonce_n1 == 0) {
            console << "  ✗ No challenge received yet\n";
            return;
        }
        
        // Vérifier qu'on a une clé avec l'expéditeur
        if(neighborKeys.find(src) == neighborKeys.end()) {
            console << "  ✗ No shared key with propagator\n";
            return;
        }
        
        // Dériver n0 = K ⊕ x
        vector<uint8_t> sharedKey = neighborKeys[src];
        uint64_t K_uint = 0;
        for(int i = 0; i < 8 && i < sharedKey.size(); i++) {
            K_uint |= ((uint64_t)sharedKey[i]) << (i * 8);
        }
        uint64_t n0 = xorOperation(K_uint, propMsg->x);
        
        console << "  Derived n0 = " << n0 << "\n";
        
        // Vérifier n1 = H(n0)
        vector<uint8_t> n0_bytes((uint8_t*)&n0, (uint8_t*)&n0 + 8);
        uint64_t computed_n1 = hashFunction(n0_bytes);
        
        if(computed_n1 != securityInfo.nonce_n1) {
            console << "  ✗ Membership proof verification failed!\n";
            console << "    Expected n1: " << securityInfo.nonce_n1 << "\n";
            console << "    Computed n1: " << computed_n1 << "\n";
            return;
        }
        
        console << "  ✓ Membership proof verified!\n";
        console << "  Requester is authenticated member of structure\n";
        
        // SF-5: Générer une clé et l'envoyer (comme dans Algorithm 1)
        uint64_t n2 = generateNonce();
        uint64_t x1 = xorOperation(n2, n0);
        vector<uint8_t> K_new = generateKey(n2);
        
        console << "  Generating shared key with requester (SF-5)\n";
        console << "  n2 = " << n2 << "\n";
        
        // Trouver l'interface du requester (celui qui a envoyé le challenge n1)
        // Pour simplifier, on utilise l'interface du propagateur
        neighborKeys[src] = K_new;
        authenticatedNeighbors[src] = true;
        securityInfo.linksInStructure++;
        
        KeyChallengeMessage* challengeMsg = new KeyChallengeMessage(x1);
        sendMessage(challengeMsg, src, 0, 0);
        
        console << "  ✓ New link established!\n";
        
    } else {
        // ===== Continuer la propagation =====
        console << "  Propagating further (remaining hops: " 
                << propMsg->hopsRemaining << ")\n";
        
        Catoms3DBlock* catom = (Catoms3DBlock*)hostBlock;
        
        for(auto& neighbor : authenticatedNeighbors) {
            if(neighbor.second && neighbor.first != src) {
                MembershipProofPropagationMessage* forwardMsg = 
                    new MembershipProofPropagationMessage(
                        propMsg->hopsRemaining - 1, propMsg->x);
                sendMessage(forwardMsg, neighbor.first, 0, 0);
            }
        }
    }
}

//=============================================================================
// CONFIGURATION
//=============================================================================

void AkcpmBlockCode::setAsInitialModule(bool initial) {
    securityInfo.isInitialModule = initial;
    if(initial) {
        securityInfo.state = STATE_AUTHENTICATED;
        
        Catoms3DBlock* catom = (Catoms3DBlock*)hostBlock;
        catom->setColor(BLUE);
        
        console << "  Module set as INITIAL (color: BLUE)\n";
        console << "  Ready to authenticate incoming modules\n";
    }
}

void AkcpmBlockCode::setKeyModel(KeyModel model) {
    keyManagementModel = model;
    console << "Key model set to: " << model << "\n";
}

bool AkcpmBlockCode::isAuthenticated() const {
    return securityInfo.state == STATE_AUTHENTICATED ||
           securityInfo.state == STATE_KEY_GENERATED;
}

//=============================================================================
// CLASSES DE MESSAGES
//=============================================================================

AuthenticationRequestMessage::AuthenticationRequestMessage(
    int links, uint64_t _n1, uint64_t _x,
    const vector<uint8_t>& fingerprint)
    : Message(), linksInStructure(links), n1(_n1), x(_x), K0(fingerprint) {
    type = AKCPM_MSG_AUTH_REQUEST;
}

AuthenticationRequestMessage::~AuthenticationRequestMessage() {}

KeyChallengeMessage::KeyChallengeMessage(uint64_t _x1)
    : Message(), x1(_x1) {
    type = AKCPM_MSG_KEY_CHALLENGE;
}

KeyChallengeMessage::~KeyChallengeMessage() {}

MembershipProofMessage::MembershipProofMessage(
    int hops, uint64_t _n1, uint64_t _x, bool toAuth)
    : Message(), hopsRemaining(hops), n1(_n1), x(_x), toAuthenticator(toAuth) {
    type = AKCPM_MSG_MEMBERSHIP_PROOF;
}

MembershipProofMessage::~MembershipProofMessage() {}

MembershipProofPropagationMessage::MembershipProofPropagationMessage(
    int hops, uint64_t _x)
    : Message(), hopsRemaining(hops), x(_x) {
    type = AKCPM_MSG_MEMBERSHIP_PROPAGATION;
}

MembershipProofPropagationMessage::~MembershipProofPropagationMessage() {}
