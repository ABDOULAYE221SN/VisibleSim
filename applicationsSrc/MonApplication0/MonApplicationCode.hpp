#ifndef MonApplicationCode_H_
#define MonApplicationCode_H_

#include "robots/catoms3D/catoms3DSimulator.h"
#include "robots/catoms3D/catoms3DWorld.h"
#include "robots/catoms3D/catoms3DBlockCode.h"
#include "robots/catoms3D/catoms3DMotionEngine.h"
#include <set>

using namespace Catoms3D;

class MonApplicationCode : public Catoms3DBlockCode {
private:
    Catoms3DBlock *module = nullptr;
    bool mustMove = false;
    Cell3DPosition myTarget;

public:
    MonApplicationCode(Catoms3DBlock *host);
    ~MonApplicationCode() {};

    Cell3DPosition targetPos;
    std::set<Cell3DPosition> visited;
    int moveSteps = 0;

    void startup() override;
    void onMotionEnd() override;
    void parseUserBlockElements(TiXmlElement *config) override;
    
    bool tryMoveToward(const Cell3DPosition &goal);

    static BlockCode *buildNewBlockCode(BuildingBlock *host) {
        return (new MonApplicationCode((Catoms3DBlock*)host));
    }
};

#endif /* MonApplicationCode_H_ */
