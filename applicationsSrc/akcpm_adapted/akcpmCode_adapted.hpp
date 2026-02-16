/**
 * @file akcpmCode_adapted.hpp
 * @brief Header pour l'implémentation adaptée du protocole AKC-PM
 * @author Abdullah Ndiaye
 * @date 2024-12-30
 * 
 * VERSION CORRIGÉE - Avec nouvelles fonctions pour SF-2 automatique
 */

#ifndef AKCPM_ADAPTED_CODE_H_
#define AKCPM_ADAPTED_CODE_H_

#include "robots/blinkyBlocks/blinkyBlocksSimulator.h"
#include "robots/blinkyBlocks/blinkyBlocksWorld.h"
#include "robots/blinkyBlocks/blinkyBlocksBlockCode.h"
#include <vector>
#include <map>
#include <set>
#include <memory>
#include <string>

using namespace BlinkyBlocks;

// ============================================================================
// IDS DE MESSAGES AKC-PM
// ============================================================================

static const int MSG_HELLO_ID = 2001;
static const int MSG_HELLO_REPLY_ID = 2002;
static const int MSG_SF2_ID = 2003;
static const int MSG_SF4_ID = 2004;
static const int MSG_AUTH_PROOF_ID = 2005;

// ============================================================================
// CONSTANTES DU PROTOCOLE AKC-PM
// ============================================================================

// Paramètres de sécurité selon l'article
const int NB_CODE_LINES = 128;        // Nb: Nombre de lignes de code
const int LINE_SIZE = 160;            // Taille d'une ligne (bits)
const int TIME_QUANTUM = 10;          // Quantum temporel pour Ts
const int TRANSMISSION_DELAY = 5;     // Δt: Délai de transmission

// ============================================================================
// STRUCTURES DE DONNÉES POUR LES MESSAGES
// ============================================================================

/**
 * @brief Données du message SF-2 (nouveau module → module initial)
 * Format: (linksInStructure, n1, x, K0)
 */
struct SF2Data {
    int linksInStructure;     // Nombre de liens déjà dans la structure
    uint64_t n1;              // n1 = H(n0)
    uint64_t x;               // x = Ts ⊕ n0
    uint64_t K0;              // K0 = H(L(n0 mod Nb))
    
    SF2Data() : linksInStructure(0), n1(0), x(0), K0(0) {}
    SF2Data(int links, uint64_t _n1, uint64_t _x, uint64_t _K0) 
        : linksInStructure(links), n1(_n1), x(_x), K0(_K0) {}
};

/**
 * @brief Données du message SF-4 (module initial → nouveau module)
 * Format: x1 = n2 ⊕ n0
 */
struct SF4Data {
    uint64_t x1;              // x1 = n2 ⊕ n0
    uint64_t n2;              // n2 (pour debug)
    
    SF4Data() : x1(0), n2(0) {}
    SF4Data(uint64_t _x1, uint64_t _n2) : x1(_x1), n2(_n2) {}
};

/**
 * @brief Données du message HELLO/HELLO_REPLY
 */
struct HelloData {
    int moduleId;             // ID du module
    int state;                // État actuel
    
    HelloData() : moduleId(0), state(0) {}
    HelloData(int id, int s) : moduleId(id), state(s) {}
};

// ============================================================================
// STRUCTURES DE DONNÉES
// ============================================================================

/**
 * @brief Ligne de code source pour le hachage
 */
struct CodeLine {
    uint8_t data[LINE_SIZE / 8];  // 160 bits = 20 octets
    
    CodeLine() {
        for (int i = 0; i < LINE_SIZE / 8; i++) {
            data[i] = 0;
        }
    }
};

/**
 * @brief États du protocole AKC-PM
 */
enum AKCPMState {
    UNINITIALIZED,           // Non initialisé
    INITIALIZED,             // Initialisé (SF-1 complété)
    NEIGHBOR_DISCOVERY,      // Découverte des voisins
    KEY_EXCHANGE_INIT,       // Échange de clé initié (SF-2 envoyé)
    KEY_EXCHANGE_RESP,       // Réponse à l'échange de clé
    KEY_CONFIRMATION,        // Confirmation de clé
    AUTHENTICATED,           // Authentifié (clé partagée établie)
    CONFIGURED,              // Configuré (module initial ou multi-liens)
    FAILED                   // Échec d'authentification
};

