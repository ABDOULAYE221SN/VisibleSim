/**
 * MonApplication - Point d'entrée
 */

#include <iostream>
#include "robots/catoms3D/catoms3DSimulator.h"
#include "robots/catoms3D/catoms3DBlockCode.h"
#include "MonApplicationCode.hpp"

using namespace std;
using namespace Catoms3D;

int main(int argc, char **argv) {
    createSimulator(argc, argv, MonApplicationCode::buildNewBlockCode);
    getSimulator()->printInfo();
    BaseSimulator::getScheduler()->start(0);
    deleteSimulator();
    return 0;
}
