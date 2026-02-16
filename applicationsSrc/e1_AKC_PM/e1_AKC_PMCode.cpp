#include "e1_AKC_PMCode.h"
#include <cstdlib>

/* =======================
   Constructeur
======================= */
e1_AKC_PMCode::e1_AKC_PMCode(Catoms3DBlock *host)
    : Catoms3DBlockCode(host) {
}

/* =======================
   Hash simple (simulation)
======================= */
int e1_AKC_PMCode::hash(int v) {
    return (v * 31 + 17) % 100000;
}

/* =======================
   STARTUP
======================= */
void e1_AKC_PMCode::startup() {

    /* Module ancien */
    if (getId() == 1) {
        setColor(RED);
        return;
    }

    /* Nouveau module */
    setColor(YELLOW);

    n0 = rand() % 1000;
    int Ts = scheduler->now() / 10000;

    AuthRequest req;
    req.n1 = hash(n0);
    req.x  = Ts ^ n0;
    req.K0 = hash(secretL[n0 % 4]);

    sendMessageToAllNeighbors(
        "AKC_REQ",
        new MessageOf<AuthRequest>(MSG_AUTH_REQ, req),
        10000, 0, 0
    );

    state = AUTH_SENT;
}

/* =======================
   RECEPTION DES MESSAGES
======================= */
void e1_AKC_PMCode::myMessageFunc(std::shared_ptr<Message> _msg,
                                 P2PNetworkInterface *sender) {

    /* ===== Réception AUTH_REQ (ancien module) ===== */
    if (_msg->type == MSG_AUTH_REQ && getId() == 1) {

        auto *msg =
            static_cast<MessageOf<AuthRequest>*>(_msg.get());
        AuthRequest req = *msg->getData();

        int Tr = scheduler->now() / 10000;
        int n0p = req.x ^ Tr;

        if (hash(n0p) != req.n1) return;
        if (hash(secretL[n0p % 4]) != req.K0) return;

        n2 = rand() % 1000;

        AuthChallenge chal;
        chal.x1 = n2 ^ n0p;

        sendMessage(
            new MessageOf<AuthChallenge>(
                MSG_AUTH_CHALLENGE, chal),
            sender,
            10000, 0
        );

        setColor(BLUE);
    }

    /* ===== Réception AUTH_CHALLENGE (nouveau module) ===== */
    if (_msg->type == MSG_AUTH_CHALLENGE &&
        state == AUTH_SENT) {

        auto *msg =
            static_cast<MessageOf<AuthChallenge>*>(_msg.get());
        AuthChallenge chal = *msg->getData();

        n2 = chal.x1 ^ n0;
        sharedKey = hash(secretL[n2 % 4]);

        state = AUTHENTICATED;
        setColor(GREEN);

        console << "[AKC-PM] SUCCESS | Module "
                << getId()
                << " | Shared key = "
                << sharedKey << "\n";
    }
}

