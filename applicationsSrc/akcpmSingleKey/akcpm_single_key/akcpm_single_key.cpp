/**
 * ╔═══════════════════════════════════════════════════════════════════════════╗
 * ║         PROTOCOLE AKC-PM - MODÈLE SINGLE_KEY - POINT D'ENTRÉE            ║
 * ║                     AVEC SUPPORT DES MOUVEMENTS                           ║
 * ╚═══════════════════════════════════════════════════════════════════════════╝
 * 
 * Ce fichier contient le point d'entrée (main) pour la simulation du protocole
 * AKC-PM avec le modèle de clé unique (SINGLE_KEY).
 * 
 * Dans ce modèle:
 * 1. Tous les modules s'authentifient d'abord (protocole AKC-PM)
 * 2. Ensuite, les modules mobiles peuvent se reconfigurer vers une forme cible
 */

#include <iostream>
#include "robots/catoms3D/catoms3DSimulator.h"
#include "robots/catoms3D/catoms3DBlockCode.h"
#include "akcpmBlockCode_single_key.h"

using namespace std;
using namespace Catoms3D;

/**
 * Initialise un plan de mouvements exemple
 * À personnaliser selon la configuration souhaitée
 */
void initialiserPlanMouvements() {
    // Exemple: Plan de mouvements pour une reconfiguration simple
    // Format: ModuleMovement(posFrom, posTo, stage)
    // Les mouvements sont exécutés par étapes (stage 0, puis 1, puis 2, etc.)
    
    // Décommentez et adaptez selon votre configuration:
    
    /*
    // Étape 0: Module à (5,6,0) se déplace vers (6,6,0)
    AkcpmBlockCode::planMouvements.push_back(
        ModuleMovement(Cell3DPosition(5,6,0), Cell3DPosition(6,6,0), 0)
    );
    
    // Étape 1: Module à (6,5,0) se déplace vers (6,4,0)
    AkcpmBlockCode::planMouvements.push_back(
        ModuleMovement(Cell3DPosition(6,5,0), Cell3DPosition(6,4,0), 1)
    );
    */
    
    cout << "Plan de mouvements: " << AkcpmBlockCode::planMouvements.size() << " mouvements" << endl;
}

int main(int argc, char **argv) {
    cout << endl;
    cout << "╔═══════════════════════════════════════════════════════════════════╗" << endl;
    cout << "║                                                                   ║" << endl;
    cout << "║     PROTOCOLE AKC-PM - MODÈLE SINGLE_KEY                         ║" << endl;
    cout << "║     (Clé Unique pour tout le Réseau)                             ║" << endl;
    cout << "║     + Support des Mouvements                                      ║" << endl;
    cout << "║                                                                   ║" << endl;
    cout << "╠═══════════════════════════════════════════════════════════════════╣" << endl;
    cout << "║                                                                   ║" << endl;
    cout << "║  Phases du protocole:                                             ║" << endl;
    cout << "║  1. Authentification AKC-PM de tous les modules                   ║" << endl;
    cout << "║  2. Reconfiguration vers la forme cible (si mouvements définis)   ║" << endl;
    cout << "║                                                                   ║" << endl;
    cout << "║  Légende des couleurs:                                            ║" << endl;
    cout << "║    GRIS  = Non authentifié (ETAT_NON_LIE)                         ║" << endl;
    cout << "║    BLEU  = Module initial (iM)                                    ║" << endl;
    cout << "║    VERT  = Authentifié avec K1                                    ║" << endl;
    cout << "║                                                                   ║" << endl;
    cout << "║  Utilisation: Marquez les modules mobiles avec mobile=\"true\"      ║" << endl;
    cout << "║  dans le fichier XML de configuration.                            ║" << endl;
    cout << "║                                                                   ║" << endl;
    cout << "╚═══════════════════════════════════════════════════════════════════╝" << endl;
    cout << endl;
    
    // Initialiser le plan de mouvements (à personnaliser)
    initialiserPlanMouvements();
    
    // Créer et lancer le simulateur
    createSimulator(argc, argv, AkcpmBlockCode::buildNewBlockCode);
    
    getSimulator()->printInfo();
    deleteSimulator();
    
    return 0;
}
