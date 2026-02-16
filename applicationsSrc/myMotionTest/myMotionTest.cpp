#include <iostream>
#include "myMotionTestCode.hpp"

using namespace std;
using namespace Catoms3D;

int main(int argc, char **argv) {
    try {
        createSimulator(argc, argv, MyMotionTestCode::buildNewBlockCode);
        getSimulator()->printInfo();
        BaseSimulator::getScheduler()->start(0);
        deleteSimulator();
    } catch(std::exception const& e) {
        cerr << "Uncaught exception: " << e.what();
    }

    return 0;
}
