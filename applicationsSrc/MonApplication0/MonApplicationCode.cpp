#include "MonApplicationCode.hpp"
#include "robots/catoms3D/catoms3DMotionEngine.h"
#include <algorithm>
#include <queue>

using namespace std;

static vector<Cell3DPosition> initialShape = {
    Cell3DPosition(1,3,2),  // M1
    Cell3DPosition(2,3,2),  // M2
    Cell3DPosition(1,2,2),  // M3
    Cell3DPosition(2,2,2),  // M4
    Cell3DPosition(3,2,2),  // M5
};

static vector<Cell3DPosition> targetShape = {
    Cell3DPosition(2,3,2),  // M2 reste
    Cell3DPosition(2,2,2),  // M4 reste
    Cell3DPosition(3,2,2),  // M5 reste
    Cell3DPosition(3,3,2),  // M1 va ici (entre M2 et M5)
    Cell3DPosition(2,2,3),  // M3 monte ici
};

static queue<bID> moveQueue;
static bool planReady = false;
static int moveCount = 0;
static map<bID, Cell3DPosition> assignments;
static const int MAX_STEPS = 20;

static bool isInShape(const Cell3DPosition &pos, const vector<Cell3DPosition> &shape) {
    return find(shape.begin(), shape.end(), pos) != shape.end();
}

MonApplicationCode::MonApplicationCode(Catoms3DBlock *host) : Catoms3DBlockCode(host), module(host)
{
    if (not host) return;
}

void MonApplicationCode::startup()
{
    console << "start " << module->blockId
            << " pos=" << module->position << "\n";

    if (!planReady && module->blockId == 1) {
        console << "=== PLANIFICATION ===\n";

        vector<Cell3DPosition> toRemove;
        for (auto &pos : initialShape) {
            if (!isInShape(pos, targetShape)) {
                toRemove.push_back(pos);
            }
        }

        vector<Cell3DPosition> toFill;
        for (auto &pos : targetShape) {
            if (!isInShape(pos, initialShape)) {
                toFill.push_back(pos);
            }
        }

        console << "A liberer : " << toRemove.size() << "\n";
        for (auto &p : toRemove) console << "  - " << p << "\n";
        console << "A remplir : " << toFill.size() << "\n";
        for (auto &p : toFill) console << "  + " << p << "\n";

        auto world = BaseSimulator::getWorld();
        for (size_t i = 0; i < toRemove.size() && i < toFill.size(); i++) {
            for (auto &pair : world->buildingBlocksMap) {
                if (pair.second->position == toRemove[i]) {
                    assignments[pair.first] = toFill[i];
                    moveQueue.push(pair.first);
                    console << "Module " << pair.first
                            << " : " << toRemove[i]
                            << " -> " << toFill[i] << "\n";
                    break;
                }
            }
        }

        planReady = true;
        console << "=== PLAN PRET ===\n";
    }

    if (planReady && assignments.count(module->blockId)) {
        mustMove = true;
        myTarget = assignments[module->blockId];
        module->setColor(ORANGE);

        if (!moveQueue.empty() && moveQueue.front() == module->blockId) {
            module->setColor(RED);
            visited.clear();
            visited.insert(module->position);
            moveSteps = 0;
            tryMoveToward(myTarget);
        }
    } else {
        module->setColor(BLUE);
    }
}

bool MonApplicationCode::tryMoveToward(const Cell3DPosition &goal)
{
    if (module->position == goal) {
        module->setColor(GREEN);
        return false;
    }

    if (moveSteps >= MAX_STEPS) {
        console << "Module " << module->blockId
                << " : LIMITE DE PAS ATTEINTE (" << MAX_STEPS << ")\n";
        module->setColor(YELLOW);
        return false;
    }

    auto tab = Catoms3DMotionEngine::getAllRotationsForModule(module);
    Cell3DPosition fp;
    short fo;

    struct Candidate {
        Cell3DPosition pos;
        double dist;
        bool alreadyVisited;
    };
    vector<Candidate> candidates;

    for (auto &elem : tab) {
        elem.second.init(((Catoms3DGlBlock *)module->ptrGlBlock)->mat);
        elem.second.getFinalPositionAndOrientation(fp, fo);

        if (!lattice->isFree(fp)) continue;

        // Connectivite
        vector<Cell3DPosition> neighbors = lattice->getNeighborhood(fp);
        int nbOccupied = 0;
        for (auto &nPos : neighbors) {
            if (lattice->cellHasBlock(nPos) && nPos != module->position) {
                nbOccupied++;
            }
        }
        if (nbOccupied < 1) continue;

        double dist = (fp[0]-goal[0])*(fp[0]-goal[0])
                    + (fp[1]-goal[1])*(fp[1]-goal[1])
                    + (fp[2]-goal[2])*(fp[2]-goal[2]);

        bool wasVisited = visited.count(fp) > 0;
        candidates.push_back({fp, dist, wasVisited});
    }

    if (candidates.empty()) {
        console << "Module " << module->blockId << " : BLOQUE\n";
        module->setColor(YELLOW);
        return false;
    }

    // Priorite 1 : positions NON visitees, la plus proche
    Cell3DPosition bestPos;
    double bestDist = 999999;
    bool found = false;

    for (auto &c : candidates) {
        if (!c.alreadyVisited && c.dist < bestDist) {
            bestDist = c.dist;
            bestPos = c.pos;
            found = true;
        }
    }

    // Priorite 2 : si toutes visitees, reset et reprendre
    if (!found) {
        console << "Module " << module->blockId
                << " : reset positions visitees\n";
        visited.clear();
        visited.insert(module->position);

        for (auto &c : candidates) {
            if (c.dist < bestDist) {
                bestDist = c.dist;
                bestPos = c.pos;
                found = true;
            }
        }
    }

    if (found) {
        targetPos = bestPos;
        visited.insert(bestPos);
        moveSteps++;
        console << "Module " << module->blockId
                << " [pas " << moveSteps << "] : "
                << module->position << " -> " << bestPos << "\n";
        module->moveTo(bestPos);
        return true;
    }

    return false;
}

void MonApplicationCode::onMotionEnd()
{
    console << "Module " << module->blockId
            << " : arrive a " << module->position << "\n";

    if (module->position == myTarget) {
        module->setColor(GREEN);
        moveCount++;
        console << "Module " << module->blockId
                << " : === CIBLE ATTEINTE en " << moveSteps << " pas ===\n";

        if (!moveQueue.empty()) moveQueue.pop();
        if (!moveQueue.empty()) {
            bID nextId = moveQueue.front();
            console << "=== Tour du module " << nextId << " ===\n";
            auto world = BaseSimulator::getWorld();
            auto nextBlock = world->getBlockById(nextId);
            if (nextBlock) {
                MonApplicationCode *nextCode =
                    (MonApplicationCode*)nextBlock->blockCode;
                nextCode->module->setColor(RED);
                nextCode->visited.clear();
                nextCode->visited.insert(nextCode->module->position);
                nextCode->moveSteps = 0;
                nextCode->tryMoveToward(nextCode->myTarget);
            }
        } else {
            console << "================================\n";
            console << "  RECONFIGURATION TERMINEE !\n";
            console << "  Modules deplaces : " << moveCount << "\n";
            console << "================================\n";
        }
    } else {
        module->setColor(MAGENTA);
        tryMoveToward(myTarget);
    }
}

void MonApplicationCode::parseUserBlockElements(TiXmlElement *config)
{
    const char *attr = config->Attribute("leader");
    if (attr != nullptr) {
        std::cout << module->blockId << " is leader!" << std::endl;
    }
}
