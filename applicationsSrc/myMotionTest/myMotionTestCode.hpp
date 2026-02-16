#ifndef myMotionTestCode_H_
#define myMotionTestCode_H_

#include "robots/catoms3D/catoms3DSimulator.h"
#include "robots/catoms3D/catoms3DWorld.h"
#include "robots/catoms3D/catoms3DBlockCode.h"
#include "robots/catoms3D/catoms3DMotionEngine.h"
#include <set>

using namespace Catoms3D;

class MyMotionTestCode : public Catoms3DBlockCode {
private:
    Catoms3DBlock *module = nullptr;
    bool mustMove = false;
    Cell3DPosition myTarget;

public:
    MyMotionTestCode(Catoms3DBlock *host);
    ~MyMotionTestCode() {};

    Cell3DPosition targetPos;
    std::set<Cell3DPosition> visited;   // positions deja visitees
    int moveSteps = 0;                   // nombre de pas effectues

    void startup() override;
    bool tryMoveToward(const Cell3DPosition &goal);
    void parseUserBlockElements(TiXmlElement *config) override;
    void onMotionEnd() override;

    static BlockCode *buildNewBlockCode(BuildingBlock *host) {
        return(new MyMotionTestCode((Catoms3DBlock*)host));
    }
};

#endif /* myMotionTestCode_H_ */
