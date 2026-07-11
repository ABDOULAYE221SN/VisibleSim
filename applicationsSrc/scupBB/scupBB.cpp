/**
 * @file scupBB.cpp
 * @brief Main entry point for S-CUP BlinkyBlocks application
 */

#include <iostream>
#include "scupBBCode.h"
#include "../../simulatorCore/src/robots/blinkyBlocks/blinkyBlocksSimulator.h"
#include "../../simulatorCore/src/robots/blinkyBlocks/blinkyBlocksWorld.h"

using namespace std;
using namespace BlinkyBlocks;

int main(int argc, char **argv) {
    cout << "\n╔════════════════════════════════════════════════════════════════╗\n";
    cout << "║  S-CUP: Security Protocol for Self-reconfiguration            ║\n";
    cout << "║  Implementation for BlinkyBlocks Modular Robots               ║\n";
    cout << "║  Based on WINCOM 2025 paper                                   ║\n";
    cout << "╚════════════════════════════════════════════════════════════════╝\n\n";

    try {
        createSimulator(argc, argv, SCupBBCode::buildNewBlockCode);
        getSimulator()->printInfo();
        BaseSimulator::getWorld()->printInfo();
        deleteSimulator();
    } catch(std::exception const& e) {
        cerr << "ERROR: " << e.what() << endl;
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
