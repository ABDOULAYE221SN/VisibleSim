#ifndef MonApplication_FourKeyCode_H_
#define MonApplication_FourKeyCode_H_

#include "robots/catoms3D/catoms3DSimulator.h"
#include "robots/catoms3D/catoms3DWorld.h"
#include "robots/catoms3D/catoms3DBlockCode.h"
#include "robots/catoms3D/catoms3DMotionEngine.h"
#include <set>
#include <map>
#include <vector>
#include <queue>

using namespace Catoms3D;

// =============================================================================
// PARAMETRES DU PROTOCOLE AKC-PM (conformes à l'article)
// ATtiny45 @ 8MHz, SPONGENT-160, IR 160 bits
// =============================================================================
#define NB_LIGNES_CODE      128     // Nb lignes de code source (Nb)
#define TAILLE_LIGNE_BITS   256     // Taille d'une ligne en bits
#define HASH_OUTPUT_BYTES    20     // SPONGENT-160 → 160 bits = 20 octets
#define DIVISEUR_SYNC        10     // Ts = round(ts / 10)
#define TOLERANCE_SYNC        5     // ±5 unités de désynchronisation tolérées
#define DELAI_TRANSMISSION  100     // Δt simulé (µs)
#define MAX_STEPS            50     // Limite de pas pour la reconfiguration

// Types de messages
static const int MSG_AUTH_REQUEST    = 2001; // SF-2  Algo1 : nM → iM
static const int MSG_KEY_CHALLENGE   = 2002; // SF-4  Algo1 : iM → nM  /  SF-5 Algo2 : 2M → 1M
static const int MSG_STRUCTURE_READY = 2003; // Diffusion structure prête
static const int MSG_PROOF_MEMBERSHIP= 2004; // SF-2  Algo2 : 1M → 2M  (n1)
static const int MSG_PROOF_RELAY     = 2005; // SF-2  Algo2 : 1M → iM  (x) + relais iM → sM

// États de sécurité d'un module
enum EtatSecurite {
    ETAT_NON_LIE = 0,       // Pas encore dans la structure
    ETAT_AUTHENTIFICATION,  // Authentification en cours
    ETAT_AUTHENTIFIE,       // Authentifié, clé établie
    ETAT_CLE_ETABLIE        // Clé partagée opérationnelle
};

// Informations de sécurité locales à chaque module
struct InfoSecurite {
    EtatSecurite etat        = ETAT_NON_LIE;
    bool estModuleInitial    = false;
    int  liensStructure      = 0;       // Nombre de liens établis dans la structure

    // Nonces (160 bits = 20 octets, conformes SPONGENT-160)
    std::vector<uint8_t> n0;            // Nonce secret local (jamais transmis)
    std::vector<uint8_t> n1;            // H(n0) — empreinte d'authentification
    std::vector<uint8_t> n2;            // Nonce de génération de clé (partagé en FOUR_KEY)
                                        // iM génère n2, puis le transmet via x1 aux modules authentifiés
                                        // Tous les modules de la structure partagent le même n2

    // Clés (160 bits)
    std::vector<uint8_t> K0;            // H(L(n0 mod Nb)) — empreinte du code
    std::vector<uint8_t> K1;            // H(L(n2 mod Nb)) — clé partagée FOUR_KEY
                                        // Tous les modules authentifiés partagent la même K1
    
    // FOUR_KEY: Numéro de clé (1, 2, 3 ou 4)
    int numeroClé = 0;                  // 0 = non assigné, 1-4 = K1, K2, K3, K4
};

// =============================================================================
// MESSAGES
// =============================================================================

// SF-2 Algo1 : nM → iM  :  (liens, n1[160b], x[160b], K0[160b])
class MessageDemandeAuth : public Message {
public:
    int liens;
    std::vector<uint8_t> n1;   // 160 bits
    std::vector<uint8_t> x;    // 160 bits : Ts XOR n0
    std::vector<uint8_t> K0;   // 160 bits
    MessageDemandeAuth(int _liens,
                       const std::vector<uint8_t>& _n1,
                       const std::vector<uint8_t>& _x,
                       const std::vector<uint8_t>& _K0);
    ~MessageDemandeAuth() {}
};

// SF-4 Algo1 / SF-5 Algo2 : réponse avec x1 (160 bits)
class MessageDefiCle : public Message {
public:
    std::vector<uint8_t> x1;   // 160 bits
    explicit MessageDefiCle(const std::vector<uint8_t>& _x1);
    ~MessageDefiCle() {}
};

// Diffusion : module dans la structure notifie ses voisins
class MessageStructurePrete : public Message {
public:
    MessageStructurePrete();
    ~MessageStructurePrete() {}
};

// SF-2 Algo2 : 1M → 2M  :  (liens, n1[160b])
class MessagePreuveMembership : public Message {
public:
    int liens;
    std::vector<uint8_t> n1;   // 160 bits
    MessagePreuveMembership(int _liens, const std::vector<uint8_t>& _n1);
    ~MessagePreuveMembership() {}
};

