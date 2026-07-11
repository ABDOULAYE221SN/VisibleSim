/**
 * MonApplicationCode.cpp
 * Protocole AKC-PM — 6 modules, reconfiguration sequentielle
 * Tout module authentifie peut verifier une demande d'authentification
 */
#include "MonApplicationCode.hpp"
#include "../../simulatorCore/src/spongent160.h"
#include <algorithm>
#include <random>
#include <iomanip>
#include <sstream>
#include <fstream>
#include <stdexcept>
#include <sys/stat.h>
#if defined(__linux__)
#  include <unistd.h>
#elif defined(__APPLE__)
#  include <mach-o/dyld.h>
#endif

// ---------------------------------------------------------------------------
// Lecture réelle du binaire en mémoire pour L(i)
// ---------------------------------------------------------------------------
// Charge les NB_LIGNES_CODE*BYTES_PER_LINE premiers octets du binaire courant
// en les découpant en blocs de BYTES_PER_LINE = TAILLE_LIGNE_BITS/8 octets.
// Chaque appel à L(i) renvoie le bloc i, exactement comme décrit dans le papier.
// ---------------------------------------------------------------------------
static constexpr int BYTES_PER_LINE = 256 / 8;   // = 32 octets

static const std::vector<uint8_t>& getBinaryCodeSegment() {
    static std::vector<uint8_t> cache;
    if (!cache.empty()) return cache;

    // Obtenir le chemin du binaire en cours d'exécution
    std::string path;
#if defined(__linux__)
    char buf[4096] = {};
    ssize_t len = ::readlink("/proc/self/exe", buf, sizeof(buf) - 1);
    if (len > 0) path = std::string(buf, len);
#elif defined(__APPLE__)
    char buf[4096] = {};
    uint32_t size = sizeof(buf);
    if (_NSGetExecutablePath(buf, &size) == 0) path = std::string(buf);
#endif

    if (path.empty()) {
        // Fallback : remplir avec des zéros si le chemin est inaccessible
        cache.assign(NB_LIGNES_CODE * BYTES_PER_LINE, 0);
        return cache;
    }

    std::ifstream f(path, std::ios::binary);
    if (!f) {
        cache.assign(NB_LIGNES_CODE * BYTES_PER_LINE, 0);
        return cache;
    }

    // Lire la taille du fichier
    f.seekg(0, std::ios::end);
    std::streamsize fileSize = f.tellg();
    f.seekg(0, std::ios::beg);

    std::streamsize needed = NB_LIGNES_CODE * BYTES_PER_LINE;
    std::streamsize toRead = std::min(needed, fileSize);

    cache.resize(needed, 0);
    f.read(reinterpret_cast<char*>(cache.data()), toRead);
    // Les octets restants (si le binaire est trop petit) restent à 0
    return cache;
}

using namespace std;

// --- Positions initiales ---
static const Cell3DPosition POS_A(2,3,2);   // -> TARGET_A
static const Cell3DPosition POS_B(3,3,2);   // -> TARGET_B
static const Cell3DPosition POS_C(4,3,2);   // module initial (iM) -> TARGET_IM
static const Cell3DPosition POS_D(2,2,2);   // -> TARGET_D
static const Cell3DPosition POS_E(3,2,2);   // -> TARGET_E
static const Cell3DPosition POS_F(4,2,2);   // -> TARGET_F

// --- Positions cibles ---
static const Cell3DPosition TARGET_A (5,3,2);
static const Cell3DPosition TARGET_B (6,1,2);
static const Cell3DPosition TARGET_D (6,3,2);
static const Cell3DPosition TARGET_E (6,2,2);
static const Cell3DPosition TARGET_F (6,0,2);
static const Cell3DPosition TARGET_IM(7,3,2);

// Ordre de deplacement : A, D, E, B, F, puis iM en dernier
static const Cell3DPosition MOVE_ORDER[] = {
    POS_A, POS_D, POS_E, POS_B, POS_F
};
static const int NB_MOVERS = 5;

static bool reconfigStarted  = false;
static int  nextMoverIndex   = 0;   // indice dans MOVE_ORDER

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

