/**
 * SCupCatoms3DTestCode.cpp
 * S-CUP — variante de test avec module falsifié en (4,2,2)
 *
 * Cluster 1 (JAUNE) : iCH=(2,2,2), CM1=(2,3,2), CM2=(3,3,2)
 * Cluster 2 (BLEU)  : CH2=(3,2,2), CM3=(4,1,2), CM4=(4,3,2)
 * FAKE               : CM5=(4,2,2)
 *
 * Séquence :
 *   0. iCH  (2,2,2)→(5,3,2)    [pas d'auth]
 *   1. CH2  (3,2,2)→(5,2,2)    [auth → BLEU]
 *   2. CM1  (2,3,2)→(6,3,2)    [auth → JAUNE]
 *   3. CH2  (5,2,2)→(6,2,2)    [auth → BLEU]
 *   4. CM2  (3,3,2)→(7,3,2) et CM3 (4,1,2)→(6,1,2)  [simultané]
 *   5. CM5  (4,2,2)→(6,0,2)    [FAKE : auth echec → ROUGE → retour (3,3,2)]
 *   6. CM4  (4,3,2)→(6,0,2)    [auth → BLEU]
 */
#include "SCupCatoms3DTestCode.h"
#include "../../simulatorCore/src/spongent160.h"
#include "robots/catoms3D/catoms3DSimulator.h"
#include <algorithm>
#include <random>
#include <iomanip>
#include <sstream>

using namespace std;
using namespace Catoms3D;

// ---------------------------------------------------------------------------
// Positions initiales
// ---------------------------------------------------------------------------
static const Cell3DPosition POS_ICH (2,2,2);
static const Cell3DPosition POS_CH2 (3,2,2);
static const Cell3DPosition POS_CM1 (2,3,2);
static const Cell3DPosition POS_CM2 (3,3,2);
static const Cell3DPosition POS_CM3 (4,1,2);  // CM3 legitime (cluster 2)
static const Cell3DPosition POS_CM4 (4,3,2);
static const Cell3DPosition POS_CM5 (4,2,2);  // module falsifié

// ---------------------------------------------------------------------------
// Positions cibles
// ---------------------------------------------------------------------------
static const Cell3DPosition TARGET_ICH  (5,3,2);
static const Cell3DPosition TARGET_CH2  (5,2,2);
static const Cell3DPosition TARGET_CH2_2(6,2,2);
static const Cell3DPosition TARGET_CM1  (6,3,2);
static const Cell3DPosition TARGET_CM2  (7,3,2);
static const Cell3DPosition TARGET_CM3  (6,1,2);
static const Cell3DPosition TARGET_CM4  (6,0,2);
static const Cell3DPosition TARGET_CM5  (6,0,2);   // cible du FAKE (même que CM4)
static const Cell3DPosition TARGET_FAKE_RETURN(4,2,2); // position de départ du FAKE

// ---------------------------------------------------------------------------
// Etat global de reconfiguration
// ---------------------------------------------------------------------------
static int  nextStep             = 0;
static int  simultaneousDone     = 0; // arrivées étape 4
static int  simultaneousAuthDone = 0; // auth réussies étape 4
static bool fakeHandled          = false; // true après traitement de l'étape 5

// ---------------------------------------------------------------------------
// Helper : interface valide
// ---------------------------------------------------------------------------
static bool ifaceValideT(P2PNetworkInterface* iface, BuildingBlock* self) {
    if (!iface || !iface->connectedInterface) return false;
    if (!iface->connectedInterface->hostBlock) return false;
    if (iface->connectedInterface->hostBlock == self) return false;
    if (iface->connectedInterface->hostBlock->blockId == self->blockId) return false;
    if (iface->connectedInterface->hostBlock->position == self->position) return false;
    return true;
}

