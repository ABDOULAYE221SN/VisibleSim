/**
 * MonApplicationTestCode.cpp
 *
 * Variante de MonApplication avec un module falsifié en (3,3,2) :
 * - S'intercale après 3 modules légitimes (POS_A, POS_D, POS_E)
 * - Tente de rejoindre la structure avec un K0 invalide
 * - Reçoit AUTH_ECHEC, retourne à (3,1,2) en rouge
 * - La séquence reprend ensuite normalement
 */
#include "MonApplicationTestCode.hpp"
#include "../../simulatorCore/src/spongent160.h"
#include <algorithm>
#include <random>
#include <iomanip>
#include <sstream>
#include <fstream>
#include <sys/stat.h>
#include <unistd.h>

// ---------------------------------------------------------------------------
// Lecture réelle du binaire en mémoire pour L(i)
// ---------------------------------------------------------------------------
static constexpr int BYTES_PER_LINE_T = 256 / 8;  // 32 octets

static const std::vector<uint8_t>& getBinaryCodeSegmentTest() {
    static std::vector<uint8_t> cache;
    if (!cache.empty()) return cache;

    std::string path;
#if defined(__linux__)
    char buf[4096] = {};
    ssize_t len = ::readlink("/proc/self/exe", buf, sizeof(buf) - 1);
    if (len > 0) path = std::string(buf, len);

    if (path.empty()) { cache.assign(NB_LIGNES_CODE * BYTES_PER_LINE_T, 0); return cache; }

    std::ifstream f(path, std::ios::binary);
    if (!f)        { cache.assign(NB_LIGNES_CODE * BYTES_PER_LINE_T, 0); return cache; }

    f.seekg(0, std::ios::end);
    std::streamsize fileSize = f.tellg();
    f.seekg(0, std::ios::beg);
    std::streamsize needed = NB_LIGNES_CODE * BYTES_PER_LINE_T;
    std::streamsize toRead = std::min(needed, fileSize);
    cache.resize(needed, 0);
    f.read(reinterpret_cast<char*>(cache.data()), toRead);
    return cache;
#endif
    cache.assign(NB_LIGNES_CODE * BYTES_PER_LINE_T, 0);
    return cache;
}

using namespace std;

// --- Positions initiales ---
static const Cell3DPosition POS_A(2,3,2);
static const Cell3DPosition POS_C(4,3,2);   // module initial (iM)
static const Cell3DPosition POS_D(2,2,2);
static const Cell3DPosition POS_E(3,2,2);
static const Cell3DPosition POS_F(4,2,2);
static const Cell3DPosition POS_FAKE(3,3,2);
static const Cell3DPosition POS_B(4,1,2);

// --- Positions cibles ---
static const Cell3DPosition TARGET_A   (5,3,2);
static const Cell3DPosition TARGET_D   (6,3,2);
static const Cell3DPosition TARGET_E   (6,2,2);
static const Cell3DPosition TARGET_F   (6,0,2);
static const Cell3DPosition TARGET_IM  (7,3,2);
static const Cell3DPosition TARGET_FAKE(6,1,2);
static const Cell3DPosition TARGET_B   (6,1,2);
static const Cell3DPosition POS_RETURN_FAKE(3,1,2);  // retour du module falsifié après échec

// Ordre de déplacement : A, D, E, B, F, puis iM en dernier
static const Cell3DPosition MOVE_ORDER[] = { POS_A, POS_D, POS_E, POS_B, POS_F, POS_C };
static const int NB_MOVERS  = 6;
static const int FAKE_AFTER = 3;  // le fake s'intercale après 3 modules légitimes

static bool reconfigStarted = false;
static int  nextMoverIndex  = 0;
static bool fakeEnCours     = false;
static bool fakeDone        = false;

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------
static string hexShort(const vector<uint8_t>& v, int n = 4) {
    ostringstream ss;
    ss << hex << uppercase;
    for (int i = 0; i < n && i < (int)v.size(); i++)
        ss << setw(2) << setfill('0') << (int)v[i];
    ss << "...";
    return ss.str();
}

static bool ifaceValide(P2PNetworkInterface* iface, BuildingBlock* self) {
    if (!iface || !iface->connectedInterface) return false;
    if (!iface->connectedInterface->hostBlock) return false;
    if (iface->connectedInterface->hostBlock == self) return false;
    if (iface->connectedInterface->hostBlock->blockId == self->blockId) return false;
    if (iface->connectedInterface->hostBlock->position == self->position) return false;
    return true;
}

// ---------------------------------------------------------------------------
// Constructeur / Destructeur
// ---------------------------------------------------------------------------
MonApplicationTestCode::MonApplicationTestCode(Catoms3DBlock *host)
    : Catoms3DBlockCode(host), module(host) {}
MonApplicationTestCode::~MonApplicationTestCode() {}

