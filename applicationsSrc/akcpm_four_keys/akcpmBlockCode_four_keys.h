/**
 * ╔═══════════════════════════════════════════════════════════════════════════╗
 * ║                     PROTOCOLE AKC-PM - MODÈLE FOUR_KEYS                   ║
 * ╠═══════════════════════════════════════════════════════════════════════════╣
 * ║  Article: "A Lightweight Distributed Security Protocol with Adaptive      ║
 * ║           Key Management for Programmable Matter Based on Modular Robots" ║
 * ║                                                                           ║
 * ║  Auteurs: Youssou Faye, Abdallah Makhoul, Serigne Mbacke Diene,          ║
 * ║           Mohammed Ouzzif                                                 ║
 * ║                                                                           ║
 * ║  Implémentation: Abdullah - Mémoire de Master                            ║
 * ║  Université Assane Seck de Ziguinchor                                    ║
 * ╚═══════════════════════════════════════════════════════════════════════════╝
 * 
 * DESCRIPTION DU MODÈLE FOUR_KEYS (Figure 4 de l'article):
 * ────────────────────────────────────────────────────────
 * Dans ce modèle, 4 clés distinctes sont utilisées dans le réseau:
 *   - K1 = H(L(n2 mod Nb))           pour les premiers modules (1M)
 *   - K2 = H(H(K1))                  pour les seconds modules (2M)
 *   - K3 = H(H(K2))                  pour les troisièmes modules (3M)
 *   - K4 = H(H(K3))                  pour les quatrièmes modules (4M)
 * 
 * Avantages:
 *   - Meilleure sécurité que SINGLE_KEY (compromis partiel)
 *   - Structure en groupes de 4 modules avec clés différentes
 *   - Balance entre sécurité et complexité
 * 
 * Mécanisme d'encodage de x1 (Section VI.B):
 *   Pour 1M: x1 = (n2||n2) ⊕ (n0||n0)
 *   Pour 2M: x1 = (H(n2)||n2) ⊕ (n0||n0)
 *   Pour 3M: x1 = (H(H(n2))||n2) ⊕ (n0||n0)
 *   Pour 4M: x1 = (H(H(H(n2)))||n2) ⊕ (n0||n0)
 */

#ifndef AKCPMBLOCKCODE_FOUR_KEYS_H_
#define AKCPMBLOCKCODE_FOUR_KEYS_H_

#include "robots/catoms3D/catoms3DBlockCode.h"
#include "robots/catoms3D/catoms3DWorld.h"
#include "robots/catoms3D/catoms3DMotionEngine.h"
#include "comm/network.h"
#include "events/scheduler.h"
#include <vector>
#include <map>
#include <set>

namespace Catoms3D {

// ═══════════════════════════════════════════════════════════════════════════
// TYPES DE MESSAGES
// ═══════════════════════════════════════════════════════════════════════════

static const int MSG_AUTH_REQUEST = 2001;     // SF-2: (liens, n1, x||K0)
static const int MSG_KEY_CHALLENGE = 2002;    // SF-4: x1 (128 bits pour FOUR_KEYS)
static const int MSG_STRUCTURE_READY = 2003;  // Signal structure prête

// ═══════════════════════════════════════════════════════════════════════════
// ÉTATS DE SÉCURITÉ
// ═══════════════════════════════════════════════════════════════════════════

enum EtatSecurite {
    ETAT_NON_LIE,
    ETAT_AUTHENTIFICATION,
    ETAT_AUTHENTIFIE,
    ETAT_CLE_ETABLIE
};

// ═══════════════════════════════════════════════════════════════════════════
// NIVEAU DE CLÉ POUR FOUR_KEYS
// ═══════════════════════════════════════════════════════════════════════════

/**
 * Niveaux de clé dans le modèle FOUR_KEYS
 * Chaque niveau correspond à une clé différente
 */
enum NiveauCle {
    CLE_K1 = 1,   // K1 = H(L(n2 mod Nb))
    CLE_K2 = 2,   // K2 = H(H(K1))
    CLE_K3 = 3,   // K3 = H(H(K2))
    CLE_K4 = 4    // K4 = H(H(K3))
};

// ═══════════════════════════════════════════════════════════════════════════
// STRUCTURE D'INFORMATION DE SÉCURITÉ
// ═══════════════════════════════════════════════════════════════════════════

struct InfoSecurite {
    EtatSecurite etat;
    uint64_t n0;
    uint64_t n1;
    uint64_t n2;
    std::vector<uint8_t> K0;
    std::vector<uint8_t> clePartagee;  // K1, K2, K3 ou K4 selon le niveau
    int niveauCle;                      // 1, 2, 3 ou 4
    int liensStructure;
    bool estModuleInitial;
    
    InfoSecurite() : etat(ETAT_NON_LIE), n0(0), n1(0), n2(0),
                     niveauCle(0), liensStructure(0), estModuleInitial(false) {}
};

// ═══════════════════════════════════════════════════════════════════════════
// CLASSE PRINCIPALE - FOUR_KEYS
// ═══════════════════════════════════════════════════════════════════════════

class AkcpmBlockCode : public Catoms3DBlockCode {
private:
    InfoSecurite infoSecurite;
    
    // Paramètres du protocole
    static const int NB_LIGNES_CODE = 128;
    static const int TAILLE_LIGNE_BITS = 256;
    static const int DIVISEUR_SYNC = 10;
    static const int TOLERANCE_SYNC = 5;
    static const int DELAI_TRANSMISSION = 100;
    