// ---------------------------------------------------------------------------
// Constructeurs messages
// ---------------------------------------------------------------------------
MessageAuthRequestT::MessageAuthRequestT(const vector<uint8_t>& _n1,
                                         const vector<uint8_t>& _x,
                                         const vector<uint8_t>& _K0)
    : Message(), n1(_n1), x(_x), K0(_K0) { type = MSG_SCUP_AUTH_REQUEST; }

MessageKeyChallengeT::MessageKeyChallengeT(const vector<uint8_t>& _x1)
    : Message(), x1(_x1) { type = MSG_SCUP_KEY_CHALLENGE; }

MessageAuthEchecT::MessageAuthEchecT() : Message() { type = MSG_SCUP_AUTH_ECHEC; }

// ---------------------------------------------------------------------------
// Constructeur / Destructeur
// ---------------------------------------------------------------------------
SCupCatoms3DTestCode::SCupCatoms3DTestCode(Catoms3DBlock* host)
    : Catoms3DBlockCode(host), module(host) {
    if (!host) return;
    addMessageEventFunc2(MSG_SCUP_AUTH_REQUEST,
        bind(&SCupCatoms3DTestCode::onAuthRequestReceived,  this,
             placeholders::_1, placeholders::_2));
    addMessageEventFunc2(MSG_SCUP_KEY_CHALLENGE,
        bind(&SCupCatoms3DTestCode::onKeyChallengeReceived, this,
             placeholders::_1, placeholders::_2));
    addMessageEventFunc2(MSG_SCUP_AUTH_ECHEC,
        [this](shared_ptr<Message>, P2PNetworkInterface* src){ onAuthEchecReceived(src); });
}
SCupCatoms3DTestCode::~SCupCatoms3DTestCode() {}

// ---------------------------------------------------------------------------
// Primitives cryptographiques
// ---------------------------------------------------------------------------
vector<uint8_t> SCupCatoms3DTestCode::H(const vector<uint8_t>& data) {
    return Spongent::Spongent160::hash(data);
}
vector<uint8_t> SCupCatoms3DTestCode::H(uint64_t val) {
    vector<uint8_t> v(8);
    for (int i = 0; i < 8; i++) v[i] = (uint8_t)((val >> (i*8)) & 0xFF);
    return H(v);
}
vector<uint8_t> SCupCatoms3DTestCode::L(int line) {
    vector<uint8_t> res(SCUP_LINE_SIZE_BITS / 8);
    uint64_t g = 0xDEADBEEFULL ^ ((uint64_t)line * 0x12345678ULL);
    for (size_t i = 0; i < res.size(); i++) {
        g = g * 1103515245ULL + 12345ULL;
        res[i] = (uint8_t)((g >> 16) & 0xFF);
    }
    return res;
}
vector<uint8_t> SCupCatoms3DTestCode::HL(const vector<uint8_t>& n) {
    uint64_t val = 0;
    for (int i = 0; i < 8 && i < (int)n.size(); i++)
        val |= ((uint64_t)n[i]) << (i*8);
    return HL(val);
}
vector<uint8_t> SCupCatoms3DTestCode::HL(uint64_t n) {
    return H(L((int)(n % SCUP_CODE_LINES)));
}
vector<uint8_t> SCupCatoms3DTestCode::xorVec(const vector<uint8_t>& a, const vector<uint8_t>& b) {
    vector<uint8_t> res(a.size());
    for (size_t i = 0; i < a.size(); i++) res[i] = a[i] ^ b[i % b.size()];
    return res;
}
vector<uint8_t> SCupCatoms3DTestCode::generateNonce160() {
    static random_device rd;
    static mt19937_64 gen(rd());
    uniform_int_distribution<uint64_t> dis;
    vector<uint8_t> n(SCUP_HASH_SIZE_BYTES);
    for (int i = 0; i < SCUP_HASH_SIZE_BYTES; i += 8) {
        uint64_t v = dis(gen);
        for (int j = 0; j < 8 && i+j < SCUP_HASH_SIZE_BYTES; j++)
            n[i+j] = (uint8_t)((v >> (j*8)) & 0xFF);
    }
    return n;
}
vector<uint8_t> SCupCatoms3DTestCode::timestampToVec(uint64_t ts) {
    uint64_t Ts = ts / SCUP_TIME_DIVISOR;
    vector<uint8_t> v(SCUP_HASH_SIZE_BYTES, 0);
    for (int i = 0; i < 8; i++) v[i] = (uint8_t)((Ts >> (i*8)) & 0xFF);
    return v;
}
string SCupCatoms3DTestCode::hexShort(const vector<uint8_t>& v, int n) const {
    ostringstream ss; ss << hex << uppercase;
    for (int i = 0; i < n && i < (int)v.size(); i++)
        ss << setw(2) << setfill('0') << (int)v[i];
    ss << "..."; return ss.str();
}
bool SCupCatoms3DTestCode::isInterfaceValid(P2PNetworkInterface* iface) const {
    return ifaceValideT(iface, module);
}
bool SCupCatoms3DTestCode::estDansStructure() const {
    return security.isICH || security.state == STATE_KEY_ESTABLISHED;
}

