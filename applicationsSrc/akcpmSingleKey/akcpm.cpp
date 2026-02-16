/**
 * ╔═══════════════════════════════════════════════════════════════════════════╗
 * ║         PROTOCOLE AKC-PM - MODÈLE SINGLE_KEY - POINT D'ENTRÉE            ║
 * ╚═══════════════════════════════════════════════════════════════════════════╝
 * 
 * Ce fichier contient le point d'entrée (main) pour la simulation du protocole
 * AKC-PM avec le modèle de clé unique (SINGLE_KEY).
 * 
 * Dans ce modèle, tous les modules de la structure partagent la même clé K1.
 */

#include <iostream>
#include "robots/catoms3D/catoms3DSimulator.h"
#include "robots/catoms3D/catoms3DBlockCode.h"
#include "akcpmBlockCode.h"

using namespace std;
using namespace Catoms3D;

int main(int argc, char **argv) {
    cout << endl;
    cout << "╔═══════════════════════════════════════════════════════════════════╗" << endl;
    cout << "║                                                                   ║" << endl;
    cout << "║     PROTOCOLE AKC-PM - MODÈLE SINGLE_KEY                         ║" << endl;
    cout << "║     (Clé Unique pour tout le Réseau)                             ║" << endl;
    cout << "║                                                                   ║" << endl;
    cout << "╠═══════════════════════════════════════════════════════════════════╣" << endl;
    cout << "║                                                                   ║" << endl;
    cout << "║  Description:                                                     ║" << endl;
    cout << "║  Dans ce modèle, une seule clé K1 est partagée par TOUS les      ║" << endl;
    cout << "║  modules du réseau. Le module initial génère n2 une seule fois,  ║" << endl;
    cout << "║  et tous les modules utilisent ce n2 pour calculer K1.           ║" << endl;
    cout << "║                                                                   ║" << endl;
    cout << "║  Avantages:                                                       ║" << endl;
    cout << "║  • Authentification simple entre modules de la structure          ║" << endl;
    cout << "║  • Moins d'opérations de hachage                                  ║" << endl;
    cout << "║  • Gestion de clés simplifiée                                     ║" << endl;
    cout << "║                                                                   ║" << endl;
    cout << "║  Légende des couleurs:                                            ║" << endl;
    cout << "║    GRIS  = Non authentifié (ETAT_NON_LIE)                         ║" << endl;
    cout << "║    BLEU  = Module initial (iM)                                    ║" << endl;
    cout << "║    VERT  = Authentifié avec K1                                    ║" << endl;
    cout << "║                                                                   ║" << endl;
    cout << "╚═══════════════════════════════════════════════════════════════════╝" << endl;
    cout << endl;
    
    // Créer et lancer le simulateur
    createSimulator(argc, argv, AkcpmBlockCode::buildNewBlockCode);
    
    getSimulator()->printInfo();
    deleteSimulator();
    
    return 0;
}
