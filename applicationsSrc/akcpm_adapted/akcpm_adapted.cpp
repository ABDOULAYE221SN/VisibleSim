/**
 * @file akcpm_adapted.cpp
 * @brief Point d'entrée de l'application AKC-PM Adaptée
 * @author Abdullah Ndiaye
 * @date 2024-12-30
 * 
 * Version adaptée du protocole AKC-PM selon l'article:
 * Faye et al. (2024) "A Lightweight Distributed Security Protocol with 
 * Adaptive Key Management for Programmable Matter Based on Modular Robots"
 */

#include <iostream>
#include "akcpmCode_adapted.hpp"

using namespace std;
using namespace BlinkyBlocks;

int main(int argc, char **argv) {
    try {
        cout << "\n" << string(75, '=') << endl;
        cout << "   SIMULATEUR AKC-PM - VERSION ADAPTÉE" << endl;
        cout << "   Authenticated Key exchange and Characterization" << endl;
        cout << "   for Programmable Matter" << endl;
        cout << "" << endl;
        cout << "   Basé sur l'article de Faye et al. (2024)" << endl;
        cout << string(75, '=') << "\n" << endl;
        
        cout << "Éléments clés implémentés:" << endl;
        cout << "  ✓ Hash des lignes de code: K0 = H(L(n0 mod Nb))" << endl;
        cout << "  ✓ Synchronisation temporelle: Ts ≈ (ts/10)" << endl;
        cout << "  ✓ Protection du nonce: x = Ts ⊕ n0" << endl;
        cout << "  ✓ Algorithm 1: Structure Formation (SF-1 à SF-5)" << endl;
        cout << "  ✓ Algorithm 2: Authentication by Proof" << endl;
        cout << "  ✓ 3 modèles de clés: Single, Four Keys, Multi-Keys" << endl;
        cout << "" << endl;
        
        // Créer le simulateur avec le BlockCode AKC-PM Adapté
        createSimulator(argc, argv, AKCPMCodeAdapted::buildNewBlockCode);
        
        // Afficher les informations du simulateur
        getSimulator()->printInfo();
        BaseSimulator::getWorld()->printInfo();
        
        cout << "\n" << string(75, '=') << endl;
        cout << "Simulation terminée" << endl;
        cout << string(75, '=') << "\n" << endl;
        
        // Nettoyer
        deleteSimulator();
        
    } catch(std::exception const& e) {
        cerr << "\n!!! EXCEPTION NON CAPTURÉE !!!" << endl;
        cerr << "Message: " << e.what() << endl;
        return 1;
    }
    
    return 0;
}