// ---------------------------------------------------------------------------
// Startup
// ---------------------------------------------------------------------------
void SCupCatoms3DTestCode::startup() {
    if (module->position == POS_ICH) {
        role = ROLE_ICH;
        security.isICH = true;
        security.state = STATE_KEY_ESTABLISHED;
        module->setColor(YELLOW);
        myTarget = TARGET_ICH;
        console << "[iCH] M" << module->blockId << " = Initial Cluster Head (JAUNE)\n";
        getScheduler()->schedule(
            new InterruptionEvent<unsigned int>(getScheduler()->now() + 100, module, 0));

    } else if (module->position == POS_CM5) {
        role        = ROLE_FAKE;
        isFalsified = true;
        myTarget    = TARGET_CM5;
        module->setColor(BLUE);  // Cluster 2
        console << "[FAKE] M" << module->blockId << " = module falsifie en (4,2,2) cluster 2 (BLEU)\n";

    } else if (module->position == POS_CH2) {
        role = ROLE_CH2;
        module->setColor(BLUE);
        myTarget = TARGET_CH2;
        console << "[CH2] M" << module->blockId << " = Cluster Head 2 (BLEU)\n";

    } else {
        role = ROLE_CM;
        if (module->position == POS_CM1 || module->position == POS_CM2)
            module->setColor(YELLOW);
        else
            module->setColor(BLUE);

        if      (module->position == POS_CM1) myTarget = TARGET_CM1;
        else if (module->position == POS_CM2) myTarget = TARGET_CM2;
        else if (module->position == POS_CM3) myTarget = TARGET_CM3;
        else if (module->position == POS_CM4) myTarget = TARGET_CM4;
        console << "[CM] M" << module->blockId << " en attente\n";
    }
}

// ---------------------------------------------------------------------------
// Reconfiguration séquentielle
// ---------------------------------------------------------------------------
void SCupCatoms3DTestCode::lancerReconfiguration() {
    nextStep = 0; simultaneousDone = 0; simultaneousAuthDone = 0; fakeHandled = false;
    lancerProchainMobile();
}

