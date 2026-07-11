#ifndef EXAMPLEAPPCODE_H_
#define EXAMPLEAPPCODE_H_

#include <set>
#include <queue>
#include <map>
#include "robots/catoms3D/catoms3DSimulator.h"
#include "robots/catoms3D/catoms3DWorld.h"
#include "robots/catoms3D/catoms3DBlockCode.h"
#include "robots/catoms3D/catoms3DMotionEngine.h"

using namespace Catoms3D;

/**
 * ExampleApp: reconfiguration Catoms3D
 *
 * Forme initiale : ligne horizontale de 4 modules
 * Forme cible    : carré 2x2
 *
 * Stratégie : le module #1 planifie les assignations (qui va où),
 * puis les modules bougent un par un via une file d'attente globale.
 */
class ExampleAppCode : public Catoms3DBlockCode {
public:
    Catoms3DBlock *module = nullptr;
    FCCLattice    *lattice = nullptr;

    bool mustMove = false;
    Cell3DPosition myTarget;
    std::set<Cell3DPosition> visited;
    int moveSteps = 0;
    static const int MAX_STEPS = 30;

    // Formes (partagées entre tous les modules via static)
    static const std::vector<Cell3DPosition> initialShape;
    static const std::vector<Cell3DPosition> targetShape;

    // File de mouvement et assignations (partagées)
    static std::queue<bID>              moveQueue;
    static std::map<bID, Cell3DPosition> assignments;
    static bool planReady;

    ExampleAppCode(Catoms3DBlock *host);
    ~ExampleAppCode() {}

    void startup() override;
    bool tryMoveToward(const Cell3DPosition &goal);
    void onMotionEnd() override;

    static BlockCode *buildNewBlockCode(BuildingBlock *host) {
        return new ExampleAppCode((Catoms3DBlock *)host);
    }
};

#endif /* EXAMPLEAPPCODE_H_ */