/**
 * @brief Modèles de gestion des clés (selon l'article)
 */
enum KeyModel {
    SINGLE_KEY,     // Version 1: Une clé pour tout le réseau
    FOUR_KEYS,      // Version 2: 4 clés distinctes
    MULTI_KEYS      // Version 3: Clés pairwise (une par paire)
};

/**
 * @brief Données cryptographiques d'un module
 */
struct CryptoData {
    uint64_t n0;              // Nonce initial
    uint64_t n1;              // n1 = H(n0)
    uint64_t n2;              // Nonce du module initial
    uint64_t K0;              // K0 = H(L(n0 mod Nb))
    uint64_t K1;              // Clé partagée K1 = H(L(n2 mod Nb))
    uint64_t Ts;              // Timestamp synchronisé
    uint64_t x;               // x = Ts ⊕ n0
    bool keyEstablished;      // Clé établie?
    
    CryptoData() : n0(0), n1(0), n2(0), K0(0), K1(0), 
                   Ts(0), x(0), keyEstablished(false) {}
};

/**
 * @brief Informations sur un voisin
 */
struct NeighborInfo {
    int moduleId;
    P2PNetworkInterface *interface;
    AKCPMState authState;
    bool authenticated;
    CryptoData cryptoData;
    int failedAttempts;
    
    NeighborInfo() : moduleId(-1), interface(nullptr), 
                     authState(UNINITIALIZED), authenticated(false),
                     failedAttempts(0) {}
};

/**
 * @brief Métriques de performance
 */
struct PerformanceMetrics {
    uint64_t totalTime;           // Temps total
    uint64_t hashOperations;      // Nombre d'opérations de hash
    uint64_t xorOperations;       // Nombre d'opérations XOR
    uint64_t transmissions;       // Nombre de transmissions
    uint64_t keyExchanges;        // Nombre d'échanges de clés
    uint64_t authSuccesses;       // Authentifications réussies
    uint64_t authFailures;        // Authentifications échouées
    uint64_t totalMessages;       // Total messages échangés
    
    PerformanceMetrics() : totalTime(0), hashOperations(0), xorOperations(0),
                          transmissions(0), keyExchanges(0), authSuccesses(0),
                          authFailures(0), totalMessages(0) {}
    
    void printMetrics(int moduleId) {
        std::cout << "\n=== MÉTRIQUES MODULE " << moduleId << " ===" << std::endl;
        std::cout << "Temps total: " << totalTime << " μs" << std::endl;
        std::cout << "Hash: " << hashOperations << std::endl;
        std::cout << "XOR: " << xorOperations << std::endl;
        std::cout << "Transmissions: " << transmissions << std::endl;
        std::cout << "Échanges de clés: " << keyExchanges << std::endl;
        std::cout << "Auth. réussies: " << authSuccesses << std::endl;
        std::cout << "Auth. échouées: " << authFailures << std::endl;
        std::cout << "Messages totaux: " << totalMessages << std::endl;
    }
};

// ============================================================================
// CLASSE PRINCIPALE: AKCPM CODE ADAPTED
// ============================================================================

class AKCPMCodeAdapted : public BlinkyBlocksBlockCode {
private:
    BlinkyBlocksBlock *module;              // Référence au module
    
    // État du protocole
    AKCPMState currentState;
    bool isInitialModule;                   // Module initial (iM)?
    KeyModel keyModel;                      // Modèle de clés utilisé
    
    // Code source et cryptographie
    std::vector<CodeLine> sourceCode;       // Code source hashable
    CryptoData myCrypto;                    // Mes données cryptographiques
    
    // Gestion des voisins
    std::map<int, NeighborInfo> neighbors;  // Table des voisins
    std::set<int> authenticatedNeighbors;   // Voisins authentifiés
    int neighborDiscovered;                 // Nombre de voisins découverts
    int totalNeighbors;                     // Total voisins attendus
    int linksInStructure;                   // Liens dans la structure
    
    // Métriques
    PerformanceMetrics metrics;
    uint64_t startTime;
    
