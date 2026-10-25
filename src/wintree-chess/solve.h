// Risoluzione di una posizione con il nucleo df-pn (default, ../wintree/dfpn.h), con WinTree depth-first (iterative
// deepening in modalità WIN, una ricerca al tetto in NOWIN) o con la baseline df-pn di M2b, e verifica dell'esito
// (§7.9). Condiviso da CLI, regressioni, G0 (main.cpp) e benchmark (bench.h).
#ifndef WINTREE_CHESS_SOLVE_H
#define WINTREE_CHESS_SOLVE_H

#include "chess_game.h"
#include "dfpn_baseline.h"
#include "verifier.h"

#include "../wintree/dfpn.h"

#include "uci.h"

#include <algorithm>
#include <chrono>
#include <deque>
#include <string>
#include <time.h>
#include <vector>

namespace WinTree {

// Posizione con la storia della partita (FEN + mosse) nella catena StateInfo, come uci.cpp/thread.cpp:345-346.
struct Game {
    Stockfish::Position pos;
    std::deque<Stockfish::StateInfo> states;
    bool set(const std::string& fen, const std::vector<std::string>& moves, std::string& err, bool chess960 = false) {
        using namespace Stockfish;
        states.clear(); states.emplace_back();
        if (auto e = pos.set(fen, chess960, &states.back())) { err = e->what(); return false; }
        if (pos.attackers_to_exist(pos.square<KING>(~pos.side_to_move()), pos.pieces(), pos.side_to_move())) { err = "il re di chi non è al tratto è in presa"; return false; }
        for (const std::string& s : moves) {
            Move m = UCIEngine::to_move(pos, s);
            if (m == Move::none()) { err = "mossa illegale: " + s; return false; }
            states.emplace_back(); pos.do_move(m, states.back());
        }
        return true;
    }
};

enum class Core : u8 { DFPN, WINTREE, BASE };  // nucleo di produzione (§8.1b), depth-first (§8.1), baseline di M2b

struct Cfg {                                   // parametri comuni ai tre nuclei
    Core core = Core::DFPN;
    u64 nodes = 1000000;                       // budget di espansioni
    u64 moves = ~u64(0) >> 1;                  // budget di do_move (nodo di §9.4; df-pn ne fa ~35 per espansione)
    double secs = 0;                           // tetto di tempo in secondi (0 = nessuno)
    int depth = 0;                             // 0 = default della modalità (245 WIN, 4096 NOWIN)
    int tbMax = 0;
    Order order = Order::COLD; WarmSources warm;
    bool firstNil = true, dejavu = true;       // ablazioni B5 (O6 e dejavu)
    bool mateOnly = false;                     // B2: terminali solo matto/stallo
    double eps = 0.25; bool heur = true;       // df-pn: 1+ε e inizializzazione euristica di pn/dn (§8.1b)
    bool minD = false;                         // df-pn: dopo una PROVEN_WIN ri-ricerca a tetto decrescente per un d minore (attività (e))
    bool repLoss = true; unsigned dfpnLog2 = 22;   // baseline: ripetizione, log2 delle entry (16 B) della sua tabella
};

struct Outcome {
    Result r; u64 nodes = 0, moves = 0, andNodes = 0, reexp = 0; int depth = 0, iters = 0, maxPly = 0; double meanPly = 0, secs = 0, cpu = 0;
    int d0 = 0; u64 nodes0 = 0, moves0 = 0;    // minD: d, espansioni e do_move della prima prova (il resto è il costo della minimizzazione)
};

inline double thread_cpu() { timespec ts; clock_gettime(CLOCK_THREAD_CPUTIME_ID, &ts); return ts.tv_sec + ts.tv_nsec * 1e-9; }
inline Clock::time_point deadline_of(double secs) { return secs > 0 ? Clock::now() + std::chrono::microseconds(long(secs * 1e6)) : Clock::time_point::max(); }

// WIN: iterative deepening sulla profondità (§8.4: 1, 3, 5, … da una radice OR; 2, 4, 6, … da una radice AND, dove il
// matto cade a ply pari) con budget di nodi e tempo complessivi; i PROVEN restano in tabella, gli UNKNOWN_BUDGET
// sono ritentati solo con residuo maggiore. NOWIN: una sola ricerca al tetto.
inline Outcome solve(Stockfish::Position& pos, Stockfish::Color goal, Goal mode, const Cfg& c, ProofTable& pt) {
    auto t0 = Clock::now(); double c0 = thread_cpu();
    int depth = c.depth ? c.depth : mode == Goal::WIN ? 245 : 4096;
    ChessGame g(pos, goal, c.tbMax, depth, c.order, c.warm); g.mateOnly = c.mateOnly;
    Prover<ChessGame> p(g, pt, goal == Stockfish::BLACK, mode); p.firstNil = c.firstNil; p.dejavu = c.dejavu;
    Outcome o; Budget b; b.deadline = deadline_of(c.secs);
    for (int d = mode == Goal::WIN ? (pos.side_to_move() == goal ? 1 : 2) : depth;; d += 2) {
        b.maxDepth = std::min(d, depth); b.maxNodes = c.nodes - o.nodes; b.maxMoves = c.moves - o.moves;
        o.r = p.solve(b); o.nodes += p.nodes(); o.iters++; o.depth = b.maxDepth;
        o.andNodes += p.and_nodes(); o.reexp += p.reexpansions(); o.meanPly += p.mean_ply() * p.nodes(); o.maxPly = std::max(o.maxPly, p.max_ply());
        o.moves = g.moves;
        if (o.r.status != Status::UNKNOWN_BUDGET || p.aborted() || b.maxDepth >= depth) break;
    }
    if (o.nodes) o.meanPly /= o.nodes;
    o.secs = std::chrono::duration<double>(Clock::now() - t0).count(); o.cpu = thread_cpu() - c0;
    return o;
}

// Nucleo df-pn (§8.1b): una sola ricerca; la modalità fissa il tetto di profondità (245 WIN, 4096 NOWIN).
inline Outcome solve_dfpn(Stockfish::Position& pos, Stockfish::Color goal, Goal mode, const Cfg& c, ProofTable& pt) {
    auto t0 = Clock::now(); double c0 = thread_cpu();
    int depth = c.depth ? c.depth : mode == Goal::WIN ? 245 : 4096;
    ChessGame g(pos, goal, c.tbMax, depth, c.order, c.warm); g.heur = c.heur;
    DfPn<ChessGame> p(g, pt, goal == Stockfish::BLACK, mode, c.eps);
    Outcome o; Budget b; b.deadline = deadline_of(c.secs); b.maxDepth = depth; b.maxNodes = c.nodes; b.maxMoves = c.moves;
    o.r = p.solve(b); o.nodes = p.nodes(); o.moves = g.moves; o.iters = 1; o.depth = depth;
    o.andNodes = p.and_nodes(); o.meanPly = p.mean_ply(); o.maxPly = p.max_ply();
    o.d0 = o.r.d; o.nodes0 = o.nodes; o.moves0 = o.moves;
    // d minimo: ri-ricerche sulla stessa tabella con tetto decrescente (d − 2, poi dicotomia fra l'ultimo tetto
    // fallito `lo` e l'ultimo d trovato `hi`), entro il budget residuo; un tetto fallisce con UNKNOWN_CYCLE (tetto
    // ⇒ confutazione tainted), il budget esaurito ferma. Il d finale è il minimo trovato, non necessariamente il
    // minimo (una confutazione tainted di un tetto precedente può restare nell'ombra di una trasposizione).
    if (c.minD && o.r.status == Status::PROVEN_WIN) {
        p.capD = true;
        for (int lo = 0, hi = o.r.d, cap = hi - 2; cap > lo; cap = hi - 2 * std::max(1, (hi - lo) / 4)) {
            b.maxDepth = cap; b.maxNodes = c.nodes > o.nodes ? c.nodes - o.nodes : 0; b.maxMoves = c.moves > o.moves ? c.moves - o.moves : 0;
            Result r = p.solve(b); o.nodes += p.nodes(); o.moves = g.moves; o.iters++;
            if (r.status == Status::PROVEN_WIN) { o.r = r; hi = r.d; } else if (r.status == Status::UNKNOWN_CYCLE) lo = cap; else break;
        }
    }
    o.secs = std::chrono::duration<double>(Clock::now() - t0).count(); o.cpu = thread_cpu() - c0;
    return o;
}

inline Outcome solve_base(Stockfish::Position& pos, Stockfish::Color goal, const Cfg& c, ProofTable& pt) {
    auto t0 = Clock::now(); double c0 = thread_cpu();
    int depth = c.depth ? c.depth : 245;
    ChessGame g(pos, goal, c.tbMax, depth, c.order, c.warm);
    DfPnBaseline<ChessGame> p(g, pt, goal == Stockfish::BLACK, c.dfpnLog2, c.eps, c.repLoss);
    Outcome o; Budget b; b.deadline = deadline_of(c.secs); b.maxDepth = depth; b.maxNodes = c.nodes; b.maxMoves = c.moves;
    o.r = p.solve(b); o.nodes = p.nodes(); o.moves = g.moves; o.iters = 1; o.depth = depth; o.maxPly = p.max_ply();
    o.secs = std::chrono::duration<double>(Clock::now() - t0).count(); o.cpu = thread_cpu() - c0;
    return o;
}

inline Outcome solve_any(Stockfish::Position& pos, Stockfish::Color goal, Goal mode, const Cfg& c, ProofTable& pt) {
    return c.core == Core::DFPN ? solve_dfpn(pos, goal, mode, c, pt) : c.core == Core::WINTREE ? solve(pos, goal, mode, c, pt) : solve_base(pos, goal, c, pt);
}

inline std::string describe(const Result& r, bool chess960 = false) {
    switch (r.status) {
    case Status::PROVEN_WIN:     return "PROVEN_WIN d=" + std::to_string(r.d) + " rev=" + std::to_string(r.rev) + (r.move ? " mossa=" + Stockfish::UCIEngine::move(Stockfish::Move(r.move), chess960) : "");
    case Status::PROVEN_NOWIN:   return "PROVEN_NOWIN req=" + std::to_string(r.req) + (r.move ? " mossa=" + Stockfish::UCIEngine::move(Stockfish::Move(r.move), chess960) : "");
    case Status::UNKNOWN_CYCLE:  return "UNKNOWN_CYCLE";
    default:                     return "UNKNOWN_BUDGET resid=" + std::to_string(r.resid);
    }
}

// Verifica (§7.9) dell'esito di una ricerca completa: albero vincente per PROVEN_WIN, grafo di strategia di patta
// per PROVEN_NOWIN e per una radice UNKNOWN_CYCLE (completa per I2). UNKNOWN_BUDGET: nulla da verificare.
inline Certificate verify(Stockfish::Position& pos, Stockfish::Color goal, const Result& r, int tbMax, ProofTable& pt, int depth) {
    if (r.status == Status::PROVEN_WIN) return WinTreeVerifier(pos, goal, tbMax, pt, depth).run();
    if (r.status == Status::UNKNOWN_BUDGET) { Certificate c; c.fail("UNKNOWN_BUDGET: nessun oggetto di prova"); return c; }
    return verify_draw_strategy(pos.fen(), goal, tbMax, pt, pos.is_chess960());
}

inline std::string describe(const Certificate& c) {
    return (c.ok ? "certificato" : "NON certificato (" + c.why + ")") + std::string(": nodi ") + std::to_string(c.nodes)
         + " foglie " + std::to_string(c.leaves) + " TB " + std::to_string(c.tbLeaves) + (c.depth ? " profondità " + std::to_string(c.depth) : "")
         + (c.researches ? " ri-ricerche " + std::to_string(c.researches) : "");
}

}  // namespace WinTree

#endif
