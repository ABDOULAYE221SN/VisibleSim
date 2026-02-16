/**
 * ╔═══════════════════════════════════════════════════════════════════════════╗
 * ║        PROTOCOLE AKC-PM - MODÈLE SINGLE_KEY AVEC RECONFIGURATION          ║
 * ╠═══════════════════════════════════════════════════════════════════════════╣
 * ║  Article: "A Lightweight Distributed Security Protocol with Adaptive      ║
 * ║           Key Management for Programmable Matter Based on Modular Robots" ║
 * ║                                                                           ║
 * ║  Auteurs: Youssou Faye, Abdallah Makhoul, Serigne Mbacke Diene,           ║
 * ║           Mohammed Ouzzif                                                 ║
 * ║                                                                           ║
 * ║  Implémentation: Abdoulaye Ndiaye - Mémoire de Master                     ║
 * ║  Université Assane Seck de Ziguinchor                                     ║
 * ╚═══════════════════════════════════════════════════════════════════════════╝
 * 
 * DESCRIPTION DU MODÈLE SINGLE_KEY (Figure 3 de l'article):
 * ─────────────────────────────────────────────────────────
 * Dans ce modèle, une seule clé K1 est partagée par TOUS les modules du réseau.
 * 
 * EXTENSION: RECONFIGURATION SÉCURISÉE
 * ─────────────────────────────────────
 * Les modules doivent s'authentifier avec leurs voisins AVANT tout mouvement.
 * À chaque nouvelle position, ré-authentification avec les nouveaux voisins.
 */

#ifndef AKCPMBLOCKCODE_SINGLE_KEY_H_
#define AKCPMBLOCKCODE_SINGLE_KEY_H_

#include "robots/catoms3D/catoms3DBlockCode.h"
#include "robots/catoms3D/catoms3DMotionEngine.h"
#include "comm/network.h"
#include "events/scheduler.h"
#include <vector>
#include <map>
#include <set>

namespace Catoms3D {

// ═══════════════════════════════════════════════════════════════════════════
// TYPES DE MESSAGES (selon l'article Section VI)
// ═══════════════════════════════════════════════════════════════════════════

static const int MSG_AUTH_REQUEST = 2001;
static const int MSG_KEY_CHALLENGE = 2002;
static const int MSG_STRUCTURE_READY = 2003;

// ═══════════════════════════════════════════════════════════════════════════
// ÉTATS DE SÉCURITÉ D'UN MODULE
// ═══════════════════════════════════════════════════════════════════════════

enum EtatSecurite {
    ETAT_NON_LIE,           // Module pas encore lié à la structure
    ETAT_AUTHENTIFICATION,  // Authentification en cours
    ETAT_AUTHENTIFIE,       // Authentifié mais clé pas encore établie
    ETAT_CLE_ETABLIE        // Clé partagée établie avec succès
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
    std::vector<uint8_t> K1;
    int liensStructure;
    bool estModuleInitial;
    
    InfoSecurite() : etat(ETAT_NON_LIE), n0(0), n1(0), n2(0),
                     liensStructure(0), estModuleInitial(false) {}
};

// ═══════════════════════════════════════════════════════════════════════════
// CLASSE PRINCIPALE - SINGLE_KEY AVEC RECONFIGURATION
// ═══════════════════════════════════════════════════════════════════════════

class AkcpmBlockCode : public Catoms3DBlockCode {
private:
    // ═══════════════════════════════════════════════════════════════════════
    // ATTRIBUTS AKC-PM
    // ═══════════════════════════════════════════════════════════════════════
    
    InfoSecurite infoSecurite;
    
    static const int NB_LIGNES_CODE = 128;
    static const int TAILLE_LIGNE_BITS = 256;
    static const int DIVISEUR_SYNC = 10;
    static const int TOLERANCE_SYNC = 5;
    static const int DELAI_TRANSMISSION = 100;
    
    std::map<P2PNetworkInterface*, std::vector<uint8_t>> clesVoisins;
    std::map<P2PNetworkInterface*, bool> voisinsAuthentifies;
    std::map<P2PNetworkInterface*, bool> voisinsDansStructure;
    std::set<P2PNetworkInterface*> authentificationsEnCours;
    std::map<P2PNetworkInterface*, uint64_t> n0EnAttente;
    
