/*
 * akcpm.cpp
 * 
 * AKC-PM Security Protocol for Programmable Matter
 * Based on article by Youssou Faye et al.
 */

#include <iostream>
#include "robots/catoms3D/catoms3DSimulator.h"
#include "robots/catoms3D/catoms3DBlockCode.h"
#include "akcpmBlockCode.h"

using namespace std;
using namespace Catoms3D;

int main(int argc, char **argv) {
    cout << "Starting AKC-PM Protocol Simulation..." << endl;
    
    // Créer et démarrer le simulateur
    createSimulator(argc, argv, AkcpmBlockCode::buildNewBlockCode);
    
    // Le simulateur se termine quand le scheduler est fini
    getSimulator()->printInfo();
    
    // Nettoyage
    deleteSimulator();
    
    return 0;
}
