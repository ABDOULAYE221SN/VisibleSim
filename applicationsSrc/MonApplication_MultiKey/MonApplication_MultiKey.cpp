#include <iostream>
#include "robots/catoms3D/catoms3DSimulator.h"
#include "robots/catoms3D/catoms3DBlockCode.h"
#include "MonApplication_MultiKeyCode.hpp"

using namespace std;
using namespace Catoms3D;

int main(int argc, char **argv) {
    cout << "\033[1;33m" << "AKC-PM MULTI_KEY + Reconfiguration" << "\033[0m" << endl;
    
    createSimulator(argc, argv, MonApplication_MultiKeyCode::buildNewBlockCode);
    getSimulator()->printInfo();
    BaseSimulator::getScheduler()->start(0);
    deleteSimulator();
    return 0;
}
