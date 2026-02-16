#include "myMotionTestCode.hpp"
#include "robots/catoms3D/catoms3DMotionEngine.h"
#include <algorithm>
#include <queue>

/*
 * FORME INITIALE :        FORME CIBLE :
 *   M1  M2                    M2      
 *   M3  M4  M5             M4  M5  M1
 *                             M3 (z=3)
 */

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
    Cell3DPosition(3,3,2),  // M1 va ici
    Cell3DPosition(2,2,3),  // M3 monte ici
};

static queue<bID> moveQueue;
static bool planReady = false;
static map<bID, Cell3DPosition> assignments;
static const int MAX_STEPS = 20;

static bool isInShape(const Cell3DPosition &pos, const vector<Cell3DPosition> &shape) {
    return find(shape.begin(), shape.end(), pos) != shape.end();
}

MyMotionTestCode::MyMotionTestCode(Catoms3DBlock *host) : Catoms3DBlockCode(host), module(host) {}

void MyMotionTestCode::startup()
{
    // Module 1 fait la planification
    if (!planReady && module->blockId == 1) {
        vector<Cell3DPosition> toRemove, toFill;
        
        for (auto &pos : initialShape)
            if (!isInShape(pos, targetShape)) toRemove.push_back(pos);
        for (auto &pos : targetShape)
            if (!isInShape(pos, initialShape)) toFill.push_back(pos);

        auto world = BaseSimulator::getWorld();
        for (size_t i = 0; i < toRemove.size() && i < toFill.size(); i++) {
            for (auto &pair : world->buildingBlocksMap) {
                if (pair.second->position == toRemove[i]) {
                    assignments[pair.first] = toFill[i];
                    moveQueue.push(pair.first);
                    break;
                }
            }
        }
        planReady = true;
    }

    // Configurer les modules mobiles
    if (planReady && assignments.count(module->blockId)) {
        mustMove = true;
        myTarget = assignments[module->blockId];

        // Le premier de la file commence
        if (!moveQueue.empty() && moveQueue.front() == module->blockId) {
            visited.clear();
            visited.insert(module->position);
            moveSteps = 0;
            tryMoveToward(myTarget);
        }
    }
}

bool MyMotionTestCode::tryMoveToward(const Cell3DPosition &goal)
{
    if (module->position == goal) return false;
    if (moveSteps >= MAX_STEPS) return false;

    auto tab = Catoms3DMotionEngine::getAllRotationsForModule(module);
    Cell3DPosition fp;
    short fo;

    struct Candidate { Cell3DPosition pos; double dist; bool visited; };
    vector<Candidate> candidates;

    for (auto &elem : tab) {
        elem.second.init(((Catoms3DGlBlock *)module->ptrGlBlock)->mat);
        elem.second.getFinalPositionAndOrientation(fp, fo);

        if (!lattice->isFree(fp)) continue;

        // Vérifier connectivité
        int nbOccupied = 0;
        for (auto &nPos : lattice->getNeighborhood(fp))
            if (lattice->cellHasBlock(nPos) && nPos != module->position) nbOccupied++;
        if (nbOccupied < 1) continue;

        double dist = (fp[0]-goal[0])*(fp[0]-goal[0])
                    + (fp[1]-goal[1])*(fp[1]-goal[1])
                    + (fp[2]-goal[2])*(fp[2]-goal[2]);

        candidates.push_back({fp, dist, visited.count(fp) > 0});
    }

    if (candidates.empty()) return false;

    // Trouver la meilleure position (non visitée d'abord)
    Cell3DPosition bestPos;
    double bestDist = 999999;
    bool found = false;

    for (auto &c : candidates) {
        if (!c.visited && c.dist < bestDist) {
            bestDist = c.dist;
            bestPos = c.pos;
            found = true;
        }
    }

    // Si toutes visitées, reset
    if (!found) {
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
        visited.insert(bestPos);
        moveSteps++;
        module->moveTo(bestPos);
        return true;
    }
    return false;
}

void MyMotionTestCode::onMotionEnd()
{
    if (module->position == myTarget) {
        if (!moveQueue.empty()) moveQueue.pop();
        
        // Lancer le module suivant
        if (!moveQueue.empty()) {
            bID nextId = moveQueue.front();
            auto nextBlock = BaseSimulator::getWorld()->getBlockById(nextId);
            if (nextBlock) {
                MyMotionTestCode *nextCode = (MyMotionTestCode*)nextBlock->blockCode;
                nextCode->visited.clear();
                nextCode->visited.insert(nextCode->module->position);
                nextCode->moveSteps = 0;
                nextCode->tryMoveToward(nextCode->myTarget);
            }
        }
    } else {
        tryMoveToward(myTarget);
    }
}

void MyMotionTestCode::parseUserBlockElements(TiXmlElement *config) {}
