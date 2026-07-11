#include <iostream>
#include "robots/catoms3D/catoms3DSimulator.h"
#include "robots/catoms3D/catoms3DBlockCode.h"
#include "SCupCatoms3DTestCode.h"

using namespace std;
using namespace Catoms3D;

int main(int argc, char **argv) {
    cout << "\033[1;33m" << "Starting S-CUP Catoms3D TEST simulation ..." << "\033[0m" << endl;
    createSimulator(argc, argv, SCupCatoms3DTestCode::buildNewBlockCode);
    deleteSimulator();
    return 0;
}
