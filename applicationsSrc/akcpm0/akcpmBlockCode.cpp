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
    : Catoms3DBlockCode(host), keyManagementModel(SINGLE_KEY) {
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
        
        for(int i = 0; i < 12; i++) {
            P2PNetworkInterface* interface = catom->getInterface(i);
            
            if(interface && interface->connectedInterface) {
                console << "Neighbor found on interface " << i 
                       << ", requesting authentication...\n";
                // Envoyer une demande d'authentification
                initiateAuthentication(interface);
            }
        }
    }
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
            }
            break;
        }
            
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
    
    // ✅ CORRECT selon l'article :
    // Seul le module initial OU un module déjà authentifié peut authentifier
    if(!securityInfo.isInitialModule && 
       securityInfo.state != STATE_AUTHENTICATED) {
        console << "  ✗ Cannot authenticate: not initial and not in structure\n";
        return;
    }
    
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
// ALGORITHM 2: MEMBERSHIP PROOF (À implémenter)
//=============================================================================

void AkcpmBlockCode::sendMembershipProof(P2PNetworkInterface* target) {
    console << "Sending membership proof (Algorithm 2 - TODO)\n";
    // TODO: Implémenter Algorithm 2 complet
}

void AkcpmBlockCode::onMembershipProofReceived(std::shared_ptr<Message> message,
                                               P2PNetworkInterface* src) {
    console << "Received membership proof (Algorithm 2 - TODO)\n";
    // TODO: Implémenter Algorithm 2 complet
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
    int hops, uint64_t _n1, uint64_t _x, bool structMsg)
    : Message(), hopsRemaining(hops), n1(_n1), x(_x),
      isStructureMessage(structMsg) {
    type = AKCPM_MSG_MEMBERSHIP_PROOF;
}

MembershipProofMessage::~MembershipProofMessage() {}
