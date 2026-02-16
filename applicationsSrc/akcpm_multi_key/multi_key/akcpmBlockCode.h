/**
 * ╔═══════════════════════════════════════════════════════════════════════════╗
 * ║                     PROTOCOLE AKC-PM - MODÈLE MULTI_KEY                   ║
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
 * DESCRIPTION DU MODÈLE MULTI_KEY (Figure 5 de l'article):
 * ─────────────────────────────────────────────────────────
 * Dans ce modèle, chaque LIEN a sa propre clé unique.
 * Un nouveau n2 est généré pour CHAQUE authentification.
 * 
 * Résultat: si le réseau a w liens, il y aura w clés différentes.
 * 
 * Avantages:
 *   - Sécurité maximale: compromis d'une clé n'affecte qu'un seul lien
 *   - Confidentialité entre modules (chaque paire a sa propre clé)
 *   - Idéal pour les communications sensibles
 * 
 * Inconvénients:
 *   - Plus d'opérations de génération de clés
 *   - Plus de mémoire pour stocker les clés
 *   - Authentification Mi-Mj plus complexe (pas de clé commune)
 * 
 * Principe:
 *   Pour chaque nouvelle connexion:
 *   1. Le module de structure génère un NOUVEAU n2
 *   2. Une clé unique K = H(L(n2 mod Nb)) est créée pour ce lien
 *   3. Cette clé n'est partagée qu'entre ces deux modules
 */

#ifndef AKCPMBLOCKCODE_H_
#define AKCPMBLOCKCODE_H_

#include "robots/catoms3D/catoms3DBlockCode.h"
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
static const int MSG_KEY_CHALLENGE = 2002;    // SF-4: x1 = n2 ⊕ n0
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
// STRUCTURE D'INFORMATION DE SÉCURITÉ
// ═══════════════════════════════════════════════════════════════════════════

struct InfoSecurite {
    EtatSecurite etat;
    uint64_t n0;                        // Nonce secret actuel
    uint64_t n1;                        // H(n0)
    std::vector<uint8_t> K0;            // Empreinte d'authentification
    int liensStructure;                 // Nombre de liens établis
    bool estModuleInitial;
    
    // Dans MULTI_KEY, chaque lien a sa propre clé
    // Les clés sont stockées dans clesVoisins
    
    InfoSecurite() : etat(ETAT_NON_LIE), n0(0), n1(0),
                     liensStructure(0), estModuleInitial(false) {}
};

// ═══════════════════════════════════════════════════════════════════════════
// CLASSE PRINCIPALE - MULTI_KEY
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
    
    // Gestion des voisins - chaque voisin a sa propre clé
    std::map<P2PNetworkInterface*, std::vector<uint8_t>> clesVoisins;
    std::map<P2PNetworkInterface*, uint64_t> n2Voisins;  // n2 utilisé pour chaque voisin
    std::map<P2PNetworkInterface*, bool> voisinsAuthentifies;
    std::map<P2PNetworkInterface*, bool> voisinsDansStructure;
    std::set<P2PNetworkInterface*> authentificationsEnCours;
    std::map<P2PNetworkInterface*, uint64_t> n0EnAttente;

    // ═══════════════════════════════════════════════════════════════════════
    // FONCTIONS CRYPTOGRAPHIQUES
    // ═══════════════════════════════════════════════════════════════════════
    
    uint64_t H(uint64_t valeur);
    uint64_t H(const std::vector<uint8_t>& donnees);
    std::vector<uint8_t> L(int numeroLigne);
    std::vector<uint8_t> HL(uint64_t n);
    uint64_t XOR(uint64_t a, uint64_t b);
    uint64_t genererNonce();
    
    // ═══════════════════════════════════════════════════════════════════════
    // SYNCHRONISATION TEMPORELLE
    // ═══════════════════════════════════════════════════════════════════════
    
    uint64_t obtenirTs();
    uint64_t calculerTs(uint64_t Tr, int deltaT);
    
    // ═══════════════════════════════════════════════════════════════════════
    // ALGORITHME 1 (adapté pour MULTI_KEY)
    // ═══════════════════════════════════════════════════════════════════════
    
    void algorithme1_Initier(P2PNetworkInterface* dest);
    void algorithme1_Verifier(P2PNetworkInterface* src, int liens, 
                               uint64_t n1, uint64_t x, 
                               const std::vector<uint8_t>& K0);
    void algorithme1_Completer(P2PNetworkInterface* src, uint64_t x1);
    
    // ═══════════════════════════════════════════════════════════════════════
    // GESTION DE LA STRUCTURE
    // ═══════════════════════════════════════════════════════════════════════
    
    void notifierVoisinsStructurePrete();
    void verifierNouveauxVoisins();
    int compterVoisinsConnectes();

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
    
    /**
     * Retourne le nombre de clés distinctes (= nombre de liens)
     */
    int obtenirNombreCles() const { return clesVoisins.size(); }
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

class MessageDefiCle : public Message {
public:
    uint64_t x1;    // x1 = n2 ⊕ n0 (64 bits, pas besoin de 128 bits)
    
    MessageDefiCle(uint64_t _x1);
    ~MessageDefiCle() override;
};

class MessageStructurePrete : public Message {
public:
    MessageStructurePrete();
    ~MessageStructurePrete() override;
};

} // namespace Catoms3D

#endif /* AKCPMBLOCKCODE_H_ */
