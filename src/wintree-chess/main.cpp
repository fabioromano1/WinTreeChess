// wintree-chess (spec §9.1 M2, prima metà): CLI del prover WinTree sugli scacchi, verificatore di §7.9,
// regressioni e test G0 (campione di posizioni a <= 5 pezzi risolte senza TB e confrontate con le WDL Syzygy).
// Linka i sorgenti di Stockfish 19 senza modificarli.
#include "chess_game.h"
#include "verifier.h"

#include "attacks.h"
#include "movegen.h"
#include "position.h"
#include "syzygy/tbprobe.h"
#include "uci.h"

#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <deque>
#include <iostream>
#include <mutex>
#include <random>
#include <string>
#include <thread>
#include <vector>

using namespace Stockfish;
using WinTree::u64;

namespace {


// Posizione con la storia della partita (FEN + mosse) nella catena StateInfo, come uci.cpp/thread.cpp:345-346.
struct Game {
    Position pos;
    std::deque<StateInfo> states;
    bool set(const std::string& fen, const std::vector<std::string>& moves, std::string& err) {
        states.clear(); states.emplace_back();
        if (auto e = pos.set(fen, false, &states.back())) { err = e->what(); return false; }
        if (pos.attackers_to_exist(pos.square<KING>(~pos.side_to_move()), pos.pieces(), pos.side_to_move())) { err = "il re di chi non è al tratto è in presa"; return false; }
        for (const std::string& s : moves) {
            Move m = UCIEngine::to_move(pos, s);
            if (m == Move::none()) { err = "mossa illegale: " + s; return false; }
            states.emplace_back(); pos.do_move(m, states.back());
        }
        return true;
    }
};

struct Outcome { WinTree::Result r; u64 nodes = 0; int depth = 0, iters = 0; double secs = 0; };

// WIN: iterative deepening sulla profondità (§8.4: 1, 3, 5, ...) con budget di nodi complessivo; i PROVEN restano
// in tabella, gli UNKNOWN_BUDGET sono ritentati solo con residuo maggiore. NOWIN: una sola ricerca al tetto.
Outcome solve(Position& pos, Color goal, WinTree::Goal mode, u64 budget, int depth, int tbMax, WinTree::ProofTable& pt) {
    auto t0 = WinTree::Clock::now();
    WinTree::ChessGame g(pos, goal, tbMax, depth);
    WinTree::Prover<WinTree::ChessGame> p(g, pt, goal == BLACK, mode);
    Outcome o; WinTree::Budget b;
    for (int d = mode == WinTree::Goal::WIN ? 1 : depth;; d += 2) {
        b.maxDepth = std::min(d, depth); b.maxNodes = budget - o.nodes;
        o.r = p.solve(b); o.nodes += p.nodes(); o.iters++; o.depth = b.maxDepth;
        if (o.r.status != WinTree::Status::UNKNOWN_BUDGET || p.aborted() || b.maxDepth >= depth) break;
    }
    o.secs = std::chrono::duration<double>(WinTree::Clock::now() - t0).count();
    return o;
}

std::string describe(const WinTree::Result& r) {
    using WinTree::Status;
    switch (r.status) {
    case Status::PROVEN_WIN:     return "PROVEN_WIN d=" + std::to_string(r.d) + " rev=" + std::to_string(r.rev) + (r.move ? " mossa=" + UCIEngine::move(Move(r.move)) : "");
    case Status::PROVEN_NOWIN:   return "PROVEN_NOWIN req=" + std::to_string(r.req) + (r.move ? " mossa=" + UCIEngine::move(Move(r.move)) : "");
    case Status::UNKNOWN_CYCLE:  return "UNKNOWN_CYCLE";
    default:                     return "UNKNOWN_BUDGET resid=" + std::to_string(r.resid);
    }
}

// Verifica (§7.9) dell'esito di una ricerca completa: albero vincente per PROVEN_WIN, grafo di strategia di patta
// per PROVEN_NOWIN e per una radice UNKNOWN_CYCLE (completa per I2). UNKNOWN_BUDGET: nulla da verificare.
WinTree::Certificate verify(Position& pos, Color goal, const WinTree::Result& r, int tbMax, const WinTree::ProofTable& pt, int depth) {
    using WinTree::Status;
    if (r.status == Status::PROVEN_WIN) return WinTree::WinTreeVerifier(pos, goal, tbMax, pt, depth).run();
    if (r.status == Status::UNKNOWN_BUDGET) { WinTree::Certificate c; c.fail("UNKNOWN_BUDGET: nessun oggetto di prova"); return c; }
    return WinTree::verify_draw_strategy(pos.fen(), goal, tbMax, pt);
}

std::string describe(const WinTree::Certificate& c) {
    return (c.ok ? "certificato" : "NON certificato (" + c.why + ")") + std::string(": nodi ") + std::to_string(c.nodes)
         + " foglie " + std::to_string(c.leaves) + " TB " + std::to_string(c.tbLeaves);
}

// Posizione casuale legale con n pezzi (re compresi), senza arrocco ne' ep, contatore 0; pedoni mai in 1ª/8ª.
std::string random_fen(std::mt19937_64& rng, int n) {
    for (;;) {
        char board[64]; std::memset(board, 0, 64);
        auto sq = [&]() { int s; do s = int(rng() % 64); while (board[s]); return s; };
        int wk = sq(); board[wk] = 'K';
        int bk = sq(); if (std::abs(wk / 8 - bk / 8) <= 1 && std::abs(wk % 8 - bk % 8) <= 1) continue;
        board[bk] = 'k';
        for (int i = 2; i < n; i++) {
            char pc = "PNBRQ"[rng() % 5]; if (rng() & 1) pc = char(pc + 32);
            int s = sq(); if ((pc == 'P' || pc == 'p') && (s / 8 == 0 || s / 8 == 7)) { i--; continue; }
            board[s] = pc;
        }
        std::string fen;
        for (int r = 7; r >= 0; r--) {
            int empty = 0;
            for (int f = 0; f < 8; f++) { char p = board[r * 8 + f]; if (!p) empty++; else { if (empty) fen += char('0' + empty); empty = 0; fen += p; } }
            if (empty) fen += char('0' + empty);
            fen += r ? '/' : ' ';
        }
        return fen + (rng() & 1 ? "w" : "b") + " - - 0 1";
    }
}

int failures = 0;
void expect(bool ok, const std::string& what) { std::cout << "  " << (ok ? "OK   " : "FAIL ") << what << "\n"; failures += !ok; }

// Regressioni: le 4 posizioni di matto/stallo del bench di Stockfish (benchmark.cpp:91-94) e casi elementari.
// `dtm` = distanza al matto in ply secondo Stockfish 19 (go depth 28): per Thm 3.6 il d trovato è >= dtm.
void regress(int tbMax, int hashLog2) {
    using WinTree::Goal;
    struct Case { const char* fen; Color goal; Goal mode; const char* expect; int dtm; } cases[] = {
        {"6k1/3b3r/1p1p4/p1n2p2/1PPNpP1q/P3Q1p1/1R1RB1P1/5K2 b - - 0 1", BLACK, Goal::WIN, "PROVEN_WIN", 9},   // bench: matto in 5
        {"r2r1n2/pp2bk2/2p1p2p/3q4/3PN1QP/2P3R1/P4PP1/5RK1 w - - 0 1", WHITE, Goal::WIN, "PROVEN_WIN", 7},     // bench: matto in 4
        {"8/8/8/8/8/6k1/6p1/6K1 w - - 0 1", WHITE, Goal::WIN, "PROVEN_NOWIN", 0},                              // bench: stallo
        {"8/8/8/8/8/6k1/6p1/6K1 w - - 0 1", BLACK, Goal::WIN, "PROVEN_NOWIN", 0},
        {"7k/7P/6K1/8/3B4/8/8/8 b - - 0 1", WHITE, Goal::WIN, "PROVEN_WIN", 0},                                // bench: nero matto
        {"7k/7P/6K1/8/3B4/8/8/8 b - - 0 1", BLACK, Goal::WIN, "PROVEN_NOWIN", 0},
        {"6k1/5ppp/8/8/8/8/8/4R1K1 w - - 0 1", WHITE, Goal::WIN, "PROVEN_WIN", 1},                             // Re8#
        {"k7/8/1K6/8/8/8/8/7R b - - 0 1", WHITE, Goal::WIN, "PROVEN_WIN", 2},                                  // ogni mossa nera subisce matto
        {"k7/8/1K6/8/8/8/8/7R b - - 0 1", BLACK, Goal::WIN, "PROVEN_NOWIN", 0},
        {"8/8/8/8/8/4k3/8/4K2Q w - - 0 1", WHITE, Goal::WIN, "PROVEN_WIN", 0},                                 // KQ-K
        {"8/8/8/8/8/4k3/8/4K2Q w - - 0 1", BLACK, Goal::NOWIN, "PROVEN_NOWIN", 0},                             // il re solo non vince
        {"8/8/8/8/8/4k3/8/4K2N w - - 0 1", WHITE, Goal::WIN, "PROVEN_NOWIN", 0},                               // KN-K: posizione morta
    };
    std::cout << "=== Regressioni (budget 10^6 nodi, TB " << (tbMax ? "attive" : "spente") << ")\n";
    WinTree::ProofTable pt(hashLog2);
    for (const Case& c : cases) {
        Game g; std::string err;
        if (!g.set(c.fen, {}, err)) { expect(false, std::string(c.fen) + ": " + err); continue; }
        pt.clear();
        int depth = c.mode == Goal::WIN ? 245 : 4096;
        Outcome o = solve(g.pos, c.goal, c.mode, 1000000, depth, tbMax, pt);
        WinTree::Certificate cert = verify(g.pos, c.goal, o.r, tbMax, pt, depth);
        std::string got = describe(o.r);
        std::cout << c.fen << " goal " << (c.goal == WHITE ? "white" : "black") << " modo " << (c.mode == Goal::WIN ? "win" : "nowin") << ": " << got
                  << ", nodi " << o.nodes << ", " << describe(cert) << "\n";
        expect(got.rfind(c.expect, 0) == 0 && (tbMax || o.r.d >= c.dtm),      // con le TB le foglie accorciano l'albero
               std::string("atteso ") + c.expect + (c.dtm && !tbMax ? " con d >= " + std::to_string(c.dtm) : ""));
        expect(cert.ok, "certificato dal verificatore");
    }
}

// G0 scacchistico (§9.1, §7.6): posizioni casuali a 3-5 pezzi risolte senza TB (WIN mode con ID, poi NOWIN mode
// se irrisolte, stesso budget) e confrontate con probe_wdl: WIN <=> WDL = 2 per il goal, NOWIN <=> WDL <= 1.
// Ogni PROVEN passa dal verificatore. `threads` fette indipendenti (seme derivato da seed e indice della fetta).
enum Stat { S_WIN, S_NOWIN, S_CYCLE, S_CYCLE_CERT, S_BUDGET, S_DISAGREE, S_TBFAIL, S_VIA_NOWIN, S_ROOT_TERMINAL,
            S_CERT_WIN, S_CERT_WIN_MISSING, S_CERT_WIN_FAIL, S_CERT_DRAW, S_CERT_DRAW_NOMOVE, S_CERT_DRAW_BAD, S_N };
struct Stats { long n[S_N] = {}; std::vector<u64> nodesAll, nodesSolved; std::string log; };

Stats g0_slice(int count, u64 seed, u64 budget, int hashLog2) {
    using WinTree::Status;
    std::mt19937_64 rng(seed);
    WinTree::ProofTable pt(hashLog2);
    Stats s;
    for (int i = 0; i < count; i++) {
        Game g; std::string err, fen;
        do fen = random_fen(rng, 3 + int(rng() % 3)); while (!g.set(fen, {}, err));
        Color goal = rng() & 1 ? WHITE : BLACK;
        std::string tag = fen + " goal " + (goal == WHITE ? "white" : "black");
        pt.clear();
        Outcome o = solve(g.pos, goal, WinTree::Goal::WIN, budget, 245, 0, pt);
        if (!o.r.proven()) { Outcome o2 = solve(g.pos, goal, WinTree::Goal::NOWIN, budget, 4096, 0, pt); o2.nodes += o.nodes; o = o2; s.n[S_VIA_NOWIN] += o.r.proven(); }
        s.nodesAll.push_back(o.nodes);
        Tablebases::ProbeState ps; int wdl = Tablebases::probe_wdl(g.pos, &ps);
        if (g.pos.side_to_move() != goal) wdl = -wdl;
        if (o.r.status == Status::UNKNOWN_CYCLE) {      // radice completa (I2): candidato di patta, certificato solo se §7.9.2 passa
            s.n[S_CYCLE]++;
            WinTree::Certificate c = verify(g.pos, goal, o.r, 0, pt, 4096);
            if (!c.ok) continue;
            s.n[S_CYCLE_CERT]++;
            if (ps != Tablebases::FAIL && wdl == Tablebases::WDLWin) { s.n[S_DISAGREE]++; s.log += "  DISACCORDO " + tag + ": strategia di patta certificata ma WDL(goal) = 2\n"; }
            continue;
        }
        if (!o.r.proven()) { s.n[S_BUDGET]++; continue; }
        s.nodesSolved.push_back(o.nodes);
        bool isWin = o.r.status == Status::PROVEN_WIN;
        s.n[isWin ? S_WIN : S_NOWIN]++;
        s.n[S_ROOT_TERMINAL] += o.nodes == 1;
        if (ps == Tablebases::FAIL) s.n[S_TBFAIL]++;
        else if (isWin != (wdl == Tablebases::WDLWin)) { s.n[S_DISAGREE]++; s.log += "  DISACCORDO " + tag + ": " + describe(o.r) + " ma WDL(goal) = " + std::to_string(wdl) + "\n"; }
        WinTree::Certificate c = verify(g.pos, goal, o.r, 0, pt, isWin ? 245 : 4096);
        if (isWin) {
            if (c.ok) s.n[S_CERT_WIN]++;
            else if (c.why.rfind("entry mancante", 0) == 0) s.n[S_CERT_WIN_MISSING]++;
            else { s.n[S_CERT_WIN_FAIL]++; s.log += "  ALBERO VINCENTE NON VERIFICATO " + tag + ": " + c.why + "\n"; }
        } else {
            if (c.ok) s.n[S_CERT_DRAW]++;
            else if (c.why.rfind("nessuna mossa", 0) == 0) s.n[S_CERT_DRAW_NOMOVE]++;
            else { s.n[S_CERT_DRAW_BAD]++; s.log += "  STRATEGIA DI PATTA NON VERIFICATA " + tag + ": " + c.why + "\n"; }
        }
    }
    return s;
}

void g0(int count, u64 seed, u64 budget, int hashLog2, int threads) {
    auto t0 = WinTree::Clock::now();
    std::vector<Stats> parts(threads); std::vector<std::thread> pool;
    for (int t = 0; t < threads; t++)
        pool.emplace_back([&, t] { parts[t] = g0_slice(count / threads + (t < count % threads), seed * 8191 + t, budget, hashLog2); });
    for (auto& th : pool) th.join();
    Stats s;
    for (const Stats& p : parts) {
        for (int k = 0; k < S_N; k++) s.n[k] += p.n[k];
        s.nodesAll.insert(s.nodesAll.end(), p.nodesAll.begin(), p.nodesAll.end());
        s.nodesSolved.insert(s.nodesSolved.end(), p.nodesSolved.begin(), p.nodesSolved.end());
        std::cout << p.log;
    }
    double secs = std::chrono::duration<double>(WinTree::Clock::now() - t0).count();
    auto median = [](std::vector<u64>& v) { if (v.empty()) return u64(0); std::sort(v.begin(), v.end()); return v[v.size() / 2]; };
    long* n = s.n; long solved = n[S_WIN] + n[S_NOWIN];
    std::cout << "G0 scacchi: " << count << " posizioni (seme " << seed << ", " << threads << " thread, 3-5 pezzi), budget " << budget
              << " nodi per ricerca, prover senza TB, oracolo WDL 5-man\n"
              << "  risolte " << solved << "/" << count << " (" << 100.0 * solved / count << "%): PROVEN_WIN " << n[S_WIN] << ", PROVEN_NOWIN " << n[S_NOWIN]
              << " (di cui " << n[S_VIA_NOWIN] << " dalla ricerca NOWIN, " << n[S_ROOT_TERMINAL] << " radici terminali); UNKNOWN_CYCLE " << n[S_CYCLE]
              << " (strategia di patta certificata " << n[S_CYCLE_CERT] << "), UNKNOWN_BUDGET " << n[S_BUDGET] << "\n"
              << "  disaccordi con WDL (PROVEN e patte certificate): " << n[S_DISAGREE] << " (oracolo FAIL: " << n[S_TBFAIL] << ")\n"
              << "  nodi mediani: tutte " << median(s.nodesAll) << ", risolte " << median(s.nodesSolved) << "; tempo " << secs << " s\n"
              << "  certificati: alberi vincenti " << n[S_CERT_WIN] << "/" << n[S_WIN] << " (entry mancante " << n[S_CERT_WIN_MISSING] << ", falliti " << n[S_CERT_WIN_FAIL]
              << "), strategie di patta " << n[S_CERT_DRAW] << "/" << n[S_NOWIN] << " (senza mossa del difensore: sink 50 mosse/ripetizione " << n[S_CERT_DRAW_NOMOVE]
              << ", falliti " << n[S_CERT_DRAW_BAD] << ")\n";
    expect(n[S_DISAGREE] == 0, "0 disaccordi tra PROVEN (e patte certificate) e WDL");
    expect(n[S_CERT_WIN_FAIL] == 0, "ogni albero vincente estratto passa il verificatore");
    expect(n[S_CERT_DRAW_BAD] == 0, "nessuna strategia di patta con terminale vinto dal goal");
}

int usage() {
    std::cerr << "uso: wintree-chess --fen \"<fen>\" [--moves m1 m2 ...] [--goal white|black] [--mode win|nowin] [--nodes N] [--depth D]\n"
                 "                   [--tb PATH] [--tbmax K] [--hash MiB] [--verify]\n"
                 "     wintree-chess --regress [--tb PATH] [--hash MiB]\n"
                 "     wintree-chess --g0 N [--seed S] [--nodes N] [--threads T] --tb PATH [--hash MiB]\n";
    return 2;
}

}  // namespace

