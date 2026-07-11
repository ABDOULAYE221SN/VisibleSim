#include <climits>
#include <algorithm>
#include "ExampleAppCode.h"

// ---------------------------------------------------------------------------
// Forme initiale : ligne de 4 modules sur l'axe X (z=2)
// ---------------------------------------------------------------------------
const std::vector<Cell3DPosition> ExampleAppCode::initialShape = {
    Cell3DPosition(1, 3, 2),   // M1
    Cell3DPosition(2, 3, 2),   // M2
    Cell3DPosition(3, 3, 2),   // M3
    Cell3DPosition(4, 3, 2),   // M4
};

// ---------------------------------------------------------------------------
// Forme cible : carré 2x2 (z=2)
// ---------------------------------------------------------------------------
const std::vector<Cell3DPosition> ExampleAppCode::targetShape = {
    Cell3DPosition(2, 2, 2),
    Cell3DPosition(3, 2, 2),
    Cell3DPosition(2, 3, 2),
    Cell3DPosition(3, 3, 2),
};

// Données globales partagées entre tous les modules
std::queue<bID>               ExampleAppCode::moveQueue;
std::map<bID, Cell3DPosition> ExampleAppCode::assignments;
bool                          ExampleAppCode::planReady = false;

// ---------------------------------------------------------------------------
static bool isInShape(const Cell3DPosition &pos,
                      const std::vector<Cell3DPosition> &shape) {
    return std::find(shape.begin(), shape.end(), pos) != shape.end();
}

// ---------------------------------------------------------------------------
ExampleAppCode::ExampleAppCode(Catoms3DBlock *host)
    : Catoms3DBlockCode(host), module(host)
{
    if (not host) return;
}

// ---------------------------------------------------------------------------
void ExampleAppCode::startup() {
    lattice = (FCCLattice *)(Catoms3D::getWorld()->lattice);

    // Le module #1 fait la planification une seule fois
    if (!planReady && module->blockId == 1) {
        std::vector<Cell3DPosition> toMove, toFill;
        for (auto &pos : initialShape)
            if (!isInShape(pos, targetShape)) toMove.push_back(pos);
        for (auto &pos : targetShape)
            if (!isInShape(pos, initialShape)) toFill.push_back(pos);

        auto world = BaseSimulator::getWorld();

        // Assigner chaque position à quitter à une position cible à remplir
        for (size_t i = 0; i < toMove.size() && i < toFill.size(); i++) {
            for (auto &pair : world->buildingBlocksMap) {
                if (pair.second->position == toMove[i]) {
                    assignments[pair.first] = toFill[i];
                    moveQueue.push(pair.first);
                    break;
                }
            }
        }

        // Colorer tous les modules selon leur rôle
        for (auto &pair : world->buildingBlocksMap) {
            auto *code = (ExampleAppCode *)pair.second->blockCode;
            if (assignments.count(pair.first)) {
                code->mustMove  = true;
                code->myTarget  = assignments[pair.first];
                pair.second->setColor(ORANGE);
            } else {
                pair.second->setColor(GREEN); // déjà en position cible
            }
        }

        planReady = true;

        // Déclencher directement le premier module de la file
        if (!moveQueue.empty()) {
            bID firstId = moveQueue.front();
            auto firstBlock = world->getBlockById(firstId);
            if (firstBlock) {
                auto *firstCode = (ExampleAppCode *)firstBlock->blockCode;
                firstCode->visited.clear();
                firstCode->visited.insert(firstCode->module->position);
                firstCode->moveSteps = 0;
                firstCode->tryMoveToward(firstCode->myTarget);
            }
        }
    }
}

// ---------------------------------------------------------------------------
bool ExampleAppCode::tryMoveToward(const Cell3DPosition &goal) {
    if (module->position == goal) return false;
    if (moveSteps >= MAX_STEPS) {
        console << "Module " << module->blockId << ": MAX_STEPS reached!\n";
        return false;
    }

    auto tab = Catoms3DMotionEngine::getAllRotationsForModule(module);
    Cell3DPosition fp;
    short fo;

    struct Candidate {
        Cell3DPosition pos;
        double dist;
        bool wasVisited;
    };
    std::vector<Candidate> candidates;

    for (auto &elem : tab) {
        elem.second.init(((Catoms3DGlBlock *)module->ptrGlBlock)->mat);
        elem.second.getFinalPositionAndOrientation(fp, fo);

        if (!lattice->isFree(fp)) continue;

        // Vérifier que le module reste connecté après le mouvement
        int nbNeighbors = 0;
        for (auto &nPos : lattice->getNeighborhood(fp)) {
            if (lattice->cellHasBlock(nPos) && nPos != module->position)
                nbNeighbors++;
        }
        if (nbNeighbors < 1) continue;

        Cell3DPosition d = fp - goal;
        double dist = (double)(d[0]*d[0] + d[1]*d[1] + d[2]*d[2]);
        candidates.push_back({fp, dist, visited.count(fp) > 0});
    }

    if (candidates.empty()) return false;

    // Préférer les positions non visitées, puis choisir la plus proche du but
    Cell3DPosition bestPos;
    double bestDist = 1e18;
    bool found = false;

    // 1er passage : positions non visitées
    for (auto &c : candidates) {
        if (!c.wasVisited && c.dist < bestDist) {
            bestDist = c.dist;
            bestPos  = c.pos;
            found    = true;
        }
    }

    // 2ème passage : si tout visité, on accepte les visitées (reset anti-boucle)
    if (!found) {
        visited.clear();
        visited.insert(module->position);
        bestDist = 1e18;
        for (auto &c : candidates) {
            if (c.dist < bestDist) {
                bestDist = c.dist;
                bestPos  = c.pos;
                found    = true;
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

// ---------------------------------------------------------------------------
void ExampleAppCode::onMotionEnd() {
    if (module->position == myTarget) {
        setColor(GREEN);
        console << "Module " << module->blockId << " reached target!\n";

        if (!moveQueue.empty()) moveQueue.pop();

        // Lancer le module suivant dans la file
        if (!moveQueue.empty()) {
            bID nextId = moveQueue.front();
            auto nextBlock = BaseSimulator::getWorld()->getBlockById(nextId);
            if (nextBlock) {
                auto *nextCode = (ExampleAppCode *)nextBlock->blockCode;
                nextCode->visited.clear();
                nextCode->visited.insert(nextCode->module->position);
                nextCode->moveSteps = 0;
                nextCode->tryMoveToward(nextCode->myTarget);
            }
        }
    } else {
        // Pas encore arrivé, continuer
        tryMoveToward(myTarget);
    }
}
