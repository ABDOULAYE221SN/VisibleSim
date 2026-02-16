#include <iostream>
#include "robots/catoms3D/catoms3DSimulator.h"
#include "e1_AKC_PMCode.h"

using namespace std;
using namespace Catoms3D;

int main(int argc, char **argv) {
    cout << "\033[1;32mStarting e1_AKC_PM simulation...\033[0m" << endl;

    createSimulator(argc, argv, e1_AKC_PMCode::buildNewBlockCode);
    deleteSimulator();

    return 0;
}