// ---------------------------------------------------------------------------
// Fonctions cryptographiques
// ---------------------------------------------------------------------------
vector<uint8_t> MonApplicationTestCode::H(const vector<uint8_t>& data) {
    return Spongent::Spongent160::hash(data);
}
vector<uint8_t> MonApplicationTestCode::H(uint64_t val) {
    vector<uint8_t> v(8);
    for (int i = 0; i < 8; i++) v[i] = (uint8_t)((val >> (i*8)) & 0xFF);
    return H(v);
}
vector<uint8_t> MonApplicationTestCode::L(int ligne) {
    const auto& seg = getBinaryCodeSegmentTest();
    int idx = ((ligne % NB_LIGNES_CODE) + NB_LIGNES_CODE) % NB_LIGNES_CODE;
    int offset = idx * BYTES_PER_LINE_T;
    return vector<uint8_t>(seg.begin() + offset, seg.begin() + offset + BYTES_PER_LINE_T);
}
vector<uint8_t> MonApplicationTestCode::HL(const vector<uint8_t>& n) {
    uint64_t val = 0;
    for (int i = 0; i < 8 && i < (int)n.size(); i++)
        val |= ((uint64_t)n[i]) << (i*8);
    return HL(val);
}
vector<uint8_t> MonApplicationTestCode::HL(uint64_t n) {
    return H(L((int)(n % NB_LIGNES_CODE)));
}
vector<uint8_t> MonApplicationTestCode::xorVec(const vector<uint8_t>& a, const vector<uint8_t>& b) {
    vector<uint8_t> res(a.size());
    for (size_t i = 0; i < a.size(); i++) res[i] = a[i] ^ b[i % b.size()];
    return res;
}
vector<uint8_t> MonApplicationTestCode::genererNonce160() {
    static random_device rd;
    static mt19937_64 gen(rd());
    uniform_int_distribution<uint64_t> dis;
    vector<uint8_t> n(HASH_OUTPUT_BYTES);
    for (int i = 0; i < HASH_OUTPUT_BYTES; i += 8) {
        uint64_t v = dis(gen);
        for (int j = 0; j < 8 && i+j < HASH_OUTPUT_BYTES; j++)
            n[i+j] = (uint8_t)((v >> (j*8)) & 0xFF);
    }
    return n;
}
vector<uint8_t> MonApplicationTestCode::tsVersVec(uint64_t ts) {
    uint64_t Ts = ts / DIVISEUR_SYNC;
    vector<uint8_t> v(HASH_OUTPUT_BYTES, 0);
    for (int i = 0; i < 8; i++) v[i] = (uint8_t)((Ts >> (i*8)) & 0xFF);
    return v;
}

bool MonApplicationTestCode::estDansStructure() const {
    return infoSecurite.estModuleInitial ||
           infoSecurite.etat == ETAT_AUTHENTIFIE ||
           infoSecurite.etat == ETAT_CLE_ETABLIE;
}

void MonApplicationTestCode::definirCommeModuleInitial() {
    infoSecurite.estModuleInitial = true;
    infoSecurite.etat             = ETAT_AUTHENTIFIE;
    infoSecurite.liensStructure   = 0;
    module->setColor(BLUE);
    console << "[iM] Module " << module->blockId << " désigné module initial (BLEU)\n";
}

// ---------------------------------------------------------------------------
// startup
// ---------------------------------------------------------------------------
void MonApplicationTestCode::startup() {
    isReturning = false;
    authFailed  = false;
    myHome      = module->position;

    if (module->position == POS_C) {
        definirCommeModuleInitial();
        getScheduler()->schedule(
            new InterruptionEvent<unsigned int>(getScheduler()->now() + 100, module, 0));
        myTarget = TARGET_IM;
    } else if (module->position == POS_FAKE) {
        isFalsified = true;
        myTarget    = TARGET_FAKE;
        module->setColor(GREY);
        console << "[t=" << getScheduler()->now() << "] M" << module->blockId
                << " (module falsifié) en attente (GRIS)\n";
    } else {
        module->setColor(GREY);
        console << "[t=" << getScheduler()->now() << "] M" << module->blockId
                << " en attente (GRIS)\n";
        if      (module->position == POS_A) myTarget = TARGET_A;
        else if (module->position == POS_B) myTarget = TARGET_B;
        else if (module->position == POS_D) myTarget = TARGET_D;
        else if (module->position == POS_E) myTarget = TARGET_E;
        else if (module->position == POS_F) myTarget = TARGET_F;
    }
}

// ---------------------------------------------------------------------------
// Algorithme 1 — SF-1/SF-2 : authentification normale
// ---------------------------------------------------------------------------
void MonApplicationTestCode::algorithme1_Initier(P2PNetworkInterface* dest) {
    if (voisinsAuthentifies.count(dest) || authentificationsEnCours.count(dest)) return;

    infoSecurite.n0 = genererNonce160();
    infoSecurite.n1 = H(infoSecurite.n0);
    infoSecurite.K0 = HL(infoSecurite.n0);

    uint64_t ts = getScheduler()->now();
    vector<uint8_t> x = xorVec(tsVersVec(ts), infoSecurite.n0);
    bID destId = dest->connectedInterface->hostBlock->blockId;

    console << "\n[t=" << ts << "] *** ALGO1 SF-1 — M" << module->blockId << " (nM) ***\n";
    console << "  n0  = " << hexShort(infoSecurite.n0) << " (nonce secret)\n";
    console << "  n1  = H(n0)         = " << hexShort(infoSecurite.n1) << "\n";
    console << "  K0  = HL(n0 mod Nb) = " << hexShort(infoSecurite.K0) << " (preuve de code)\n";
    console << "  Ts  = " << (ts / DIVISEUR_SYNC) << "\n";
    console << "  x   = Ts XOR n0     = " << hexShort(x) << "\n";
    console << "  >>> SF-2 : M" << module->blockId << " envoie AUTH_REQUEST à M" << destId << "\n";

    sendMessage(new MessageDemandeAuth(infoSecurite.liensStructure,
                                       infoSecurite.n1, x, infoSecurite.K0),
                dest, DELAI_TRANSMISSION, 0);
    n0EnAttente[dest] = infoSecurite.n0;
    authentificationsEnCours.insert(dest);
    infoSecurite.etat = ETAT_AUTHENTIFICATION;
    module->setColor(ORANGE);
}

