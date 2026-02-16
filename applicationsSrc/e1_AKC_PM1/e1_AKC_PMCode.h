#ifndef E1_AKC_PM_CODE_H_
#define E1_AKC_PM_CODE_H_

#include "robots/catoms3D/catoms3DBlockCode.h"
#include "robots/catoms3D/catoms3DSimulator.h"

using namespace Catoms3D;

/* Messages du protocole */
static const int MSG_HELLO = 2001;
static const int MSG_KEY   = 2002;
static const int MSG_ACK   = 2003;

class e1_AKC_PMCode : public Catoms3DBlockCode {
private:
    Catoms3DBlock *module;
    int protocolStage = 0;

public:
    e1_AKC_PMCode(Catoms3DBlock *host);
    ~e1_AKC_PMCode() {}

    void startup() override;
    void myMessageFunc(std::shared_ptr<Message> msg, P2PNetworkInterface *sender);
    string onInterfaceDraw() override;

    static BlockCode *buildNewBlockCode(BuildingBlock *host) {
        return new e1_AKC_PMCode((Catoms3DBlock *)host);
    }
};

#endif

