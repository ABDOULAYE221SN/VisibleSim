/**
 * ╔═══════════════════════════════════════════════════════════════════════════╗
 * ║          PROTOCOLE AKC-PM - MODÈLE FOUR_KEYS - POINT D'ENTRÉE            ║
 * ╚═══════════════════════════════════════════════════════════════════════════╝
 */

#include <iostream>
#include "robots/catoms3D/catoms3DSimulator.h"
#include "robots/catoms3D/catoms3DBlockCode.h"
#include "akcpmBlockCode_four_keys.h"

using namespace std;
using namespace Catoms3D;

int main(int argc, char **argv) {
    cout << endl;
    cout << "╔═══════════════════════════════════════════════════════════════════╗" << endl;
    cout << "║                                                                   ║" << endl;
    cout << "║     PROTOCOLE AKC-PM - MODÈLE FOUR_KEYS                          ║" << endl;
    cout << "║     (4 Clés: K1, K2, K3, K4)                                      ║" << endl;
    cout << "║                                                                   ║" << endl;
    cout << "╠═══════════════════════════════════════════════════════════════════╣" << endl;
    cout << "║                                                                   ║" << endl;
    cout << "║  Description:                                                     ║" << endl;
    cout << "║  4 clés distinctes sont utilisées cycliquement:                   ║" << endl;
    cout << "║    K1 = H(L(n2 mod Nb))                                           ║" << endl;
    cout << "║    K2 = H(H(K1))                                                  ║" << endl;
    cout << "║    K3 = H(H(K2))                                                  ║" << endl;
    cout << "║    K4 = H(H(K3))                                                  ║" << endl;
    cout << "║                                                                   ║" << endl;
    cout << "║  Encodage de x1 (128 bits):                                       ║" << endl;
    cout << "║    x1 = (H^(k-1)(n2) || n2) XOR (n0 || n0)                        ║" << endl;
    cout << "║                                                                   ║" << endl;
    cout << "║  Légende des couleurs:                                            ║" << endl;
    cout << "║    GRIS   = Non authentifié                                       ║" << endl;
    cout << "║    BLEU   = Module initial (iM)                                   ║" << endl;
    cout << "║    VERT   = Authentifié avec K1                                   ║" << endl;
    cout << "║    JAUNE  = Authentifié avec K2                                   ║" << endl;
    cout << "║    ORANGE = Authentifié avec K3                                   ║" << endl;
    cout << "║    ROUGE  = Authentifié avec K4                                   ║" << endl;
    cout << "║                                                                   ║" << endl;
    cout << "╚═══════════════════════════════════════════════════════════════════╝" << endl;
    cout << endl;
    
    createSimulator(argc, argv, AkcpmBlockCode::buildNewBlockCode);
    
    getSimulator()->printInfo();
    deleteSimulator();
    
    return 0;
}
