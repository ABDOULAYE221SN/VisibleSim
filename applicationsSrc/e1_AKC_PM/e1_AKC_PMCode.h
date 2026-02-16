#ifndef E1_AKC_PM_CODE_H
#define E1_AKC_PM_CODE_H

#include "robots/catoms3D/catoms3DBlockCode.h"


using namespace Catoms3D;

/* =======================
   TYPES DE MESSAGES AKC-PM
======================= */
struct AuthRequest {
    int n1;
    int x;
    int K0;
};

struct AuthChallenge {
    int x1;
};

/* =======================
   IDENTIFIANTS DE MESSAGES
======================= */
static const int MSG_AUTH_REQ       = 1001;
static const int MSG_AUTH_CHALLENGE = 1002;

/* =======================
   ETATS DU PROTOCOLE
======================= */
enum AuthState {
    IDLE,
    AUTH_SENT,
    AUTHENTICATED
};

class e1_AKC_PMCode : public Catoms3DBlockCode {
public:
    e1_AKC_PMCode(Catoms3DBlock *host);
    ~e1_AKC_PMCode();

   void startup();
    void myMessageFunc(std::shared_ptr<Message>,
                       P2PNetworkInterface *);

private:
    /* ===== Variables protocole ===== */
    int n0 = 0;
    int n2 = 0;
    int sharedKey = 0;
    AuthState state = IDLE;

    /* ===== Secrets pré-partagés ===== */
    int secretL[4] = {11, 23, 37, 51};

    int hash(int v);
};

#endif