// ---------------------------------------------------------------------------
// Algorithme 1 — SF-1/SF-2 : version falsifiée (K0 invalide)
// ---------------------------------------------------------------------------
void MonApplicationTestCode::algorithme1_InitierFalsifie(P2PNetworkInterface* dest) {
    if (voisinsAuthentifies.count(dest) || authentificationsEnCours.count(dest)) return;

    infoSecurite.n0 = genererNonce160();
    infoSecurite.n1 = H(infoSecurite.n0);
    infoSecurite.K0 = genererNonce160();  // K0 FAUX

    uint64_t ts = getScheduler()->now();
    vector<uint8_t> x = xorVec(tsVersVec(ts), infoSecurite.n0);
    bID destId = dest->connectedInterface->hostBlock->blockId;

    console << "\n[t=" << ts << "] *** ALGO1 SF-1 — M" << module->blockId << " (nM FALSIFIÉ) ***\n";
    console << "  n0  = " << hexShort(infoSecurite.n0) << " (nonce secret)\n";
    console << "  n1  = H(n0)         = " << hexShort(infoSecurite.n1) << "\n";
    console << "  K0  = ALÉATOIRE     = " << hexShort(infoSecurite.K0) << " !! K0 FAUX !!\n";
    console << "  Ts  = " << (ts / DIVISEUR_SYNC) << "\n";
    console << "  x   = Ts XOR n0     = " << hexShort(x) << "\n";
    console << "  >>> SF-2 : M" << module->blockId << " envoie AUTH_REQUEST à M" << destId << " [K0 INVALIDE]\n";

    sendMessage(new MessageDemandeAuth(infoSecurite.liensStructure,
                                       infoSecurite.n1, x, infoSecurite.K0),
                dest, DELAI_TRANSMISSION, 0);
    n0EnAttente[dest] = infoSecurite.n0;
    authentificationsEnCours.insert(dest);
    infoSecurite.etat = ETAT_AUTHENTIFICATION;
    module->setColor(ORANGE);
}

// ---------------------------------------------------------------------------
// Algorithme 1 — SF-3/SF-4 : vérification
// ---------------------------------------------------------------------------
void MonApplicationTestCode::algorithme1_Verifier(P2PNetworkInterface* src, int liens,
                                                   const vector<uint8_t>& n1,
                                                   const vector<uint8_t>& x,
                                                   const vector<uint8_t>& K0) {
    if (voisinsAuthentifies.count(src)) return;

    // Si le demandeur est déjà dans la structure, il doit utiliser l'Algo 2
    if (liens > 0 && !infoSecurite.estModuleInitial && estDansStructure()) {
        console << "  [INFO] M" << src->connectedInterface->hostBlock->blockId
                << " est dans la structure (liens=" << liens
                << ") — doit utiliser Algo2\n";
        return;
    }

    if (!estDansStructure()) {
        sendMessage(new MessageAuthEchec(), src, DELAI_TRANSMISSION, 0);
        return;
    }

    bID srcId = src->connectedInterface->hostBlock->blockId;
    uint64_t Tr = getScheduler()->now();

    console << "\n[t=" << Tr << "] *** ALGO1 SF-3 — M" << module->blockId
            << " (vérificateur) reçoit AUTH_REQUEST de M" << srcId << " ***\n";
    console << "  MSG reçu : n1=" << hexShort(n1)
            << ", x=" << hexShort(x) << ", K0=" << hexShort(K0) << "\n";

    uint64_t Ts_base = (Tr - DELAI_TRANSMISSION) / DIVISEUR_SYNC;
    vector<uint8_t> n0_prime;
    bool syncOK = false;
    int  offset_ok = 0;

    for (int off = -TOLERANCE_SYNC; off <= TOLERANCE_SYNC && !syncOK; off++) {
        uint64_t Ts_test = Ts_base + (uint64_t)off;
        vector<uint8_t> Ts_vec(HASH_OUTPUT_BYTES, 0);
        for (int i = 0; i < 8; i++)
            Ts_vec[i] = (uint8_t)((Ts_test >> (i*8)) & 0xFF);
        vector<uint8_t> n0_test = xorVec(x, Ts_vec);
        if (H(n0_test) == n1) { n0_prime = n0_test; syncOK = true; offset_ok = off; }
    }

    if (!syncOK) {
        console << "  [ECHEC] Synchronisation impossible\n";
        console << "  >>> AUTH_ECHEC envoyé à M" << srcId << "\n";
        sendMessage(new MessageAuthEchec(), src, DELAI_TRANSMISSION, 0);
        return;
    }
    console << "  Sync OK (offset=" << offset_ok << ") : n0' = " << hexShort(n0_prime) << "\n";

    if (HL(n0_prime) != K0) {
        console << "  [ECHEC] K0 invalide — HL(n0')=" << hexShort(HL(n0_prime))
                << " != K0=" << hexShort(K0) << "\n";
        console << "  >>> AUTH_ECHEC envoyé à M" << srcId << "\n";
        sendMessage(new MessageAuthEchec(), src, DELAI_TRANSMISSION, 0);
        return;
    }
    console << "  K0 valide : même code confirmé\n";

    if (infoSecurite.n2.empty()) {
        infoSecurite.n2 = genererNonce160();
        console << "  n2 généré = " << hexShort(infoSecurite.n2) << " (SINGLE_KEY)\n";
    } else {
        console << "  n2 existant = " << hexShort(infoSecurite.n2) << " (SINGLE_KEY partagé)\n";
    }

    vector<uint8_t> x1 = xorVec(infoSecurite.n2, n0_prime);
    vector<uint8_t> K1 = HL(infoSecurite.n2);

    clesVoisins[src]         = K1;
    voisinsAuthentifies[src] = true;
    infoSecurite.liensStructure++;

    console << "  K1 = HL(n2 mod Nb) = " << hexShort(K1) << "\n";
    console << "  x1 = n2 XOR n0'    = " << hexShort(x1) << "\n";
    console << "  >>> SF-4 : M" << module->blockId
            << " envoie KEY_CHALLENGE à M" << srcId << "\n";

    sendMessage(new MessageDefiCle(x1), src, DELAI_TRANSMISSION, 0);
}