// SF-2 Algo2 : 1M → iM  puis relais iM → sM  :  (liens, x[160b], sourceId)
class MessageRelaiPreuve : public Message {
public:
    int  liens;
    std::vector<uint8_t> x;    // 160 bits
    bID  sourceId;
    MessageRelaiPreuve(int _liens, const std::vector<uint8_t>& _x, bID _sourceId);
    ~MessageRelaiPreuve() {}
};

// =============================================================================
// CLASSE PRINCIPALE
// =============================================================================

class MonApplication_FourKeyCode : public Catoms3DBlockCode {
private:
    Catoms3DBlock *module = nullptr;

    // --- Reconfiguration ---
    Cell3DPosition myTarget;
    std::set<Cell3DPosition> visited;
    int moveSteps = 0;

    // --- AKC-PM : état local ---
    InfoSecurite infoSecurite;

    // Clés partagées avec chaque voisin authentifié (160 bits)
    std::map<P2PNetworkInterface*, std::vector<uint8_t>> clesVoisins;
    // Voisins authentifiés
    std::map<P2PNetworkInterface*, bool> voisinsAuthentifies;
    // Voisins connus comme étant dans la structure
    std::map<P2PNetworkInterface*, bool> voisinsDansStructure;
    // Authentifications en cours (évite les doublons)
    std::set<P2PNetworkInterface*> authentificationsEnCours;

    // Algo1 : n0 en attente de la réponse KEY_CHALLENGE
    std::map<P2PNetworkInterface*, std::vector<uint8_t>> n0EnAttente;
    // Algo2 : n0 en attente de la réponse KEY_CHALLENGE de 2M
    std::map<P2PNetworkInterface*, std::vector<uint8_t>> n0Algo2EnAttente;

public:
    MonApplication_FourKeyCode(Catoms3DBlock *host);
    ~MonApplication_FourKeyCode();

    void startup() override;
    void onMotionEnd() override;
    void processLocalEvent(EventPtr pev) override;
    void parseUserBlockElements(TiXmlElement *config) override;

    // --- Reconfiguration ---
    bool tryMoveToward(const Cell3DPosition &goal);

    // --- Primitives cryptographiques (conformes article) ---
    // H() : SPONGENT-160 simulé → 160 bits
    std::vector<uint8_t> H(const std::vector<uint8_t>& data);
    std::vector<uint8_t> H(uint64_t val);
    // L(i) : ligne i du code source → 256 bits
    std::vector<uint8_t> L(int ligne);
    // HL(n) = H(L(n mod Nb)) → 160 bits
    std::vector<uint8_t> HL(const std::vector<uint8_t>& n);
    std::vector<uint8_t> HL(uint64_t n);
    // XOR 160 bits
    std::vector<uint8_t> xorVec(const std::vector<uint8_t>& a, const std::vector<uint8_t>& b);
    // Nonce aléatoire 160 bits
    std::vector<uint8_t> genererNonce160();
    // Ts = round(ts / DIVISEUR_SYNC) → 160 bits
    std::vector<uint8_t> tsVersVec(uint64_t ts);

    // --- Utilitaires ---
    bool estDansStructure() const;
    void definirCommeModuleInitial();
    void afficherStats() const;
    void afficherStatsGlobal() const;  // Affiche les stats (n'importe quel module)

    // --- Algorithme 1 : authentification nM → structure ---
    void algorithme1_Initier(P2PNetworkInterface* dest);
    void algorithme1_Verifier(P2PNetworkInterface* src, int liens,
                              const std::vector<uint8_t>& n1,
                              const std::vector<uint8_t>& x,
                              const std::vector<uint8_t>& K0);
    void algorithme1_Completer(P2PNetworkInterface* src,
                               const std::vector<uint8_t>& x1);

    // --- Algorithme 2 : preuve d'appartenance entre modules de la structure ---
    void algorithme2_Initier(P2PNetworkInterface* dest);
    void algorithme2_Relayer(P2PNetworkInterface* src, int liens,
                             const std::vector<uint8_t>& x, bID sourceId);
    void algorithme2_Verifier(P2PNetworkInterface* src, int liens,
                              const std::vector<uint8_t>& n1);
    void algorithme2_Completer(P2PNetworkInterface* src,
                               const std::vector<uint8_t>& x1);

    // --- Gestion structure ---
    void notifierVoisinsStructurePrete();
    void verifierNouveauxVoisins();
    void lancerReconfiguration();

    // --- Gestionnaires de messages ---
    void surReceptionDemandeAuth(std::shared_ptr<Message> msg, P2PNetworkInterface* src);
    void surReceptionDefiCle(std::shared_ptr<Message> msg, P2PNetworkInterface* src);
    void surReceptionStructurePrete(std::shared_ptr<Message> msg, P2PNetworkInterface* src);
    void surReceptionPreuveMembership(std::shared_ptr<Message> msg, P2PNetworkInterface* src);
    void surReceptionRelaiPreuve(std::shared_ptr<Message> msg, P2PNetworkInterface* src);

    static BlockCode *buildNewBlockCode(BuildingBlock *host) {
        return new MonApplication_FourKeyCode((Catoms3DBlock*)host);
    }
};

#endif