MonApplicationCode::MonApplicationCode(Catoms3DBlock *host)
    : Catoms3DBlockCode(host), module(host) {}
MonApplicationCode::~MonApplicationCode() {}

vector<uint8_t> MonApplicationCode::H(const vector<uint8_t>& data) {
    return Spongent::Spongent160::hash(data);
}
vector<uint8_t> MonApplicationCode::H(uint64_t val) {
    vector<uint8_t> v(8);
    for (int i = 0; i < 8; i++) v[i] = (uint8_t)((val >> (i*8)) & 0xFF);
    return H(v);
}
vector<uint8_t> MonApplicationCode::L(int ligne) {
    // Lecture réelle du binaire en mémoire (plus de simulation LCG)
    const auto& seg = getBinaryCodeSegment();
    int idx = ((ligne % NB_LIGNES_CODE) + NB_LIGNES_CODE) % NB_LIGNES_CODE;
    int offset = idx * BYTES_PER_LINE;
    return vector<uint8_t>(seg.begin() + offset, seg.begin() + offset + BYTES_PER_LINE);
}
vector<uint8_t> MonApplicationCode::HL(const vector<uint8_t>& n) {
    uint64_t val = 0;
    for (int i = 0; i < 8 && i < (int)n.size(); i++)
        val |= ((uint64_t)n[i]) << (i*8);
    return HL(val);
}
vector<uint8_t> MonApplicationCode::HL(uint64_t n) {
    return H(L((int)(n % NB_LIGNES_CODE)));
}
vector<uint8_t> MonApplicationCode::xorVec(const vector<uint8_t>& a, const vector<uint8_t>& b) {
    vector<uint8_t> res(a.size());
    for (size_t i = 0; i < a.size(); i++) res[i] = a[i] ^ b[i % b.size()];
    return res;
}
vector<uint8_t> MonApplicationCode::genererNonce160() {
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
vector<uint8_t> MonApplicationCode::tsVersVec(uint64_t ts) {
    uint64_t Ts = ts / DIVISEUR_SYNC;
    vector<uint8_t> v(HASH_OUTPUT_BYTES, 0);
    for (int i = 0; i < 8; i++) v[i] = (uint8_t)((Ts >> (i*8)) & 0xFF);
    return v;
}

bool MonApplicationCode::estDansStructure() const {
    return infoSecurite.estModuleInitial ||
           infoSecurite.etat == ETAT_AUTHENTIFIE ||
           infoSecurite.etat == ETAT_CLE_ETABLIE;
}

void MonApplicationCode::definirCommeModuleInitial() {
    infoSecurite.estModuleInitial = true;
    infoSecurite.etat             = ETAT_AUTHENTIFIE;
    infoSecurite.liensStructure   = 0;
    module->setColor(BLUE);
    console << "[iM] Module " << module->blockId << " designe module initial (BLEU)\n";
}


void MonApplicationCode::startup() {
    isReturning = false;
    authFailed  = false;

    if (module->position == POS_C) {
        // Module initial
        definirCommeModuleInitial();
        getScheduler()->schedule(
            new InterruptionEvent<unsigned int>(getScheduler()->now() + 100, module, 0));
        myTarget = TARGET_IM;
    } else {
        module->setColor(GREY);
        console << "[t=" << getScheduler()->now() << "] M" << module->blockId
                << " en attente (GRIS)\n";
        // Assigner la cible selon la position initiale
        if      (module->position == POS_A) myTarget = TARGET_A;
        else if (module->position == POS_B) myTarget = TARGET_B;
        else if (module->position == POS_D) myTarget = TARGET_D;
        else if (module->position == POS_E) myTarget = TARGET_E;
        else if (module->position == POS_F) myTarget = TARGET_F;
    }
}

// SF-1/SF-2 : authentification normale
void MonApplicationCode::algorithme1_Initier(P2PNetworkInterface* dest) {
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
    console << "  >>> SF-2 : M" << module->blockId << " envoie AUTH_REQUEST a M" << destId << "\n";
    console << "             MSG = (n1=" << hexShort(infoSecurite.n1)
            << ", x=" << hexShort(x) << ", K0=" << hexShort(infoSecurite.K0) << ")\n";

    sendMessage(new MessageDemandeAuth(infoSecurite.liensStructure,
                                       infoSecurite.n1, x, infoSecurite.K0),
                dest, DELAI_TRANSMISSION, 0);
    n0EnAttente[dest] = infoSecurite.n0;
    authentificationsEnCours.insert(dest);
    infoSecurite.etat = ETAT_AUTHENTIFICATION;
    module->setColor(ORANGE);
}


// SF-3/SF-4 : tout module authentifie peut verifier
void MonApplicationCode::algorithme1_Verifier(P2PNetworkInterface* src, int /*liens*/,
                                               const vector<uint8_t>& n1,
                                               const vector<uint8_t>& x,
                                               const vector<uint8_t>& K0) {
    if (voisinsAuthentifies.count(src)) return;
    if (!estDansStructure()) {
        console << "[Algo1 SF-3] REJETE: verificateur pas dans la structure\n";
        sendMessage(new MessageAuthEchec(), src, DELAI_TRANSMISSION, 0);
        return;
    }

    bID srcId = src->connectedInterface->hostBlock->blockId;
    uint64_t Tr = getScheduler()->now();

    console << "\n[t=" << Tr << "] *** ALGO1 SF-3 — M" << module->blockId
            << " (verificateur) recoit AUTH_REQUEST de M" << srcId << " ***\n";
    console << "  MSG recu : n1=" << hexShort(n1)
            << ", x=" << hexShort(x) << ", K0=" << hexShort(K0) << "\n";

    uint64_t Ts_base = (Tr - DELAI_TRANSMISSION) / DIVISEUR_SYNC;
    vector<uint8_t> n0_prime;
    bool syncOK = false;
    int offset_ok = 0;

    console << "  Verification sync (Ts_base=" << Ts_base
            << ", tolerance=+-" << TOLERANCE_SYNC << ") ...\n";

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
        console << "  >>> AUTH_ECHEC envoye a M" << srcId << "\n";
        sendMessage(new MessageAuthEchec(), src, DELAI_TRANSMISSION, 0);
        return;
    }
    console << "  Sync OK (offset=" << offset_ok << ") : n0' = " << hexShort(n0_prime) << "\n";
    console << "  Verification K0 : HL(n0' mod Nb) =? K0 ...\n";

    if (HL(n0_prime) != K0) {
        console << "  [ECHEC] K0 invalide — HL(n0')=" << hexShort(HL(n0_prime))
                << " != K0=" << hexShort(K0) << "\n";
        console << "  >>> AUTH_ECHEC envoye a M" << srcId << "\n";
        sendMessage(new MessageAuthEchec(), src, DELAI_TRANSMISSION, 0);
        return;
    }
    console << "  K0 valide : meme code confirme\n";

    if (infoSecurite.n2.empty()) {
        infoSecurite.n2 = genererNonce160();
        console << "  n2 genere = " << hexShort(infoSecurite.n2) << " (SINGLE_KEY)\n";
    } else {
        console << "  n2 existant = " << hexShort(infoSecurite.n2) << " (SINGLE_KEY partage)\n";
    }

    vector<uint8_t> x1 = xorVec(infoSecurite.n2, n0_prime);
    vector<uint8_t> K1 = HL(infoSecurite.n2);

    clesVoisins[src]         = K1;
    voisinsAuthentifies[src] = true;
    infoSecurite.liensStructure++;

    console << "  K1 = HL(n2 mod Nb) = " << hexShort(K1) << "\n";
    console << "  x1 = n2 XOR n0'    = " << hexShort(x1) << "\n";
    console << "  >>> SF-4 : M" << module->blockId
            << " envoie KEY_CHALLENGE(x1=" << hexShort(x1) << ") a M" << srcId << "\n";

    sendMessage(new MessageDefiCle(x1), src, DELAI_TRANSMISSION, 0);
}

