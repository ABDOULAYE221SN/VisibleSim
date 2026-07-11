#include <iostream>
#include "robots/catoms3D/catoms3DSimulator.h"
#include "robots/catoms3D/catoms3DBlockCode.h"
#include "MonApplicationTestCode.hpp"

using namespace std;
using namespace Catoms3D;

int main(int argc, char **argv) {
    createSimulator(argc, argv, MonApplicationTestCode::buildNewBlockCode);
    getSimulator()->printInfo();
    deleteSimulator();
    return 0;
}