void SCupCatoms3DTestCode::lancerProchainMobile() {
    auto world = BaseSimulator::getWorld();

    auto lancerModule = [&](const Cell3DPosition& pos, const Cell3DPosition& target) {
        for (auto& kv : world->buildingBlocksMap) {
            if (kv.second->position == pos) {
                SCupCatoms3DTestCode* m = (SCupCatoms3DTestCode*)kv.second->blockCode;
                m->myTarget = target;
                console << "\n[Reconf etape " << nextStep << "] M" << m->module->blockId
                        << " depuis " << pos << " vers " << target << "\n";
                m->visited.clear();
                m->visited.insert(m->module->position);
                m->moveSteps = 0;
                m->tryMoveToward(target);
                return;
            }
        }
    };

    switch (nextStep) {
        case 0:  // iCH (2,2,2) → (5,3,2)
            nextStep++;
            lancerModule(POS_ICH, TARGET_ICH);
            break;
        case 1:  // CH2 (3,2,2) → (5,2,2)
            nextStep++;
            lancerModule(POS_CH2, TARGET_CH2);
            break;
        case 2:  // CM1 (2,3,2) → (6,3,2)
            nextStep++;
            lancerModule(POS_CM1, TARGET_CM1);
            break;
        case 3:  // CH2 (5,2,2) → (6,2,2)
            nextStep++;
            for (auto& kv : world->buildingBlocksMap) {
                if (kv.second->position == TARGET_CH2) {
                    SCupCatoms3DTestCode* m = (SCupCatoms3DTestCode*)kv.second->blockCode;
                    m->myTarget = TARGET_CH2_2;
                    console << "\n[Reconf etape 3] CH2 M" << m->module->blockId
                            << " depuis " << TARGET_CH2 << " vers " << TARGET_CH2_2 << "\n";
                    m->visited.clear();
                    m->visited.insert(m->module->position);
                    m->moveSteps = 0;
                    m->tryMoveToward(TARGET_CH2_2);
                    return;
                }
            }
            break;
        case 4:  // CM2 (3,3,2)→(7,3,2) ET CM3 (4,1,2)→(6,1,2) simultané
            nextStep++;
            simultaneousDone     = 0;
            simultaneousAuthDone = 0;
            lancerModule(POS_CM2, TARGET_CM2);
            lancerModule(POS_CM3, TARGET_CM3);
            break;
        case 5:  // CM5 FAKE (4,2,2) → (6,0,2) — auth échouera
            nextStep++;
            lancerModule(POS_CM5, TARGET_CM5);
            break;
        case 6:  // CM4 (4,3,2) → (6,0,2)
            nextStep++;
            lancerModule(POS_CM4, TARGET_CM4);
            break;
        default:
            console << "\n[S-CUP TEST] Reconfiguration terminee. Module falsifie rejete.\n";
            break;
    }
}

// ---------------------------------------------------------------------------
// Algorithme 1 — SF-1/SF-2 : initiation normale
// ---------------------------------------------------------------------------
void SCupCatoms3DTestCode::algorithm1_Initiate(P2PNetworkInterface* dest) {
    if (authenticatedNeighbors.count(dest) || ongoingAuthentications.count(dest)) return;

    vector<uint8_t> n0 = generateNonce160();
    vector<uint8_t> n1 = H(n0);
    vector<uint8_t> K0 = HL(n0);
    uint64_t ts = scheduler->now();
    vector<uint8_t> x  = xorVec(timestampToVec(ts), n0);
    bID destId = dest->connectedInterface->hostBlock->blockId;

    console << "\n[t=" << ts << "] *** S-CUP ALGO1 SF-1 — M" << module->blockId << " ***\n";
    console << "  n1  = H(n0)         = " << hexShort(n1) << "\n";
    console << "  K0  = HL(n0 mod Nb) = " << hexShort(K0) << "\n";
    console << "  x   = Ts XOR n0     = " << hexShort(x)  << "\n";
    console << "  >>> SF-2 : AUTH_REQUEST -> M" << destId << "\n";

    sendMessage(new MessageAuthRequestT(n1, x, K0), dest, SCUP_TRANSMISSION_DELAY, 0);
    pendingN0[dest] = n0;
    ongoingAuthentications.insert(dest);
    security.state = STATE_AUTHENTICATING;
    module->setColor(ORANGE);
}