// SF-5 : nM complete l'authentification
void MonApplicationCode::algorithme1_Completer(P2PNetworkInterface* src,
                                                const vector<uint8_t>& x1) {
    if (voisinsAuthentifies.count(src)) return;
    if (!n0EnAttente.count(src)) return;

    bID srcId = src->connectedInterface->hostBlock->blockId;
    uint64_t t = getScheduler()->now();

    vector<uint8_t> n0 = n0EnAttente[src];
    vector<uint8_t> n2 = xorVec(x1, n0);
    vector<uint8_t> K1 = HL(n2);

    console << "\n[t=" << t << "] *** ALGO1 SF-5 — M" << module->blockId
            << " recoit KEY_CHALLENGE de M" << srcId << " ***\n";
    console << "  x1 recu = " << hexShort(x1) << "\n";
    console << "  n2 = x1 XOR n0 = " << hexShort(n2) << "\n";
    console << "  K1 = HL(n2 mod Nb) = " << hexShort(K1) << "\n";
    console << "  ==> Cle K1 partagee etablie\n";
    console << "  ==> AUTHENTIFICATION REUSSIE — M" << module->blockId << " devient VERT\n";

    infoSecurite.n2          = n2;
    infoSecurite.K1          = K1;
    clesVoisins[src]         = K1;
    voisinsAuthentifies[src] = true;
    infoSecurite.etat        = ETAT_CLE_ETABLIE;
    infoSecurite.liensStructure++;
    n0EnAttente.erase(src);
    authentificationsEnCours.erase(src);

    module->setColor(GREEN);

    // Lancer le prochain module mobile dans la sequence
    lancerProchainMobile();
}