// ---------------------------------------------------------------------------
// Algorithme 1 — SF-5 : complétion côté nM
// ---------------------------------------------------------------------------
void MonApplicationTestCode::algorithme1_Completer(P2PNetworkInterface* src,
                                                    const vector<uint8_t>& x1) {
    if (voisinsAuthentifies.count(src)) return;
    if (!n0EnAttente.count(src)) return;

    bID srcId = src->connectedInterface->hostBlock->blockId;
    uint64_t t = getScheduler()->now();

    vector<uint8_t> n0 = n0EnAttente[src];
    vector<uint8_t> n2 = xorVec(x1, n0);
    vector<uint8_t> K1 = HL(n2);

    console << "\n[t=" << t << "] *** ALGO1 SF-5 — M" << module->blockId
            << " reçoit KEY_CHALLENGE de M" << srcId << " ***\n";
    console << "  x1 reçu = " << hexShort(x1) << "\n";
    console << "  n2 = x1 XOR n0     = " << hexShort(n2) << "\n";
    console << "  K1 = HL(n2 mod Nb) = " << hexShort(K1) << "\n";
    console << "  ==> AUTHENTIFICATION RÉUSSIE — M" << module->blockId << " devient VERT\n";

    infoSecurite.n2          = n2;
    infoSecurite.K1          = K1;
    clesVoisins[src]         = K1;
    voisinsAuthentifies[src] = true;
    infoSecurite.etat        = ETAT_CLE_ETABLIE;
    infoSecurite.liensStructure++;
    n0EnAttente.erase(src);
    authentificationsEnCours.erase(src);

    module->setColor(GREEN);
    lancerProchainMobile();
}

// ---------------------------------------------------------------------------
// Reconfiguration
// ---------------------------------------------------------------------------
void MonApplicationTestCode::lancerReconfiguration() {
    reconfigStarted = true;
    nextMoverIndex  = 0;
    fakeEnCours     = false;
    fakeDone        = false;
    lancerProchainMobile();
}

// Retourne la cible associée à une position de départ
static Cell3DPosition cibleDe(const Cell3DPosition& pos) {
    if (pos == POS_A) return TARGET_A;
    if (pos == POS_B) return TARGET_B;
    if (pos == POS_C) return TARGET_IM;
    if (pos == POS_D) return TARGET_D;
    if (pos == POS_E) return TARGET_E;
    if (pos == POS_F) return TARGET_F;
    return pos;
}

void MonApplicationTestCode::lancerProchainMobile() {
    // Vérifie si le module d'index précédent est bien arrivé à sa cible
    auto precedentArrive = [](int index) -> bool {
        if (index <= 0) return true;
        Cell3DPosition prevTarget = cibleDe(MOVE_ORDER[index - 1]);
        auto world = BaseSimulator::getWorld();
        for (auto& kv : world->buildingBlocksMap) {
            MonApplicationTestCode* m = (MonApplicationTestCode*)kv.second->blockCode;
            if (!m->isFalsified && m->myTarget == prevTarget
                && m->module->position == prevTarget)
                return true;
        }
        return false;
    };

    // Intercaler le module falsifié après FAKE_AFTER modules légitimes
    if (nextMoverIndex == FAKE_AFTER && !fakeEnCours && !fakeDone) {
        if (!precedentArrive(FAKE_AFTER)) return;

        fakeEnCours = true;
        auto world = BaseSimulator::getWorld();
        for (auto& kv : world->buildingBlocksMap) {
            MonApplicationTestCode* m = (MonApplicationTestCode*)kv.second->blockCode;
            if (m->isFalsified && m->module->position == POS_FAKE) {
                console << "\n[Reconf] Lancement MODULE FALSIFIÉ M" << m->module->blockId
                        << " depuis " << POS_FAKE << " vers " << TARGET_FAKE << "\n";
                m->visited.clear();
                m->visited.insert(m->module->position);
                m->moveSteps = 0;
                m->tryMoveToward(TARGET_FAKE);
                return;
            }
        }
        // Module falsifié introuvable, continuer
        fakeEnCours = false;
        fakeDone    = true;
    }

    if (fakeEnCours) return;

    if (nextMoverIndex < NB_MOVERS) {
        if (!precedentArrive(nextMoverIndex)) return;

        Cell3DPosition targetCherchee = cibleDe(MOVE_ORDER[nextMoverIndex]);
        nextMoverIndex++;

        auto world = BaseSimulator::getWorld();
        for (auto& kv : world->buildingBlocksMap) {
            MonApplicationTestCode* m = (MonApplicationTestCode*)kv.second->blockCode;
            if (!m->isFalsified && m->myTarget == targetCherchee
                && m->module->position != targetCherchee) {
                console << "\n[Reconf] Lancement M" << m->module->blockId
                        << " depuis " << m->module->position
                        << " vers " << targetCherchee << "\n";
                m->visited.clear();
                m->visited.insert(m->module->position);
                m->moveSteps = 0;
                m->tryMoveToward(targetCherchee);
                return;
            }
        }
        lancerProchainMobile();  // module déjà arrivé, passer au suivant
    }
}

