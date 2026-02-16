#ifndef exempleCode_H_
#define exempleCode_H_
#include "robots/blinkyBlocks/blinkyBlocksBlockCode.h"

static const int BROADCAST_MSG=1001;

using namespace BlinkyBlocks;

class ExempleCode : public BlinkyBlocksBlockCode {
private:
    int distance;
public :
    ExempleCode(BlinkyBlocksBlock *host):BlinkyBlocksBlockCode(host) {};
    ~ExempleCode() {};

    void startup() override;
    void myBroadcastFunc(MessagePtr anonMsg, P2PNetworkInterface *sender);

/*****************************************************************************/
/** needed to associate code to module                                      **/
    static BlockCode *buildNewBlockCode(BuildingBlock *host) {
        return (new ExempleCode((BlinkyBlocksBlock*)host));
    };
/*****************************************************************************/
};
#endif /* exempleCode_H_ */
