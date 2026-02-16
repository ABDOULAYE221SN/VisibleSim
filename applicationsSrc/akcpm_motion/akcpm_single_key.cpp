#include <iostream>
#include "robots/catoms3D/catoms3DSimulator.h"
#include "robots/catoms3D/catoms3DBlockCode.h"
#include "akcpmBlockCode_single_key.h"

using namespace std;
using namespace Catoms3D;

int main(int argc, char **argv) {
    cout << "\033[1;33m" << "Starting AKC-PM SINGLE_KEY + MOTION ..." << "\033[0m" << endl;

    createSimulator(argc, argv, AkcpmBlockCode::buildNewBlockCode);
    deleteSimulator();

    return 0;
}
