/**
 * ╔═══════════════════════════════════════════════════════════════════════════╗
 * ║                      PROTOCOLE AKC-PM - MODÈLE SINGLE_KEY                 ║
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
 * DESCRIPTION DU MODÈLE SINGLE_KEY (Figure 3 de l'article):
 * ─────────────────────────────────────────────────────────
 * Dans ce modèle, une seule clé K1 est partagée par TOUS les modules du réseau.
 * 
 * Avantages:
 *   - Authentification simple entre deux modules de la structure
 *   - Moins d'opérations de hachage nécessaires
 *   - Gestion de clés simplifiée
 * 
 * Inconvénients:
 *   - Si un module est compromis, toute la structure est compromise
 *   - Pas de confidentialité entre les modules de la structure
 * 
 * Fonctionnement:
 *   1. Le module initial (iM) génère n2 une seule fois
 *   2. Tous les modules utilisent ce même n2 pour générer K1 = H(L(n2 mod Nb))
 *   3. Résultat: K1 identique pour tout le réseau
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
// TYPES DE MESSAGES (selon l'article Section VI)
// ═══════════════════════════════════════════════════════════════════════════

/**
 * MSG_AUTH_REQUEST: Message d'authentification (Algorithme 1, SF-2)
 * Contenu: (links, n1, x||K0)
 *   - links: nombre de liens dans la structure (1 = pas encore lié)
 *   - n1: H(n0) pour vérification de synchronisation
 *   - x: Ts ⊕ n0 (nonce protégé par XOR avec timestamp)
 *   - K0: H(L(n0 mod Nb)) empreinte d'authentification
 */
static const int MSG_AUTH_REQUEST = 2001;

/**
 * MSG_KEY_CHALLENGE: Défi pour génération de clé (Algorithme 1, SF-4)
 * Contenu: x1 = n2 ⊕ n0
 */
static const int MSG_KEY_CHALLENGE = 2002;

/**
 * MSG_STRUCTURE_READY: Signal qu'un module fait partie de la structure
 * Permet aux voisins non authentifiés d'initier l'authentification
 */
static const int MSG_STRUCTURE_READY = 2003;

// ═══════════════════════════════════════════════════════════════════════════
// ÉTATS DE SÉCURITÉ D'UN MODULE
// ═══════════════════════════════════════════════════════════════════════════

/**
 * États possibles d'un module dans le protocole AKC-PM
 */
enum EtatSecurite {
    ETAT_NON_LIE,           // Module pas encore lié à la structure
    ETAT_AUTHENTIFICATION,  // Authentification en cours
    ETAT_AUTHENTIFIE,       // Authentifié mais clé pas encore établie
    ETAT_CLE_ETABLIE        // Clé partagée établie avec succès
};

// ═══════════════════════════════════════════════════════════════════════════
// STRUCTURE D'INFORMATION DE SÉCURITÉ
// ═══════════════════════════════════════════════════════════════════════════

/**
 * Structure contenant toutes les informations de sécurité d'un module
 */
struct InfoSecurite {
    EtatSecurite etat;          // État actuel du module
    
    // Nonces (Section VI.A de l'article)
    uint64_t n0;                // Nonce secret, jamais divulgué
    uint64_t n1;                // n1 = H(n0), pour vérification de sync
    uint64_t n2;                // Généré par le module de structure pour la clé
    
    // Clés
    std::vector<uint8_t> K0;    // Empreinte d'authentification K0 = H(L(n0 mod Nb))
    std::vector<uint8_t> K1;    // Clé partagée K1 = H(L(n2 mod Nb))
    
    // Informations de structure
    int liensStructure;         // Nombre de liens authentifiés
    bool estModuleInitial;      // Vrai si c'est le module initial (iM)
    
    InfoSecurite() : etat(ETAT_NON_LIE), n0(0), n1(0), n2(0),
                     liensStructure(0), estModuleInitial(false) {}
};

// ═══════════════════════════════════════════════════════════════════════════
// CLASSE PRINCIPALE - SINGLE_KEY
// ═══════════════════════════════════════════════════════════════════════════

class AkcpmBlockCode : public Catoms3DBlockCode {
private:
    // ═══════════════════════════════════════════════════════════════════════
    // ATTRIBUTS PRIVÉS
    // ═══════════════════════════════════════════════════════════════════════
    
    InfoSecurite infoSecurite;  // Informations de sécurité du module
    
    // Paramètres du protocole (Section VII.B de l'article)
    static const int NB_LIGNES_CODE = 128;      // Nombre de lignes du code source
    static const int TAILLE_LIGNE_BITS = 256;   // Taille de chaque ligne en bits
    static const int DIVISEUR_SYNC = 10;        // Ts ≈ ts/10
    static const int TOLERANCE_SYNC = 5;        // Tolérance ±5
    static const int DELAI_TRANSMISSION = 100;  // Δt en microsecondes
    
    // Gestion des voisins
    std::map<P2PNetworkInterface*, std::vector<uint8_t>> clesVoisins;
    std::map<P2PNetworkInterface*, bool> voisinsAuthentifies;
    std::map<P2PNetworkInterface*, bool> voisinsDansStructure;
    std::set<P2PNetworkInterface*> authentificationsEnCours;
    
