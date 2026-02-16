#include <iostream>
#include "robots/blinkyBlocks/blinkyBlocksSimulator.h"
#include "robots/blinkyBlocks/blinkyBlocksBlockCode.h"
#include "exempleCode.h"

using namespace std;
using namespace BlinkyBlocks;

int main(int argc, char **argv) {
    createSimulator(argc, argv, ExempleCode::buildNewBlockCode);

    getSimulator()->printInfo();
    BaseSimulator::getWorld()->printInfo();
    deleteSimulator();
    return(0);
}