// ---------------------------------------------------------------------------
// Réception des messages
// ---------------------------------------------------------------------------
void MonApplicationTestCode::surReceptionDemandeAuth(shared_ptr<Message> msg,
                                                      P2PNetworkInterface* src) {
    auto* m = static_cast<MessageDemandeAuth*>(msg.get());
    algorithme1_Verifier(src, m->liens, m->n1, m->x, m->K0);
}

void MonApplicationTestCode::surReceptionDefiCle(shared_ptr<Message> msg,
                                                   P2PNetworkInterface* src) {
    auto* m = static_cast<MessageDefiCle*>(msg.get());
    if (n0EnAttente.count(src))
        algorithme1_Completer(src, m->x1);
}

void MonApplicationTestCode::surReceptionAuthEchec(P2PNetworkInterface* src) {
    bID srcId = src->connectedInterface ? src->connectedInterface->hostBlock->blockId : 0;
    uint64_t t = getScheduler()->now();

    authentificationsEnCours.erase(src);
    n0EnAttente.erase(src);
    authFailed = true;

    console << "\n[t=" << t << "] *** AUTH_ECHEC reçu par M" << module->blockId
            << " de M" << srcId << " ***\n";

    if (isFalsified) {
        console << "  Code source NON CONFORME — module rejeté\n";
        console << "  M" << module->blockId << " retourne à " << POS_RETURN_FAKE << "\n";
        module->setColor(RED);
        isReturning = true;
        myHome = POS_RETURN_FAKE;
        visited.clear();
        visited.insert(module->position);
        moveSteps = 0;
        tryMoveToward(POS_RETURN_FAKE);
        return;
    }

    console << "  Authentification refusée — M" << module->blockId << " passe en ROUGE\n";
    module->setColor(RED);
}

// ---------------------------------------------------------------------------
// Traitement des événements
// ---------------------------------------------------------------------------
void MonApplicationTestCode::processLocalEvent(EventPtr pev) {
    Catoms3DBlockCode::processLocalEvent(pev);

    switch (pev->eventType) {
        case EVENT_NI_RECEIVE: {
            auto msg = static_pointer_cast<NetworkInterfaceReceiveEvent>(pev)->message;
            P2PNetworkInterface* src = msg->destinationInterface;
            switch (msg->type) {
                case MSG_AUTH_REQUEST:    surReceptionDemandeAuth(msg, src);   break;
                case MSG_KEY_CHALLENGE:   surReceptionDefiCle(msg, src);       break;
                case MSG_AUTH_ECHEC:      surReceptionAuthEchec(src);          break;
                case MSG_PROOF_REQUEST:   surReceptionPreuveRequete(msg, src); break;
                case MSG_PROOF_N1:        surReceptionPreuveN1(msg, src);      break;
                case MSG_PROOF_RELAY:     surReceptionPreuveRelais(msg, src);  break;
                case MSG_PROOF_CHALLENGE: surReceptionPreuveDefi(msg, src);    break;
            }
            break;
        }
        case EVENT_INTERRUPTION: {
            auto id = static_pointer_cast<InterruptionEvent<unsigned int>>(pev)->data;
            if (id == 0 && infoSecurite.estModuleInitial)
                lancerReconfiguration();
            break;
        }
        default: break;
    }
}

