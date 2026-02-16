#ifndef AKCPMBLOCKCODE_H_
#define AKCPMBLOCKCODE_H_

#include "robots/catoms3D/catoms3DBlockCode.h"
#include "comm/network.h"
#include <vector>
#include <map>

namespace Catoms3D {

// IDs des messages du protocole AKC-PM
static const int AKCPM_MSG_AUTH_REQUEST = 2000;
static const int AKCPM_MSG_KEY_CHALLENGE = 2001;
static const int AKCPM_MSG_MEMBERSHIP_PROOF = 2002;
static const int AKCPM_MSG_MEMBERSHIP_PROPAGATION = 2003;
static const int AKCPM_MSG_ENCRYPTED_DATA = 2004;

// États de sécurité d'un module
enum SecurityState {
    STATE_UNLINKED,         // Pas encore lié à la structure
    STATE_AUTHENTICATING,   // En cours d'authentification
    STATE_AUTHENTICATED,    // Authentifié et lié
    STATE_KEY_GENERATED     // Clé partagée générée
};

// Modèle de gestion des clés
enum KeyModel {
    SINGLE_KEY,    // Une clé pour tout le réseau
    FOUR_KEYS,     // 4 clés pour le système
    MULTI_KEY      // Clés multiples
};

// Structure pour les informations de sécurité
struct SecurityInfo {
    SecurityState state;
    uint64_t nonce_n0;
    uint64_t nonce_n1;
    std::vector<uint8_t> sharedKey;
    uint64_t timestamp;
    int linksInStructure;
    bool isInitialModule;
    
    SecurityInfo() : state(STATE_UNLINKED), nonce_n0(0), nonce_n1(0),
                     timestamp(0), linksInStructure(0), 
                     isInitialModule(false) {}
};

class AkcpmBlockCode : public Catoms3DBlockCode {
private:
    SecurityInfo securityInfo;
    KeyModel keyManagementModel;
    
    // Paramètres du protocole
    static const int SOURCE_CODE_LINES = 128;
    static const int TRANSMISSION_DELAY = 5;
    
    // Stockage des voisins authentifiés
    std::map<P2PNetworkInterface*, std::vector<uint8_t>> neighborKeys;
    std::map<P2PNetworkInterface*, bool> authenticatedNeighbors;
    
    // Compteur de tentatives pour retry
    int authenticationAttempts;
    Time lastAttemptTime;
    
    // Fonctions cryptographiques
    uint64_t hashFunction(const std::vector<uint8_t>& data);
    uint64_t hashCodeLine(int lineNumber);
    uint64_t xorOperation(uint64_t a, uint64_t b);
    std::vector<uint8_t> generateKey(uint64_t nonce);
    uint64_t generateNonce();
    
    // Gestion du temps
    uint64_t getSynchronizedTimestamp();
    uint64_t approximateTimestamp(uint64_t tr, int delta_t);
    
    // Protocol Algorithm 1
    void initiateAuthentication(P2PNetworkInterface* dest);
    void attemptAuthentication();
    void scheduleAuthenticationRetry();
    
    // Protocol Algorithm 2
    void sendMembershipProof(P2PNetworkInterface* target);

public:
    AkcpmBlockCode(Catoms3DBlock *host);
    ~AkcpmBlockCode();
    
    // Méthode obligatoire pour créer des instances
    static BlockCode* buildNewBlockCode(BuildingBlock *host) {
        return new AkcpmBlockCode((Catoms3DBlock*)host);
    }
    
    // Handlers de messages
    void onAuthRequestReceived(std::shared_ptr<Message> msg, P2PNetworkInterface *src);
    void onKeyChallengeReceived(std::shared_ptr<Message> msg, P2PNetworkInterface *src);
    void onMembershipProofReceived(std::shared_ptr<Message> msg, P2PNetworkInterface *src);
    void onMembershipPropagationReceived(std::shared_ptr<Message> msg, P2PNetworkInterface *src);
    
    // Événements du cycle de vie
    void startup() override;
    void processLocalEvent(EventPtr pev) override;
    
    // Gestion des taps (pour tests)
    void onTap(int face) override;
    
    // Configuration
    void setAsInitialModule(bool initial);
    void setKeyModel(KeyModel model);
    
    // État
    bool isAuthenticated() const;
    SecurityState getSecurityState() const { return securityInfo.state; }
};

// Messages personnalisés

class AuthenticationRequestMessage : public Message {
public:
    int linksInStructure;
    uint64_t n1;
    uint64_t x;
    std::vector<uint8_t> K0;
    
    AuthenticationRequestMessage(int links, uint64_t _n1, uint64_t _x,
                                const std::vector<uint8_t>& fingerprint);
    ~AuthenticationRequestMessage();
};

class KeyChallengeMessage : public Message {
public:
    uint64_t x1;
    
    KeyChallengeMessage(uint64_t _x1);
    ~KeyChallengeMessage();
};

class MembershipProofMessage : public Message {
public:
    int hopsRemaining;
    uint64_t n1;          // Hash du nonce (pour vérification finale)
    uint64_t x;           // Nonce protégé (x = K ⊕ n0)
    bool toAuthenticator; // true = message (2,x) vers authenticateur, false = (2,n1) vers cible
    
    MembershipProofMessage(int hops, uint64_t _n1, uint64_t _x, bool toAuth);
    ~MembershipProofMessage();
};

class MembershipProofPropagationMessage : public Message {
public:
    int hopsRemaining;
    uint64_t x;           // x protégé à propager
    
    MembershipProofPropagationMessage(int hops, uint64_t _x);
    ~MembershipProofPropagationMessage();
};

}

#endif /* AKCPMBLOCKCODE_H_ */