void MonApplicationCode::lancerReconfiguration() {
    reconfigStarted = true;
    nextMoverIndex  = 0;
    lancerProchainMobile();
}

// Lance le prochain module dans l'ordre sequentiel
void MonApplicationCode::lancerProchainMobile() {
    if (nextMoverIndex < NB_MOVERS) {
        Cell3DPosition pos = MOVE_ORDER[nextMoverIndex];
        nextMoverIndex++;
        auto world = BaseSimulator::getWorld();
        for (auto& kv : world->buildingBlocksMap) {
            if (kv.second->position == pos) {
                MonApplicationCode* m = (MonApplicationCode*)kv.second->blockCode;
                console << "\n[Reconf] Lancement M" << m->module->blockId
                        << " depuis (" << pos[0] << "," << pos[1] << "," << pos[2]
                        << ") vers (" << m->myTarget[0] << "," << m->myTarget[1]
                        << "," << m->myTarget[2] << ")\n";
                m->visited.clear();
                m->visited.insert(m->module->position);
                m->moveSteps = 0;
                m->tryMoveToward(m->myTarget);
                return;
            }
        }
        // Module introuvable a cette position, passer au suivant
        lancerProchainMobile();
    } else {
        // Tous les modules mobiles sont arrives — lancer iM en dernier
        auto world = BaseSimulator::getWorld();
        for (auto& kv : world->buildingBlocksMap) {
            MonApplicationCode* m = (MonApplicationCode*)kv.second->blockCode;
            if (m->infoSecurite.estModuleInitial && m->module->position != TARGET_IM) {
                console << "\n[Reconf] Lancement iM M" << m->module->blockId
                        << " vers (" << TARGET_IM[0] << "," << TARGET_IM[1]
                        << "," << TARGET_IM[2] << ")\n";
                m->visited.clear();
                m->visited.insert(m->module->position);
                m->moveSteps = 0;
                m->tryMoveToward(TARGET_IM);
                return;
            }
        }
    }
}

void MonApplicationCode::surReceptionDemandeAuth(shared_ptr<Message> msg,
                                                  P2PNetworkInterface* src) {
    auto* m = static_cast<MessageDemandeAuth*>(msg.get());
    algorithme1_Verifier(src, m->liens, m->n1, m->x, m->K0);
}

void MonApplicationCode::surReceptionDefiCle(shared_ptr<Message> msg,
                                              P2PNetworkInterface* src) {
    auto* m = static_cast<MessageDefiCle*>(msg.get());
    if (n0EnAttente.count(src))
        algorithme1_Completer(src, m->x1);
}

