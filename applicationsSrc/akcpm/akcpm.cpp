/*
 * ═══════════════════════════════════════════════════════════════════════════
 * AKC-PM Protocol Implementation for VisibleSim
 * ═══════════════════════════════════════════════════════════════════════════
 * 
 * Based on: "A Lightweight Distributed Security Protocol with Adaptive 
 *            Key Management for Programmable Matter Based on Modular Robots"
 * 
 * Authors: Youssou Faye, Abdallah Makhoul, Serigne Mbacke Diene, Mohammed Ouzzif
 * 
 * Implementation by: Abdullah (Master's thesis)
 * 
 * ═══════════════════════════════════════════════════════════════════════════
 * 
 * Protocol Features:
 * - Algorithm 1: Authentication for structure formation (SF-1 to SF-5)
 * - Algorithm 2: Authentication by proof of membership (for Mi-Mj)
 * - Three key models: SINGLE_KEY, FOUR_KEYS, MULTI_KEY
 * - Lightweight cryptography: Hash functions, XOR, synchronized time, nonces
 * 
 * ═══════════════════════════════════════════════════════════════════════════
 */

#include <iostream>
#include "robots/catoms3D/catoms3DSimulator.h"
#include "robots/catoms3D/catoms3DBlockCode.h"
#include "akcpmBlockCode.h"

using namespace std;
using namespace Catoms3D;

int main(int argc, char **argv) {
    cout << endl;
    cout << "╔═══════════════════════════════════════════════════════════════════╗" << endl;
    cout << "║                                                                   ║" << endl;
    cout << "║     AKC-PM: Authentication, Key Management & Confidentiality      ║" << endl;
    cout << "║              for Programmable Matter (Modular Robots)             ║" << endl;
    cout << "║                                                                   ║" << endl;
    cout << "╠═══════════════════════════════════════════════════════════════════╣" << endl;
    cout << "║                                                                   ║" << endl;
    cout << "║  Protocol Components:                                             ║" << endl;
    cout << "║  • Algorithm 1: Structure Formation Authentication                ║" << endl;
    cout << "║  • Algorithm 2: Membership Proof (for existing members)           ║" << endl;
    cout << "║                                                                   ║" << endl;
    cout << "║  Three Types of Authentication (Figure 2):                        ║" << endl;
    cout << "║  • iM ↔ nM : Initial module with new module                       ║" << endl;
    cout << "║  • sM ↔ nM : Structure module with new module                     ║" << endl;
    cout << "║  • Mi ↔ Mj : Two modules already in structure                     ║" << endl;
    cout << "║                                                                   ║" << endl;
    cout << "║  Color Legend:                                                    ║" << endl;
    cout << "║    GREY   = Not yet authenticated (STATE_UNLINKED)                ║" << endl;
    cout << "║    BLUE   = Initial module (iM)                                   ║" << endl;
    cout << "║    GREEN  = Authenticated with K1                                 ║" << endl;
    cout << "║    YELLOW = Authenticated with K2 (FOUR_KEYS)                     ║" << endl;
    cout << "║    ORANGE = Authenticated with K3 (FOUR_KEYS)                     ║" << endl;
    cout << "║    RED    = Authenticated with K4 (FOUR_KEYS)                     ║" << endl;
    cout << "║                                                                   ║" << endl;
    cout << "╚═══════════════════════════════════════════════════════════════════╝" << endl;
    cout << endl;
    
    createSimulator(argc, argv, AkcpmBlockCode::buildNewBlockCode);
    
    getSimulator()->printInfo();
    deleteSimulator();
    
    return 0;
}
