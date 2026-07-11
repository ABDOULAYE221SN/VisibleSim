#include <iostream>
#include "robots/catoms3D/catoms3DSimulator.h"
#include "robots/catoms3D/catoms3DBlockCode.h"
#include "ExampleAppCode.h"

using namespace std;
using namespace Catoms3D;

int main(int argc, char **argv) {

    createSimulator(argc, argv, ExampleAppCode::buildNewBlockCode);
    deleteSimulator();

    return 0;
}