void MonApplicationCode::surReceptionAuthEchec(P2PNetworkInterface* src) {
    bID srcId = src->connectedInterface ? src->connectedInterface->hostBlock->blockId : 0;
    uint64_t t = getScheduler()->now();

    authentificationsEnCours.erase(src);
    n0EnAttente.erase(src);
    authFailed = true;

    console << "\n[t=" << t << "] *** AUTH_ECHEC recu par M" << module->blockId
            << " de M" << srcId << " ***\n";
    console << "  Authentification refusee\n";
    console << "  M" << module->blockId << " passe en ROUGE\n";

    module->setColor(RED);
}

void MonApplicationCode::processLocalEvent(EventPtr pev) {
    Catoms3DBlockCode::processLocalEvent(pev);

    switch (pev->eventType) {
        case EVENT_NI_RECEIVE: {
            auto msg = static_pointer_cast<NetworkInterfaceReceiveEvent>(pev)->message;
            P2PNetworkInterface* src = msg->destinationInterface;
            switch (msg->type) {
                case MSG_AUTH_REQUEST:  surReceptionDemandeAuth(msg, src); break;
                case MSG_KEY_CHALLENGE: surReceptionDefiCle(msg, src);     break;
                case MSG_AUTH_ECHEC:    surReceptionAuthEchec(src);        break;
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

bool MonApplicationCode::tryMoveToward(const Cell3DPosition& goal) {
    if (module->position == goal) return false;
    if (moveSteps >= MAX_STEPS) {
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
        if (nbVoisins < 1) continue;
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

void MonApplicationCode::onMotionEnd() {
    if (module->position == myTarget) {
        // Purger les interfaces deconnectees apres le mouvement
        for (auto it = voisinsAuthentifies.begin(); it != voisinsAuthentifies.end(); )
            if (!it->first->connectedInterface) it = voisinsAuthentifies.erase(it); else ++it;
        for (auto it = clesVoisins.begin(); it != clesVoisins.end(); )
            if (!it->first->connectedInterface) it = clesVoisins.erase(it); else ++it;
        authentificationsEnCours.clear();
        n0EnAttente.clear();

        if (isReturning) {
            console << "[Reconf] M" << module->blockId
                    << " retourne a sa position initiale (ROUGE)\n";
            module->setColor(RED);
            return;
        }

        console << "[Reconf] M" << module->blockId << " arrive a ("
                << myTarget[0] << "," << myTarget[1] << "," << myTarget[2] << ")\n";

        // Chercher un voisin authentifie pour declencer l'Algo1
        for (int i = 0; i < 12; i++) {
            P2PNetworkInterface* iface = module->getInterface(i);
            if (!ifaceValide(iface, module)) continue;
            MonApplicationCode* vc =
                (MonApplicationCode*)iface->connectedInterface->hostBlock->blockCode;
            if (vc->estDansStructure()) {
                algorithme1_Initier(iface);
                return;
            }
        }
        console << "[WARN] M" << module->blockId << " : aucun voisin authentifie trouve\n";
    } else {
        if (!tryMoveToward(myTarget)) {
            visited.clear();
            visited.insert(module->position);
            moveSteps = 0;
            tryMoveToward(myTarget);
        }
    }
}

void MonApplicationCode::parseUserBlockElements(TiXmlElement* /*config*/) {}

MessageDemandeAuth::MessageDemandeAuth(int _liens,
                                       const vector<uint8_t>& _n1,
                                       const vector<uint8_t>& _x,
                                       const vector<uint8_t>& _K0)
    : Message(), liens(_liens), n1(_n1), x(_x), K0(_K0) { type = MSG_AUTH_REQUEST; }

MessageDefiCle::MessageDefiCle(const vector<uint8_t>& _x1)
    : Message(), x1(_x1) { type = MSG_KEY_CHALLENGE; }

MessageAuthEchec::MessageAuthEchec()
    : Message() { type = MSG_AUTH_ECHEC; }