// ---------------------------------------------------------------------------
// Algorithme 1 — SF-1/SF-2 : version FALSIFIÉE (K0 aléatoire invalide)
// ---------------------------------------------------------------------------
void SCupCatoms3DTestCode::algorithm1_InitiateFalsified(P2PNetworkInterface* dest) {
    if (authenticatedNeighbors.count(dest) || ongoingAuthentications.count(dest)) return;

    vector<uint8_t> n0      = generateNonce160();
    vector<uint8_t> n1      = H(n0);
    vector<uint8_t> K0_fake = generateNonce160();  // K0 INVALIDE — aléatoire
    uint64_t ts = scheduler->now();
    vector<uint8_t> x = xorVec(timestampToVec(ts), n0);
    bID destId = dest->connectedInterface->hostBlock->blockId;

    console << "\n[t=" << ts << "] *** [FAKE] ALGO1 SF-1 — M" << module->blockId
            << " (module falsifie) ***\n";
    console << "  n1  = " << hexShort(n1) << "\n";
    console << "  K0  = " << hexShort(K0_fake) << "  !! K0 ALEATOIRE INVALIDE !!\n";
    console << "  >>> SF-2 : AUTH_REQUEST (falsifie) -> M" << destId << "\n";

    sendMessage(new MessageAuthRequestT(n1, x, K0_fake), dest, SCUP_TRANSMISSION_DELAY, 0);
    pendingN0[dest] = n0;
    ongoingAuthentications.insert(dest);
    security.state = STATE_AUTHENTICATING;
    module->setColor(ORANGE);
}

// ---------------------------------------------------------------------------
// Algorithme 1 — SF-3/SF-4 : vérification (côté vérificateur)
// ---------------------------------------------------------------------------
void SCupCatoms3DTestCode::algorithm1_Verify(P2PNetworkInterface* src,
                                              const vector<uint8_t>& n1,
                                              const vector<uint8_t>& x,
                                              const vector<uint8_t>& K0) {
    if (authenticatedNeighbors.count(src)) return;
    if (!estDansStructure()) {
        sendMessage(new MessageAuthEchecT(), src, SCUP_TRANSMISSION_DELAY, 0);
        return;
    }

    bID srcId = src->connectedInterface->hostBlock->blockId;
    uint64_t Tr = scheduler->now();
    console << "\n[t=" << Tr << "] *** S-CUP ALGO1 SF-3 — M" << module->blockId
            << " verifie M" << srcId << " ***\n";

    // Reconstruction de n0 via tolérance de sync ±5
    uint64_t Ts_base = (Tr - SCUP_TRANSMISSION_DELAY) / SCUP_TIME_DIVISOR;
    vector<uint8_t> n0_prime;
    bool syncOK = false; int offset_ok = 0;
    for (int off = -SCUP_TIME_TOLERANCE; off <= SCUP_TIME_TOLERANCE && !syncOK; off++) {
        uint64_t Ts_test = Ts_base + (uint64_t)off;
        vector<uint8_t> Ts_vec(SCUP_HASH_SIZE_BYTES, 0);
        for (int i = 0; i < 8; i++) Ts_vec[i] = (uint8_t)((Ts_test >> (i*8)) & 0xFF);
        vector<uint8_t> n0_test = xorVec(x, Ts_vec);
        if (H(n0_test) == n1) { n0_prime = n0_test; syncOK = true; offset_ok = off; }
    }
    if (!syncOK) {
        console << "  [ECHEC] Synchronisation impossible\n";
        sendMessage(new MessageAuthEchecT(), src, SCUP_TRANSMISSION_DELAY, 0);
        return;
    }
    console << "  Sync OK (offset=" << offset_ok << ")\n";

    // Vérification K0 — échouera si module falsifié
    if (HL(n0_prime) != K0) {
        console << "  [ECHEC] K0 INVALIDE — code source non conforme !\n";
        console << "  >>> AUTH_ECHEC envoye a M" << srcId << " (module rejete)\n";
        sendMessage(new MessageAuthEchecT(), src, SCUP_TRANSMISSION_DELAY, 0);
        return;
    }
    console << "  K0 valide\n";

    if (security.n2.empty()) {
        security.n2 = generateNonce160();
        console << "  n2 genere = " << hexShort(security.n2) << "\n";
    }

    vector<uint8_t> x1 = xorVec(security.n2, n0_prime);
    vector<uint8_t> K1 = HL(security.n2);
    neighborKeys[src]           = K1;
    authenticatedNeighbors[src] = true;
    security.liens++;
    console << "  >>> SF-4 : KEY_CHALLENGE -> M" << srcId << "\n";
    sendMessage(new MessageKeyChallengeT(x1), src, SCUP_TRANSMISSION_DELAY, 0);
}