// ---------------------------------------------------------------------------
// Déplacement
// ---------------------------------------------------------------------------
bool MonApplicationTestCode::tryMoveToward(const Cell3DPosition& goal) {
    if (module->position == goal) return false;
    if (moveSteps >= MAX_STEPS) {
        console << "[WARN] M" << module->blockId << " MAX_STEPS atteint\n";
        return false;
    }

    auto tab = Catoms3DMotionEngine::getAllRotationsForModule(module);
    Cell3DPosition fp; short fo;

    struct Candidate { Cell3DPosition pos; double dist; bool vis; };
    vector<Candidate> cands;

    bool requireVoisin = !infoSecurite.estModuleInitial;
    for (auto& elem : tab) {
        elem.second.init(((Catoms3DGlBlock*)module->ptrGlBlock)->mat);
        elem.second.getFinalPositionAndOrientation(fp, fo);
        if (!lattice->isFree(fp)) continue;
        int nbVoisins = 0;
        for (auto& np : lattice->getNeighborhood(fp))
            if (lattice->cellHasBlock(np) && np != module->position) nbVoisins++;
        if (requireVoisin && nbVoisins < 1) continue;
        double d = (double)((fp[0]-goal[0])*(fp[0]-goal[0])
                          + (fp[1]-goal[1])*(fp[1]-goal[1])
                          + (fp[2]-goal[2])*(fp[2]-goal[2]));
        cands.push_back({fp, d, visited.count(fp) > 0});
    }

    if (cands.empty()) {
        console << "[WARN] M" << module->blockId << " : aucune position candidate\n";
        return false;
    }

    Cell3DPosition best; double bestDist = 1e9; bool found = false;
    for (auto& c : cands)
        if (!c.vis && c.dist < bestDist) { bestDist = c.dist; best = c.pos; found = true; }

    if (!found) {
        visited.clear();
        visited.insert(module->position);
        bestDist = 1e9;
        for (auto& c : cands)
            if (c.dist < bestDist) { bestDist = c.dist; best = c.pos; found = true; }
    }

    if (found) {
        visited.insert(best);
        moveSteps++;
        bool moved = ((Catoms3DBlock*)module)->moveTo(best);
        if (!moved) moveSteps--;
        return moved;
    }
    return false;
}

// ---------------------------------------------------------------------------
// Fin de déplacement
// ---------------------------------------------------------------------------
void MonApplicationTestCode::onMotionEnd() {
    // Gestion du retour du module falsifié après échec
    if (isReturning) {
        if (module->position == myHome) {
            console << "[Reconf] M" << module->blockId
                    << " retourné à " << POS_RETURN_FAKE << " (ROUGE)\n";
            module->setColor(RED);
            isReturning = false;
            fakeEnCours = false;
            fakeDone    = true;
            lancerProchainMobile();
        } else {
            if (!tryMoveToward(myHome)) {
                visited.clear();
                visited.insert(module->position);
                moveSteps = 0;
                tryMoveToward(myHome);
            }
        }
        return;
    }

    if (module->position == myTarget) {
        // Nettoyer les interfaces déconnectées
        for (auto it = voisinsAuthentifies.begin(); it != voisinsAuthentifies.end(); )
            if (!it->first->connectedInterface) it = voisinsAuthentifies.erase(it); else ++it;
        for (auto it = clesVoisins.begin(); it != clesVoisins.end(); )
            if (!it->first->connectedInterface) it = clesVoisins.erase(it); else ++it;
        authentificationsEnCours.clear();
        n0EnAttente.clear();

        console << "[Reconf] M" << module->blockId << " arrivé à " << myTarget << "\n";

        if (isFalsified) {
            for (int i = 0; i < 12; i++) {
                P2PNetworkInterface* iface = module->getInterface(i);
                if (!ifaceValide(iface, module)) continue;
                MonApplicationTestCode* vc =
                    (MonApplicationTestCode*)iface->connectedInterface->hostBlock->blockCode;
                if (vc->estDansStructure()) {
                    algorithme1_InitierFalsifie(iface);
                    return;
                }
            }
            console << "[WARN] M" << module->blockId << " (FALSIFIÉ) : aucun voisin authentifié\n";
            return;
        }

        if (infoSecurite.estModuleInitial) {
            console << "[Reconf] iM arrivé à sa cible — reconfiguration terminée\n";
            return;
        }

        for (int i = 0; i < 12; i++) {
            P2PNetworkInterface* iface = module->getInterface(i);
            if (!ifaceValide(iface, module)) continue;
            MonApplicationTestCode* vc =
                (MonApplicationTestCode*)iface->connectedInterface->hostBlock->blockCode;
            if (!vc->estDansStructure()) continue;

            if (estDansStructure()) {
                // Les deux dans la structure → Algo 2
                for (int j = 0; j < 12; j++) {
                    if (j == i) continue;
                    P2PNetworkInterface* iface2 = module->getInterface(j);
                    if (!ifaceValide(iface2, module)) continue;
                    MonApplicationTestCode* vc2 =
                        (MonApplicationTestCode*)iface2->connectedInterface->hostBlock->blockCode;
                    if (vc2->estDansStructure() && !voisinsAuthentifies.count(iface2)) {
                        algorithme2_Initier(iface, iface2);
                        return;
                    }
                }
                lancerProchainMobile();
            } else {
                // Nouveau module → Algo 1
                algorithme1_Initier(iface);
            }
            return;
        }
        console << "[WARN] M" << module->blockId << " : aucun voisin authentifié — séquence continuée\n";
        lancerProchainMobile();
    } else {
        if (!tryMoveToward(myTarget)) {
            visited.clear();
            visited.insert(module->position);
            moveSteps = 0;
            tryMoveToward(myTarget);
        }
    }
}

void MonApplicationTestCode::afficherStatsGlobal() const {}
void MonApplicationTestCode::parseUserBlockElements(TiXmlElement* /*config*/) {}

// ---------------------------------------------------------------------------
// Constructeurs de messages
// ---------------------------------------------------------------------------
MessageDemandeAuth::MessageDemandeAuth(int _liens,
                                       const vector<uint8_t>& _n1,
                                       const vector<uint8_t>& _x,
                                       const vector<uint8_t>& _K0)
    : Message(), liens(_liens), n1(_n1), x(_x), K0(_K0) { type = MSG_AUTH_REQUEST; }

