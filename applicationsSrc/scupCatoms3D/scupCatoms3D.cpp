/**
 * @file scupCatoms3D.cpp
 * @brief Entry point for S-CUP Protocol simulation on Catoms3D
 *
 * S-CUP: A Security Protocol for Self-reconfiguration by Clustering
 * on Programmable Matter based Modular Robots (WINCOM 2025)
 */

#include <iostream>
#include "robots/catoms3D/catoms3DSimulator.h"
#include "robots/catoms3D/catoms3DBlockCode.h"
#include "SCupCatoms3DCode.h"

using namespace std;
using namespace Catoms3D;

int main(int argc, char **argv) {
    cout << "\033[1;33m" << "Starting S-CUP Catoms3D simulation ..." << "\033[0m" << endl;

    createSimulator(argc, argv, SCupCatoms3DCode::buildNewBlockCode);
    deleteSimulator();

    return 0;
}