// ---------------------------------------------------------------------------
// Algorithme 1 — SF-5 : complétion (côté initiateur)
// ---------------------------------------------------------------------------
void SCupCatoms3DTestCode::algorithm1_Complete(P2PNetworkInterface* src,
                                                const vector<uint8_t>& x1) {
    if (authenticatedNeighbors.count(src)) return;
    if (!pendingN0.count(src)) return;

    bID srcId = src->connectedInterface->hostBlock->blockId;
    vector<uint8_t> n0 = pendingN0[src];
    vector<uint8_t> n2 = xorVec(x1, n0);
    vector<uint8_t> K1 = HL(n2);

    console << "\n[t=" << scheduler->now() << "] *** S-CUP ALGO1 SF-5 — M"
            << module->blockId << " cle etablie avec M" << srcId << " ***\n";
    console << "  K1 = " << hexShort(K1) << " ==> AUTHENTIFICATION REUSSIE\n";

    security.n2 = n2; security.K1 = K1;
    neighborKeys[src]           = K1;
    authenticatedNeighbors[src] = true;
    security.state              = STATE_KEY_ESTABLISHED;
    security.liens++;
    pendingN0.erase(src);
    ongoingAuthentications.erase(src);

    // Couleur selon cluster
    bool isCluster2 = (role == ROLE_CH2
                       || myTarget == TARGET_CM3
                       || myTarget == TARGET_CM4);
    module->setColor(isCluster2 ? BLUE : YELLOW);
    console << "  Module M" << module->blockId << " -> "
            << (isCluster2 ? "BLEU (cluster 2)" : "JAUNE (cluster 1)") << "\n";

    // Etape 4 simultanée : attendre les 2 auth avant de continuer
    if (myTarget == TARGET_CM2 || myTarget == TARGET_CM3) {
        simultaneousAuthDone++;
        if (simultaneousAuthDone >= 2) lancerProchainMobile();
    } else {
        lancerProchainMobile();
    }
}

// ---------------------------------------------------------------------------
// Gestionnaires de messages
// ---------------------------------------------------------------------------
void SCupCatoms3DTestCode::onAuthRequestReceived(shared_ptr<Message> msg,
                                                  P2PNetworkInterface* src) {
    auto* m = static_cast<MessageAuthRequestT*>(msg.get());
    algorithm1_Verify(src, m->n1, m->x, m->K0);
}
void SCupCatoms3DTestCode::onKeyChallengeReceived(shared_ptr<Message> msg,
                                                   P2PNetworkInterface* src) {
    auto* m = static_cast<MessageKeyChallengeT*>(msg.get());
    if (pendingN0.count(src))
        algorithm1_Complete(src, m->x1);
}
void SCupCatoms3DTestCode::onAuthEchecReceived(P2PNetworkInterface* src) {
    bID srcId = src->connectedInterface ? src->connectedInterface->hostBlock->blockId : 0;
    console << "\n[t=" << scheduler->now() << "] AUTH_ECHEC recu par M"
            << module->blockId << " de M" << srcId << "\n";
    ongoingAuthentications.erase(src);
    pendingN0.erase(src);

    if (isFalsified) {
        console << "  [FAKE] Code source NON CONFORME — module REJETE\n";
        console << "  [FAKE] M" << module->blockId
                << " passe en ROUGE et retourne vers sa position de depart " << TARGET_FAKE_RETURN << "\n";
        module->setColor(RED);
        isReturning = true;
        fakeHandled = true;
        visited.clear();
        visited.insert(module->position);
        moveSteps = 0;
        tryMoveToward(TARGET_FAKE_RETURN);
    } else {
        module->setColor(RED);
        console << "  Authentification refusee — M" << module->blockId << " passe en rouge\n";
    }
}