    // Stockage des nonces pour les authentifications en cours
    std::map<P2PNetworkInterface*, uint64_t> n0EnAttente;
    
    // Variable statique pour SINGLE_KEY: n2 global partagé
    static uint64_t n2Global;
    static bool n2GlobalDefini;

    // ═══════════════════════════════════════════════════════════════════════
    // FONCTIONS CRYPTOGRAPHIQUES (Section VI de l'article)
    // ═══════════════════════════════════════════════════════════════════════
    
    /**
     * H() - Fonction de hachage unidirectionnelle (Table I)
     * Utilise l'algorithme DJB2, léger et adapté aux microcontrôleurs
     */
    uint64_t H(uint64_t valeur);
    uint64_t H(const std::vector<uint8_t>& donnees);
    
    /**
     * L(i) - Retourne la ligne i du code source (Section VI.A)
     * Simulé de manière déterministe pour la démonstration
     */
    std::vector<uint8_t> L(int numeroLigne);
    
    /**
     * HL(n) - Calcule H(L(n mod Nb))
     * Utilisé pour générer K0 et K1
     */
    std::vector<uint8_t> HL(uint64_t n);
    
    /**
     * XOR - Opération OU exclusif (Table I)
     */
    uint64_t XOR(uint64_t a, uint64_t b);
    
    /**
     * Génère un nonce aléatoire
     */
    uint64_t genererNonce();
    
    // ═══════════════════════════════════════════════════════════════════════
    // SYNCHRONISATION TEMPORELLE (Section VI.A, VII.A)
    // ═══════════════════════════════════════════════════════════════════════
    
    /**
     * Calcule Ts ≈ ts/10
     * Permet de corriger les incertitudes de ±5 unités
     */
    uint64_t obtenirTs();
    
    /**
     * Calcule Ts ≈ (Tr - Δt)/10 côté récepteur
     */
    uint64_t calculerTs(uint64_t Tr, int deltaT);
    
    // ═══════════════════════════════════════════════════════════════════════
    // ALGORITHME 1: Authentification pour formation de structure
    // Référence: Section VI.A de l'article
    // ═══════════════════════════════════════════════════════════════════════
    
    /**
     * SF-1, SF-2: Le nouveau module (nM) initie l'authentification
     * @param dest Interface vers le module de structure
     */
    void algorithme1_Initier(P2PNetworkInterface* dest);
    
    /**
     * SF-3, SF-4: Le module de structure (sM) vérifie et répond
     * @param src Interface source du message
     * @param liens Nombre de liens du module demandeur
     * @param n1 Hash du nonce pour vérification sync
     * @param x Nonce protégé (Ts ⊕ n0)
     * @param K0 Empreinte d'authentification
     */
    void algorithme1_Verifier(P2PNetworkInterface* src, int liens, 
                               uint64_t n1, uint64_t x, 
                               const std::vector<uint8_t>& K0);
    
    /**
     * SF-5: Le nouveau module (nM) complète l'authentification
     * @param src Interface source du message
     * @param x1 Défi de clé (n2 ⊕ n0)
     */
    void algorithme1_Completer(P2PNetworkInterface* src, uint64_t x1);
    
    // ═══════════════════════════════════════════════════════════════════════
    // GESTION DE LA STRUCTURE
    // ═══════════════════════════════════════════════════════════════════════
    
    void notifierVoisinsStructurePrete();
    void verifierNouveauxVoisins();
    int compterVoisinsConnectes();

public:
    // ═══════════════════════════════════════════════════════════════════════
    // CONSTRUCTEUR / DESTRUCTEUR
    // ═══════════════════════════════════════════════════════════════════════
    
    AkcpmBlockCode(Catoms3DBlock *host);
    ~AkcpmBlockCode() override;
    
    /**
     * Fonction de création pour le simulateur
     */
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

/**
 * Message de demande d'authentification (Algorithme 1, SF-2)
 * Envoyé par le nouveau module vers le module de structure
 */
class MessageDemandeAuth : public Message {
public:
    int liens;                  // Nombre de liens dans la structure
    uint64_t n1;                // n1 = H(n0) pour vérification sync
    uint64_t x;                 // x = Ts ⊕ n0 (nonce protégé)
    std::vector<uint8_t> K0;    // K0 = H(L(n0 mod Nb)) empreinte
    
    MessageDemandeAuth(int _liens, uint64_t _n1, uint64_t _x, 
                       const std::vector<uint8_t>& _K0);
    ~MessageDemandeAuth() override;
};

/**
 * Message de défi de clé (Algorithme 1, SF-4)
 * Envoyé par le module de structure vers le nouveau module
 */
class MessageDefiCle : public Message {
public:
    uint64_t x1;                // x1 = n2 ⊕ n0
    
    MessageDefiCle(uint64_t _x1);
    ~MessageDefiCle() override;
};

/**
 * Message de notification de structure prête
 * Permet la propagation de l'authentification en cascade
 */
class MessageStructurePrete : public Message {
public:
    MessageStructurePrete();
    ~MessageStructurePrete() override;
};

} // namespace Catoms3D

#endif /* AKCPMBLOCKCODE_H_ */
