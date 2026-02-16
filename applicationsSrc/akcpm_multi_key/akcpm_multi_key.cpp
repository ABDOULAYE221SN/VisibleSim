/**
 * ╔═══════════════════════════════════════════════════════════════════════════╗
 * ║          PROTOCOLE AKC-PM - MODÈLE MULTI_KEY - POINT D'ENTRÉE            ║
 * ╚═══════════════════════════════════════════════════════════════════════════╝
 */

#include <iostream>
#include "robots/catoms3D/catoms3DSimulator.h"
#include "robots/catoms3D/catoms3DBlockCode.h"
#include "akcpmBlockCode_multi_key.h"

using namespace std;
using namespace Catoms3D;

int main(int argc, char **argv) {
    cout << endl;
    cout << "╔═══════════════════════════════════════════════════════════════════╗" << endl;
    cout << "║                                                                   ║" << endl;
    cout << "║     PROTOCOLE AKC-PM - MODÈLE MULTI_KEY                          ║" << endl;
    cout << "║     (Clé Unique par Lien)                                         ║" << endl;
    cout << "║                                                                   ║" << endl;
    cout << "╠═══════════════════════════════════════════════════════════════════╣" << endl;
    cout << "║                                                                   ║" << endl;
    cout << "║  Description:                                                     ║" << endl;
    cout << "║  Dans ce modèle, CHAQUE LIEN possède sa propre clé unique.       ║" << endl;
    cout << "║  Un nouveau n2 est généré pour chaque authentification.           ║" << endl;
    cout << "║                                                                   ║" << endl;
    cout << "║  Si le réseau a w liens, il y aura w clés distinctes.            ║" << endl;
    cout << "║                                                                   ║" << endl;
    cout << "║  Avantages:                                                       ║" << endl;
    cout << "║  • Sécurité maximale (compromis = 1 seul lien affecté)           ║" << endl;
    cout << "║  • Confidentialité entre modules                                  ║" << endl;
    cout << "║  • Idéal pour communications sensibles                            ║" << endl;
    cout << "║                                                                   ║" << endl;
    cout << "║  Légende des couleurs (cycle pour chaque clé):                 ║" << endl;
    cout << "║    GRIS    = Non authentifié                                      ║" << endl;
    cout << "║    BLEU    = Module initial (iM)                                  ║" << endl;
    cout << "║    VERT    = K1 (clé 1, 7, 13...)                                 ║" << endl;
    cout << "║    JAUNE   = K2 (clé 2, 8, 14...)                                 ║" << endl;
    cout << "║    ORANGE  = K3 (clé 3, 9, 15...)                                 ║" << endl;
    cout << "║    ROUGE   = K4 (clé 4, 10, 16...)                                ║" << endl;
    cout << "║    CYAN    = K5 (clé 5, 11, 17...)                                ║" << endl;
    cout << "║    MAGENTA = K6 (clé 6, 12, 18...)                                ║" << endl;
    cout << "║                                                                   ║" << endl;
    cout << "╚═══════════════════════════════════════════════════════════════════╝" << endl;
    cout << endl;
    
    createSimulator(argc, argv, AkcpmBlockCode::buildNewBlockCode);
    
    getSimulator()->printInfo();
    deleteSimulator();
    
    return 0;
}