// ---------------------------------------------------------------------------
// Boucle d'événements
// ---------------------------------------------------------------------------
void SCupCatoms3DTestCode::processLocalEvent(EventPtr pev) {
    Catoms3DBlockCode::processLocalEvent(pev);
    switch (pev->eventType) {
        case EVENT_INTERRUPTION: {
            auto id = static_pointer_cast<InterruptionEvent<unsigned int>>(pev)->data;
            if (id == 0 && security.isICH) lancerReconfiguration();
            break;
        }
        default: break;
    }
}

// ---------------------------------------------------------------------------
// Déplacement glouton
// ---------------------------------------------------------------------------
bool SCupCatoms3DTestCode::tryMoveToward(const Cell3DPosition& goal) {
    if (module->position == goal) return false;
    if (moveSteps >= SCUP_MAX_STEPS) {
        console << "[WARN] M" << module->blockId << " MAX_STEPS atteint\n";
        return false;
    }
    auto tab = Catoms3DMotionEngine::getAllRotationsForModule(module);
    Cell3DPosition fp; short fo;

    struct Candidate { Cell3DPosition pos; double dist; bool vis; };
    vector<Candidate> cands;

    for (auto& elem : tab) {
        elem.second.init(((Catoms3DGlBlock*)module->ptrGlBlock)->mat);
        elem.second.getFinalPositionAndOrientation(fp, fo);
        if (!lattice->isFree(fp)) continue;
        int nbVoisins = 0;
        for (auto& np : lattice->getNeighborhood(fp))
            if (lattice->cellHasBlock(np) && np != module->position) nbVoisins++;
        // Le module falsifié en retraite peut se déconnecter
        if (!isFalsified && nbVoisins < 1) continue;
        double d = (double)((fp[0]-goal[0])*(fp[0]-goal[0])
                          + (fp[1]-goal[1])*(fp[1]-goal[1])
                          + (fp[2]-goal[2])*(fp[2]-goal[2]));
        cands.push_back({fp, d, visited.count(fp) > 0});
    }
    if (cands.empty()) return false;

    Cell3DPosition best; double bestDist = 1e9; bool found = false;
    for (auto& c : cands)
        if (!c.vis && c.dist < bestDist) { bestDist = c.dist; best = c.pos; found = true; }
    if (!found) {
        visited.clear(); visited.insert(module->position); bestDist = 1e9;
        for (auto& c : cands)
            if (c.dist < bestDist) { bestDist = c.dist; best = c.pos; found = true; }
    }
    if (found) {
        visited.insert(best); moveSteps++;
        bool moved = ((Catoms3DBlock*)module)->moveTo(best);
        if (!moved) moveSteps--;
        return moved;
    }
    return false;
}