MessageDefiCle::MessageDefiCle(const vector<uint8_t>& _x1)
    : Message(), x1(_x1) { type = MSG_KEY_CHALLENGE; }

MessageAuthEchec::MessageAuthEchec()
    : Message() { type = MSG_AUTH_ECHEC; }

// ---------------------------------------------------------------------------
// Constructeurs des messages Algorithme 2
// ---------------------------------------------------------------------------
MessagePreuveRequete::MessagePreuveRequete(int _ttl, const vector<uint8_t>& _x)
    : Message(), ttl(_ttl), x(_x) { type = MSG_PROOF_REQUEST; }

MessagePreuveN1::MessagePreuveN1(int _ttl, const vector<uint8_t>& _n1)
    : Message(), ttl(_ttl), n1(_n1) { type = MSG_PROOF_N1; }

MessagePreuveRelais::MessagePreuveRelais(int _ttl, const vector<uint8_t>& _xv)
    : Message(), ttl(_ttl), xv(_xv) { type = MSG_PROOF_RELAY; }

MessagePreuveDefi::MessagePreuveDefi(const vector<uint8_t>& _x1)
    : Message(), x1(_x1) { type = MSG_PROOF_CHALLENGE; }

// ---------------------------------------------------------------------------
// Algorithme 2 — SF-1/SF-2 : initiation (côté 1M)
// relais : voisin commun déjà lié à 1M (clé K1 partagée)
// cible  : nouveau voisin dans la structure à authentifier
// ---------------------------------------------------------------------------
void MonApplicationTestCode::algorithme2_Initier(P2PNetworkInterface* relais,
                                                  P2PNetworkInterface* cible) {
    if (voisinsAuthentifies.count(cible) || authentificationsEnCours.count(cible)) return;

    if (!clesVoisins.count(relais)) {
        console << "[WARN] Algo2 : pas de clé avec le relais\n";
        return;
    }

    vector<uint8_t> n0 = genererNonce160();
    vector<uint8_t> n1 = H(n0);
    vector<uint8_t> x  = xorVec(clesVoisins[relais], n0);  // x = K1 ⊕ n0

    bID relaisId = relais->connectedInterface->hostBlock->blockId;
    bID cibleId  = cible->connectedInterface->hostBlock->blockId;

    console << "\n[t=" << getScheduler()->now() << "] *** ALGO2 SF-1 — M"
            << module->blockId << " vers M" << cibleId
            << " via relais M" << relaisId << " ***\n";
    console << "  n0 = " << hexShort(n0) << "  n1 = H(n0) = " << hexShort(n1) << "\n";
    console << "  x  = K1 XOR n0 = " << hexShort(x) << "\n";
    console << "  >>> SF-2a : (ttl=2, x) -> M" << relaisId << "\n";
    console << "  >>> SF-2b : (ttl=2, n1) -> M" << cibleId << "\n";

    sendMessage(new MessagePreuveRequete(2, x),  relais, DELAI_TRANSMISSION, 0);
    sendMessage(new MessagePreuveN1(2, n1),       cible,  DELAI_TRANSMISSION, 0);

    n0Preuve[cible] = n0;
    authentificationsEnCours.insert(cible);
    module->setColor(ORANGE);
}

// ---------------------------------------------------------------------------
// Algorithme 2 — SF-3a : relais reçoit (ttl, x), dérive n0, propage xv
// ---------------------------------------------------------------------------
void MonApplicationTestCode::algorithme2_TraiterRequete(P2PNetworkInterface* src,
                                                         int ttl,
                                                         const vector<uint8_t>& x) {
    if (!estDansStructure()) return;
    if (!clesVoisins.count(src)) {
        console << "[WARN] Algo2 TraiterRequete : pas de clé avec M"
                << src->connectedInterface->hostBlock->blockId << "\n";
        return;
    }

    vector<uint8_t> n0 = xorVec(clesVoisins[src], x);  // n0 = K1 ⊕ x
    bID srcId = src->connectedInterface->hostBlock->blockId;

    console << "\n[t=" << getScheduler()->now() << "] *** ALGO2 SF-3a — M"
            << module->blockId << " (relais) reçoit PROOF_REQUEST de M" << srcId << " ***\n";
    console << "  n0 = K1 XOR x = " << hexShort(n0) << "\n";

    // Propager xv = n0 ⊕ Kv à chaque voisin (sauf l'émetteur)
    for (auto& kv : clesVoisins) {
        if (kv.first == src || !kv.first->connectedInterface) continue;
        vector<uint8_t> xv = xorVec(n0, kv.second);
        bID smId = kv.first->connectedInterface->hostBlock->blockId;
        console << "  >>> SF-3 relais : xv -> M" << smId << "\n";
        sendMessage(new MessagePreuveRelais(ttl - 1, xv), kv.first, DELAI_TRANSMISSION, 0);
    }
}

// ---------------------------------------------------------------------------
// Algorithme 2 — SF-3b : cible (2M) reçoit (ttl, n1) de 1M, mémorise n1
// ---------------------------------------------------------------------------
void MonApplicationTestCode::algorithme2_TraiterN1(P2PNetworkInterface* src,
                                                    int /*ttl*/,
                                                    const vector<uint8_t>& n1) {
    if (!estDansStructure()) return;
    bID srcId = src->connectedInterface->hostBlock->blockId;
    console << "\n[t=" << getScheduler()->now() << "] *** ALGO2 SF-3b — M"
            << module->blockId << " reçoit PROOF_N1 de M" << srcId << " ***\n";
    console << "  n1 = " << hexShort(n1) << " (mémorisé, attente xv)\n";
    n1AttenteCible[src] = n1;
}