    // ========================================================================
    // FONCTIONS PRIVÉES
    // ========================================================================
    
    // Initialisation
    void initializeSourceCode();
    
    // Fonctions cryptographiques
    uint64_t hashCodeLine(int lineNumber);
    uint64_t hashFunction(uint64_t data);
    uint64_t generateNonce();
    uint64_t computeTs(uint64_t currentTime);
    uint64_t xorOperation(uint64_t a, uint64_t b);
    bool verifySynchronization(uint64_t receivedN1, uint64_t receivedX,
                              uint64_t receptionTime, uint64_t& n0_derived);
    
    // Algorithm 1: Structure Formation
    void structureFormation_SF1();
    void structureFormation_SF2(P2PNetworkInterface *iM_interface);
    void structureFormation_SF3(uint64_t receivedN1, uint64_t receivedX,
                               uint64_t receivedK0, uint64_t receptionTime,
                               P2PNetworkInterface *sender);
    void structureFormation_SF4(uint64_t n0_derived, 
                               P2PNetworkInterface *sender);
    void structureFormation_SF5(uint64_t receivedX1);
    
    // Nouvelles fonctions
    void initiateSF2ToInitialModule();
    void sendHelloToAllNeighbors();
    
    // Algorithm 2: Authentication by Proof
    void authenticationByProof(int targetModuleId, 
                              P2PNetworkInterface *targetInterface);
    
    // Gestion des modèles de clés
    uint64_t generateKey(uint64_t baseNonce, int linkNumber);
    uint64_t generateSingleKey(uint64_t n2);
    uint64_t generateFourKeys(uint64_t n2, int keyIndex);
    uint64_t generatePairwiseKey(uint64_t prevKey);
    
    // Gestionnaires de messages
    void handleHelloMessage(MessagePtr msg, P2PNetworkInterface *sender);
    void handleHelloReply(MessagePtr msg, P2PNetworkInterface *sender);
    void handleSF2Message(MessagePtr msg, P2PNetworkInterface *sender);
    void handleSF4Message(MessagePtr msg, P2PNetworkInterface *sender);
    void handleAuthProof(MessagePtr msg, P2PNetworkInterface *sender);
    
    // Utilitaires
    void updateModuleColor();
    void registerNeighbor(int neighborId, P2PNetworkInterface *interface);
    bool allNeighborsAuthenticated();
    void printState(const std::string& action);
    void sendSecureMessage(const std::string& type, MessagePtr msg,
                          P2PNetworkInterface *dest);
    void sendSF2Message(const SF2Data& data, P2PNetworkInterface *dest);
    void sendSF4Message(const SF4Data& data, P2PNetworkInterface *dest);
    void sendHelloMessage(const HelloData& data, P2PNetworkInterface *dest);
    
public:
    // ========================================================================
    // INTERFACE PUBLIQUE
    // ========================================================================
    
    /**
     * @brief Constructeur
     */
    AKCPMCodeAdapted(BlinkyBlocksBlock *host);
    
    /**
     * @brief Destructeur
     */
    ~AKCPMCodeAdapted() {}
    
    /**
     * @brief Démarrage du module
     */
    void startup() override;
    
    /**
     * @brief Handlers de messages (enregistrés via addMessageEventFunc2)
     */
    void onHelloMessage(std::shared_ptr<Message> msg, P2PNetworkInterface *sender);
    void onHelloReplyMessage(std::shared_ptr<Message> msg, P2PNetworkInterface *sender);
    void onSF2Message(std::shared_ptr<Message> msg, P2PNetworkInterface *sender);
    void onSF4Message(std::shared_ptr<Message> msg, P2PNetworkInterface *sender);
    void onAuthProofMessage(std::shared_ptr<Message> msg, P2PNetworkInterface *sender);
    
    /**
     * @brief Afficher le résumé de configuration
     */
    void printConfigurationSummary();
    
    /**
     * @brief Factory method pour créer un nouveau BlockCode
     */
    static BlockCode* buildNewBlockCode(BuildingBlock *host) {
        return new AKCPMCodeAdapted((BlinkyBlocksBlock*)host);
    }
};

#endif // AKCPM_ADAPTED_CODE_H_