// ---------------------------------------------------------------------------
// Fin de déplacement
// ---------------------------------------------------------------------------
void SCupCatoms3DTestCode::onMotionEnd() {

    // --- Module falsifié en retraite ---
    if (isReturning) {
        if (module->position == TARGET_FAKE_RETURN) {
            console << "[FAKE] M" << module->blockId
                    << " retourne a " << TARGET_FAKE_RETURN << " (hors structure) — reste en ROUGE\n";
            module->setColor(RED);
            isReturning = false;
            // La séquence légitime continue : étape 6 (CM4)
            auto world = BaseSimulator::getWorld();
            for (auto& kv : world->buildingBlocksMap) {
                SCupCatoms3DTestCode* m = (SCupCatoms3DTestCode*)kv.second->blockCode;
                if (m->security.isICH) { m->lancerProchainMobile(); return; }
            }
        } else {
            if (!tryMoveToward(TARGET_FAKE_RETURN)) {
                visited.clear(); visited.insert(module->position);
                moveSteps = 0; tryMoveToward(TARGET_FAKE_RETURN);
            }
        }
        return;
    }

    if (module->position == myTarget) {
        // Purger interfaces déconnectées
        for (auto it = authenticatedNeighbors.begin(); it != authenticatedNeighbors.end(); )
            if (!it->first->connectedInterface) it = authenticatedNeighbors.erase(it); else ++it;
        for (auto it = neighborKeys.begin(); it != neighborKeys.end(); )
            if (!it->first->connectedInterface) it = neighborKeys.erase(it); else ++it;
        ongoingAuthentications.clear(); pendingN0.clear();

        console << "[S-CUP] M" << module->blockId << " arrive a " << myTarget << "\n";

        // --- iCH arrivé : lance directement l'étape suivante ---
        if (myTarget == TARGET_ICH) {
            lancerProchainMobile();
            return;
        }

        // --- Etape 4 simultanée : CM2 et CM3 ---
        if (myTarget == TARGET_CM2 || myTarget == TARGET_CM3) {
            simultaneousDone++;
            for (int i = 0; i < 12; i++) {
                P2PNetworkInterface* iface = module->getInterface(i);
                if (!isInterfaceValid(iface)) continue;
                SCupCatoms3DTestCode* vc =
                    (SCupCatoms3DTestCode*)iface->connectedInterface->hostBlock->blockCode;
                if (vc->estDansStructure()) { algorithm1_Initiate(iface); return; }
            }
            if (simultaneousDone >= 2) lancerProchainMobile();
            return;
        }

        // --- Etape 5 : CM5 FAKE arrive à sa cible → tente auth falsifiée ---
        if (myTarget == TARGET_CM5 && isFalsified) {
            console << "\n[S-CUP TEST] CM5 FAKE arrive a " << TARGET_CM5
                    << " — tentative d'authentification falsifiee\n";
            for (int i = 0; i < 12; i++) {
                P2PNetworkInterface* iface = module->getInterface(i);
                if (!isInterfaceValid(iface)) continue;
                SCupCatoms3DTestCode* vc =
                    (SCupCatoms3DTestCode*)iface->connectedInterface->hostBlock->blockCode;
                if (vc->estDansStructure()) {
                    algorithm1_InitiateFalsified(iface);
                    return;
                }
            }
            // Aucun voisin dans la structure : rejeter directement
            console << "  [FAKE] Aucun voisin dans la structure — module rejete\n";
            module->setColor(RED);
            isReturning = true;
            fakeHandled = true;
            visited.clear(); visited.insert(module->position);
            moveSteps = 0;
            tryMoveToward(TARGET_FAKE_RETURN);
            return;
        }

        // --- Tous les autres modules légitimes : s'authentifier ---
        for (int i = 0; i < 12; i++) {
            P2PNetworkInterface* iface = module->getInterface(i);
            if (!isInterfaceValid(iface)) continue;
            SCupCatoms3DTestCode* vc =
                (SCupCatoms3DTestCode*)iface->connectedInterface->hostBlock->blockCode;
            if (vc->estDansStructure()) {
                console << "[S-CUP] M" << module->blockId
                        << " initie AUTH vers M"
                        << iface->connectedInterface->hostBlock->blockId << "\n";
                algorithm1_Initiate(iface);
                return;
            }
        }
        console << "[WARN] M" << module->blockId << " : aucun voisin dans la structure\n";
        lancerProchainMobile();

    } else {
        if (!tryMoveToward(myTarget)) {
            visited.clear(); visited.insert(module->position);
            moveSteps = 0; tryMoveToward(myTarget);
        }
    }
}

void SCupCatoms3DTestCode::parseUserBlockElements(TiXmlElement* /*config*/) {}
