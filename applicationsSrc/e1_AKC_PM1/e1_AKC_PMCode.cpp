#include "e1_AKC_PMCode.h"
#include <iostream>

e1_AKC_PMCode::e1_AKC_PMCode(Catoms3DBlock *host)
        : Catoms3DBlockCode(host), module(host) {

    if (!host) return;

    addMessageEventFunc2(MSG_HELLO,
        std::bind(&e1_AKC_PMCode::myMessageFunc, this,
                  std::placeholders::_1, std::placeholders::_2));

    addMessageEventFunc2(MSG_KEY,
        std::bind(&e1_AKC_PMCode::myMessageFunc, this,
                  std::placeholders::_1, std::placeholders::_2));

    addMessageEventFunc2(MSG_ACK,
        std::bind(&e1_AKC_PMCode::myMessageFunc, this,
                  std::placeholders::_1, std::placeholders::_2));
}

void e1_AKC_PMCode::startup() {
    if (getId() == 1) {
        protocolStage = 1;
        setColor(RED);

        sendMessageToAllNeighbors(
            "HELLO",
            new Message(MSG_HELLO),
            10000, 0, 0
        );
    }
}

void e1_AKC_PMCode::myMessageFunc(std::shared_ptr<Message> msg,
                                  P2PNetworkInterface *sender) {

    switch (msg->type) {

        case MSG_HELLO:
            protocolStage = 2;
            setColor(YELLOW);

            sendMessage(
                new Message(MSG_KEY),
                sender,
                10000, 0
            );
            break;

        case MSG_KEY:
            protocolStage = 3;
            setColor(BLUE);

            sendMessage(
                new Message(MSG_ACK),
                sender,
                10000, 0
            );
            break;

        case MSG_ACK:
            protocolStage = 4;
            setColor(GREEN);
            break;
    }
}

string e1_AKC_PMCode::onInterfaceDraw() {
    return "AKC_PM\nStage: " + std::to_string(protocolStage);
}