// ---------------------------------------------------------------------------
// Algorithme 2 — SF-4 : cible (2M) reçoit xv du relais, vérifie H(n0)==n1
// Si OK : génère K2, calcule x1=K2⊕n0, K3=HL(K2), envoie x1 à 1M
// ---------------------------------------------------------------------------
void MonApplicationTestCode::algorithme2_TraiterRelais(P2PNetworkInterface* src,
                                                        int /*ttl*/,
                                                        const vector<uint8_t>& xv) {
    if (!estDansStructure() || !clesVoisins.count(src)) return;

    vector<uint8_t> n0 = xorVec(clesVoisins[src], xv);  // n0 = Kv ⊕ xv
    bID srcId = src->connectedInterface->hostBlock->blockId;

    console << "\n[t=" << getScheduler()->now() << "] *** ALGO2 SF-4 — M"
            << module->blockId << " reçoit PROOF_RELAY de M" << srcId << " ***\n";
    console << "  n0 = Kv XOR xv = " << hexShort(n0) << "\n";

    // Trouver l'initiateur dont H(n0) == n1 mémorisé
    P2PNetworkInterface* initiateur = nullptr;
    for (auto& kv : n1AttenteCible) {
        if (H(n0) == kv.second) { initiateur = kv.first; break; }
    }
    if (!initiateur) {
        console << "  [ECHEC] Aucun n1 correspondant — preuve invalide\n";
        return;
    }

    bID initId = initiateur->connectedInterface->hostBlock->blockId;
    console << "  H(n0) == n1 : preuve validée pour M" << initId << "\n";
    n1AttenteCible.erase(initiateur);

    // SF-5 : K2 aléatoire, x1 = K2 ⊕ n0, K3 = HL(K2)
    vector<uint8_t> K2 = genererNonce160();
    vector<uint8_t> x1 = xorVec(K2, n0);
    vector<uint8_t> K3 = HL(K2);

    clesVoisins[initiateur]         = K3;
    voisinsAuthentifies[initiateur] = true;
    infoSecurite.liensStructure++;

    console << "  K2 = " << hexShort(K2) << "  K3 = HL(K2) = " << hexShort(K3) << "\n";
    console << "  >>> SF-5 : x1 -> M" << initId << "\n";
    sendMessage(new MessagePreuveDefi(x1), initiateur, DELAI_TRANSMISSION, 0);
}

// ---------------------------------------------------------------------------
// Algorithme 2 — SF-6 : initiateur (1M) reçoit x1, dérive K2 et K3
// ---------------------------------------------------------------------------
void MonApplicationTestCode::algorithme2_Completer(P2PNetworkInterface* src,
                                                    const vector<uint8_t>& x1) {
    if (!n0Preuve.count(src)) {
        console << "[WARN] Algo2 SF-6 : n0 introuvable\n";
        return;
    }

    vector<uint8_t> n0 = n0Preuve[src];
    vector<uint8_t> K2 = xorVec(x1, n0);
    vector<uint8_t> K3 = HL(K2);
    bID srcId = src->connectedInterface->hostBlock->blockId;

    console << "\n[t=" << getScheduler()->now() << "] *** ALGO2 SF-6 — M"
            << module->blockId << " reçoit PROOF_CHALLENGE de M" << srcId << " ***\n";
    console << "  K2 = x1 XOR n0 = " << hexShort(K2) << "\n";
    console << "  K3 = HL(K2)    = " << hexShort(K3) << "\n";
    console << "  ==> AUTHENTIFICATION MUTUELLE REUSSIE (Algo2)\n";

    clesVoisins[src]         = K3;
    voisinsAuthentifies[src] = true;
    infoSecurite.liensStructure++;
    n0Preuve.erase(src);
    authentificationsEnCours.erase(src);

    module->setColor(GREEN);
    lancerProchainMobile();
}

// ---------------------------------------------------------------------------
// Gestionnaires de messages Algorithme 2
// ---------------------------------------------------------------------------
void MonApplicationTestCode::surReceptionPreuveRequete(shared_ptr<Message> msg,
                                                        P2PNetworkInterface* src) {
    auto* m = static_cast<MessagePreuveRequete*>(msg.get());
    algorithme2_TraiterRequete(src, m->ttl, m->x);
}
void MonApplicationTestCode::surReceptionPreuveN1(shared_ptr<Message> msg,
                                                   P2PNetworkInterface* src) {
    auto* m = static_cast<MessagePreuveN1*>(msg.get());
    algorithme2_TraiterN1(src, m->ttl, m->n1);
}
void MonApplicationTestCode::surReceptionPreuveRelais(shared_ptr<Message> msg,
                                                       P2PNetworkInterface* src) {
    auto* m = static_cast<MessagePreuveRelais*>(msg.get());
    algorithme2_TraiterRelais(src, m->ttl, m->xv);
}
void MonApplicationTestCode::surReceptionPreuveDefi(shared_ptr<Message> msg,
                                                     P2PNetworkInterface* src) {
    auto* m = static_cast<MessagePreuveDefi*>(msg.get());
    algorithme2_Completer(src, m->x1);
}