    // Gestion des voisins
    std::map<P2PNetworkInterface*, std::vector<uint8_t>> clesVoisins;
    std::map<P2PNetworkInterface*, int> niveauxClesVoisins;  // Niveau de clé par voisin
    std::map<P2PNetworkInterface*, bool> voisinsAuthentifies;
    std::map<P2PNetworkInterface*, bool> voisinsDansStructure;
    std::set<P2PNetworkInterface*> authentificationsEnCours;
    std::map<P2PNetworkInterface*, uint64_t> n0EnAttente;
    
    // Compteur pour l'attribution des niveaux de clé (cycle 1,2,3,4,1,2,...)
    int compteurNiveauCle;
    
    // n2 global pour FOUR_KEYS
    static uint64_t n2Global;
    static bool n2GlobalDefini;
    
    // === RECONFIGURATION ===
    Cell3DPosition myTarget;
    std::set<Cell3DPosition> visited;
    int moveSteps;
    bool isMoving;
    Cell3DPosition lastPosition;

    // ═══════════════════════════════════════════════════════════════════════
    // FONCTIONS CRYPTOGRAPHIQUES
    // ═══════════════════════════════════════════════════════════════════════
    
    uint64_t H(uint64_t valeur);
    uint64_t H(const std::vector<uint8_t>& donnees);
    std::vector<uint8_t> L(int numeroLigne);
    std::vector<uint8_t> HL(uint64_t n);
    uint64_t XOR(uint64_t a, uint64_t b);
    uint64_t genererNonce();
    
    /**
     * Applique H() n fois: H(H(H(...H(valeur)...)))
     * Utilisé pour générer K2, K3, K4 à partir de K1
     */
    uint64_t appliquerHashNFois(uint64_t valeur, int n);
    
    /**
     * Opérations sur 128 bits pour FOUR_KEYS
     * x1 = (H^(k-1)(n2) || n2) ⊕ (n0 || n0)
     */
    void xor128(uint64_t aHaut, uint64_t aBas,
                uint64_t bHaut, uint64_t bBas,
                uint64_t& resultatHaut, uint64_t& resultatBas);
    
    // ═══════════════════════════════════════════════════════════════════════
    // SYNCHRONISATION TEMPORELLE
    // ═══════════════════════════════════════════════════════════════════════
    
    uint64_t obtenirTs();
    uint64_t calculerTs(uint64_t Tr, int deltaT);
    
    // ═══════════════════════════════════════════════════════════════════════
    // ALGORITHME 1 (adapté pour FOUR_KEYS)
    // ═══════════════════════════════════════════════════════════════════════
    
    void algorithme1_Initier(P2PNetworkInterface* dest);
    void algorithme1_Verifier(P2PNetworkInterface* src, int liens, 
                               uint64_t n1, uint64_t x, 
                               const std::vector<uint8_t>& K0);
    void algorithme1_Completer(P2PNetworkInterface* src, 
                                uint64_t x1Haut, uint64_t x1Bas, int niveauCle);
    
    // ═══════════════════════════════════════════════════════════════════════
    // GESTION DE LA STRUCTURE
    // ═══════════════════════════════════════════════════════════════════════
    
    void notifierVoisinsStructurePrete();
    void verifierNouveauxVoisins();
    int compterVoisinsConnectes();
    
    // ═══════════════════════════════════════════════════════════════════════
    // RECONFIGURATION
    // ═══════════════════════════════════════════════════════════════════════
    
    bool tryMoveToward(const Cell3DPosition &goal);

public:
    AkcpmBlockCode(Catoms3DBlock *host);
    ~AkcpmBlockCode() override;
    
    static BlockCode* buildNewBlockCode(BuildingBlock *host) {
        return new AkcpmBlockCode((Catoms3DBlock*)host);
    }
    
    void surReceptionDemandeAuth(std::shared_ptr<Message> msg, P2PNetworkInterface* src);
    void surReceptionDefiCle(std::shared_ptr<Message> msg, P2PNetworkInterface* src);
    void surReceptionStructurePrete(std::shared_ptr<Message> msg, P2PNetworkInterface* src);
    
    void startup() override;
    void processLocalEvent(EventPtr pev) override;
    void onMotionEnd() override;
    void onTap(int face) override;
    
    bool estDansStructure() const;
    bool estModuleInitial() const { return infoSecurite.estModuleInitial; }
    void definirCommeModuleInitial();
};

// ═══════════════════════════════════════════════════════════════════════════
// CLASSES DE MESSAGES
// ═══════════════════════════════════════════════════════════════════════════

class MessageDemandeAuth : public Message {
public:
    int liens;
    uint64_t n1;
    uint64_t x;
    std::vector<uint8_t> K0;
    
    MessageDemandeAuth(int _liens, uint64_t _n1, uint64_t _x, 
                       const std::vector<uint8_t>& _K0);
    ~MessageDemandeAuth() override;
};

/**
 * Message de défi de clé pour FOUR_KEYS
 * Contient x1 sur 128 bits: (x1Haut || x1Bas)
 */
class MessageDefiCle : public Message {
public:
    uint64_t x1Haut;    // Partie haute de x1 (H^(k-1)(n2) ⊕ n0)
    uint64_t x1Bas;     // Partie basse de x1 (n2 ⊕ n0)
    int niveauCle;      // Niveau de clé (1, 2, 3 ou 4)
    
    MessageDefiCle(uint64_t _x1Haut, uint64_t _x1Bas, int _niveauCle);
    ~MessageDefiCle() override;
};

class MessageStructurePrete : public Message {
public:
    MessageStructurePrete();
    ~MessageStructurePrete() override;
};

} // namespace Catoms3D

#endif /* AKCPMBLOCKCODE_FOUR_KEYS_H_ */