int main(int argc, char** argv) {
    std::string fen = StartFEN, tb; std::vector<std::string> moves;
    int goal = -1, depth = 0, tbmax = -1, hashMiB = 64, g0n = 0, threads = int(std::thread::hardware_concurrency()); u64 nodes = 1000000, seed = 1;
    WinTree::Goal mode = WinTree::Goal::WIN; bool doVerify = false, doRegress = false;
    for (int i = 1; i < argc; i++) {
        std::string a = argv[i];
        auto next = [&]() -> const char* { return i + 1 < argc ? argv[++i] : (std::exit(usage()), nullptr); };
        if (a == "--fen") fen = next();
        else if (a == "--moves") while (i + 1 < argc && argv[i + 1][0] != '-') moves.push_back(argv[++i]);
        else if (a == "--goal") { std::string s = next(); goal = s == "white" ? WHITE : s == "black" ? BLACK : (std::exit(usage()), 0); }
        else if (a == "--mode") { std::string s = next(); mode = s == "win" ? WinTree::Goal::WIN : s == "nowin" ? WinTree::Goal::NOWIN : (std::exit(usage()), WinTree::Goal::WIN); }
        else if (a == "--nodes") nodes = std::strtoull(next(), nullptr, 10);
        else if (a == "--depth") depth = std::atoi(next());
        else if (a == "--tb") tb = next();
        else if (a == "--tbmax") tbmax = std::atoi(next());
        else if (a == "--hash") hashMiB = std::atoi(next());
        else if (a == "--verify") doVerify = true;
        else if (a == "--regress") doRegress = true;
        else if (a == "--g0") g0n = std::atoi(next());
        else if (a == "--seed") seed = std::strtoull(next(), nullptr, 10);
        else if (a == "--threads") threads = std::atoi(next());
        else return usage();
    }
    Attacks::init(); Position::init();
    if (!tb.empty()) Tablebases::init(tb);
    if (tbmax < 0) tbmax = Tablebases::MaxCardinality;
    int hashLog2 = 0; while ((u64(16) << hashLog2) < u64(std::max(hashMiB, 1)) << 20) hashLog2++;   // entry da 16 byte
    if (doRegress) regress(tbmax, hashLog2);
    if (g0n) {
        if (Tablebases::MaxCardinality < 5) { std::cerr << "--g0 richiede --tb con le tablebase 5-man\n"; return 2; }
        g0(g0n, seed, nodes, hashLog2, std::max(threads, 1));
    }
    if (doRegress || g0n) { std::cout << (failures ? "FALLITI: " + std::to_string(failures) : std::string("tutto OK")) << "\n"; return failures ? 1 : 0; }

    Game g; std::string err;
    if (!g.set(fen, moves, err)) { std::cerr << "posizione non valida: " << err << "\n"; return 2; }
    Color goalSide = goal < 0 ? g.pos.side_to_move() : Color(goal);
    if (!depth) depth = mode == WinTree::Goal::WIN ? 245 : 4096;
    WinTree::ProofTable pt(hashLog2);
    Outcome o = solve(g.pos, goalSide, mode, nodes, depth, tbmax, pt);
    std::cout << "posizione " << g.pos.fen() << "\ngoal " << (goalSide == WHITE ? "white" : "black") << " modo " << (mode == WinTree::Goal::WIN ? "win" : "nowin")
              << " TB " << (tbmax ? std::to_string(tbmax) + " pezzi" : "spente") << "\nesito " << describe(o.r) << "\nnodi " << o.nodes << " iterazioni " << o.iters
              << " profondità " << o.depth << " tempo " << o.secs << " s\n";
    if (doVerify) std::cout << "verifica " << describe(verify(g.pos, goalSide, o.r, tbmax, pt, depth)) << "\n";
    return 0;
}
