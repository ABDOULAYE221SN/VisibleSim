/**
 * 
 * Ce fichier contient le point d'entrée (main) pour la simulation du protocole
 * AKC-PM avec le modèle de clé unique (SINGLE_KEY).
 * 
 * Dans ce modèle, tous les modules de la structure partagent la même clé K1.
 */

#include <iostream>
#include "robots/catoms3D/catoms3DSimulator.h"
#include "robots/catoms3D/catoms3DBlockCode.h"
#include "akcpmBlockCode_single_key.h"

using namespace std;
using namespace Catoms3D;

int main(int argc, char **argv) {
    
    
    // Créer et lancer le simulateur
    createSimulator(argc, argv, AkcpmBlockCode::buildNewBlockCode);
    
    getSimulator()->printInfo();
    deleteSimulator();
    
    return 0;
}