    static uint64_t n2Global;
    static bool n2GlobalDefini;

    // ═══════════════════════════════════════════════════════════════════════
    // ATTRIBUTS RECONFIGURATION
    // ═══════════════════════════════════════════════════════════════════════
    
    bool mustMove = false;                      // Ce module doit-il bouger ?
    Cell3DPosition myTarget;                    // Position cible assignée
    Cell3DPosition targetPos;                   // Position du prochain pas
    std::set<Cell3DPosition> visited;           // Positions déjà visitées
    int moveSteps = 0;                          // Nombre de pas effectués
    bool authenticationComplete = false;        // Authentification terminée ?
    int neighborsToAuth = 0;                    // Voisins à authentifier
    int neighborsAuthenticated = 0;             // Voisins authentifiés

    // ═══════════════════════════════════════════════════════════════════════
    // FONCTIONS CRYPTOGRAPHIQUES
    // ═══════════════════════════════════════════════════════════════════════
    
    uint64_t H(uint64_t valeur);
    uint64_t H(const std::vector<uint8_t>& donnees);
    std::vector<uint8_t> L(int numeroLigne);
    std::vector<uint8_t> HL(uint64_t n);
    uint64_t XOR(uint64_t a, uint64_t b);
    uint64_t genererNonce();
    uint64_t obtenirTs();
    uint64_t calculerTs(uint64_t Tr, int deltaT);
    
    // ═══════════════════════════════════════════════════════════════════════
    // ALGORITHME 1: Authentification
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

    // ═══════════════════════════════════════════════════════════════════════
    // FONCTIONS DE RECONFIGURATION
    // ═══════════════════════════════════════════════════════════════════════
    
    void planifierReconfiguration();
    void lancerProchainModule();
    void startAuthenticationForMove();
    void checkAuthCompleteForMove();
    bool tryMoveToward(const Cell3DPosition &goal);

public:
    // ═══════════════════════════════════════════════════════════════════════
    // CONSTRUCTEUR / DESTRUCTEUR
    // ═══════════════════════════════════════════════════════════════════════
    
    AkcpmBlockCode(Catoms3DBlock *host);
    ~AkcpmBlockCode() override;
    
    static BlockCode* buildNewBlockCode(BuildingBlock *host) {
        return new AkcpmBlockCode((Catoms3DBlock*)host);
    }
    
    // ═══════════════════════════════════════════════════════════════════════
    // GESTIONNAIRES DE MESSAGES
    // ═══════════════════════════════════════════════════════════════════════
    
    void surReceptionDemandeAuth(std::shared_ptr<Message> msg, P2PNetworkInterface* src);
    void surReceptionDefiCle(std::shared_ptr<Message> msg, P2PNetworkInterface* src);
    void surReceptionStructurePrete(std::shared_ptr<Message> msg, P2PNetworkInterface* src);
    
    // ═══════════════════════════════════════════════════════════════════════
    // ÉVÉNEMENTS DU CYCLE DE VIE
    // ═══════════════════════════════════════════════════════════════════════
    
    void startup() override;
    void processLocalEvent(EventPtr pev) override;
    void onMotionEnd() override;
    void onTap(int face) override;
    
    // ═══════════════════════════════════════════════════════════════════════
    // ACCESSEURS
    // ═══════════════════════════════════════════════════════════════════════
    
    bool estDansStructure() const;
    bool estModuleInitial() const { return infoSecurite.estModuleInitial; }
    EtatSecurite obtenirEtat() const { return infoSecurite.etat; }
    
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

class MessageDefiCle : public Message {
public:
    uint64_t x1;
    
    MessageDefiCle(uint64_t _x1);
    ~MessageDefiCle() override;
};

class MessageStructurePrete : public Message {
public:
    MessageStructurePrete();
    ~MessageStructurePrete() override;
};

} // namespace Catoms3D

#endif /* AKCPMBLOCKCODE_SINGLE_KEY_H_ */
