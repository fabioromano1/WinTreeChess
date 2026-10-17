/*
  Stockfish, a UCI chess playing engine derived from Glaurung 2.1
  Copyright (C) 2004-2026 The Stockfish developers (see AUTHORS file)

  Stockfish is free software: you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 3 of the License, or
  (at your option) any later version.

  Stockfish is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

// WinTree × Stockfish, M3: stato del prover (H1/H7), probe in-search (H3) e Search::Worker::prove_root() (H2 con
// H4/H5 via rootProof, H6, H8). Nucleo df-pn, adapter scacchi e verificatore inclusi dai percorsi del repository.

#include "prover.h"

#include <algorithm>
#include <chrono>
#include <memory>
#include <string>

#include "misc.h"
#include "movegen.h"
#include "position.h"
#include "search.h"
#include "syzygy/tbprobe.h"
#include "thread.h"
#include "uci.h"

#include "../wintree/dfpn.h"
#include "../wintree-chess/chess_game.h"
#include "../wintree-chess/verifier.h"

namespace Stockfish {
namespace Prover {

Mode mode           = OFF;
int  probeDepth     = 6;
int  budgetPermille = 100;
int  maxNodes       = 1000000;

namespace {

int                                  hashMiB = 16;
std::unique_ptr<WinTree::ProofTable> table;  // allocata al primo uso (set_mode != OFF)

void ensure_table() {
    if (table)
        return;
    unsigned log2Entries = 16;  // 1 MiB = 2^16 entry da 16 byte; potenza di due per difetto
    while ((2 << (log2Entries - 16)) <= hashMiB)
        ++log2Entries;
    table = std::make_unique<WinTree::ProofTable>(log2Entries);
}

// Cammino principale dell'albero di prova (H4): ai nodi del goal la mossa memorizzata, ai nodi avversari la risposta
// con d massimo (la resistenza più lunga); si ferma a un terminale o a un'entry mancante.
std::vector<Move>
principal_pv(Position& pos, Color goal, const WinTree::ProofTable& pt, int tbMax, int maxLen) {
    using namespace WinTree;
    std::vector<Move>      pv;
    std::vector<StateInfo> st(maxLen + 1);
    auto entry = [&](Result& e) {
        u64 data = probe_for(pt, pos.state()->key, goal, true);
        u32 h;
        if (!same_goal(data, goal) || !ProofTable::proven(data))
            return false;
        e = ProofTable::unpack(data, h);
        return e.status == Status::PROVEN_WIN;
    };
    while (int(pv.size()) < maxLen)
    {
        MoveList<LEGAL> ml(pos);
        if (classify(pos, goal, tbMax, unsigned(ml.size())).kind != Kind::NONE)
            break;
        Move   best = Move::none();
        Result e;
        if (pos.side_to_move() == goal)
        {
            if (!entry(e) || !ml.contains(Move(e.move)))
                break;
            best = Move(e.move);
        }
        else
        {
            int bestD = -1;
            for (Move m : ml)
            {
                pos.do_move(m, st[pv.size()]);
                bool ok = entry(e);
                pos.undo_move(m);
                if (ok && int(e.d) > bestD)
                    bestD = e.d, best = m;
            }
            if (!best)
                break;
        }
        pos.do_move(best, st[pv.size()]);
        pv.push_back(best);
    }
    for (auto it = pv.rbegin(); it != pv.rend(); ++it)
        pos.undo_move(*it);
    return pv;
}

}  // namespace

void set_mode(Mode m) {
    mode = m;
    if (m != OFF)
        ensure_table();
}

void set_hash(int mib) {
    hashMiB = mib;
    if (table)
    {
        table.reset();
        ensure_table();
    }
}

void clear() {
    if (table)
        table->clear();
}

// §7.8: PROVEN_WIN(g, d) vale mate_in(ply + d) (bound LOWER) per g al tratto e mated_in (UPPER) per l'avversario;
// con foglie TB resta nella banda TB (VALUE_TB - (ply + d)); PROVEN_NOWIN(g) vale VALUE_DRAW, UPPER per g al tratto
// e LOWER per l'avversario. Entry valide per il contatore delle 50 mosse (§7.5); sola entry base (niente ombra).
int probe(const Position& pos, int ply, Hit out[2]) {
    using namespace WinTree;
    int n = 0;
    for (int goal = 0; goal < 2; ++goal)
    {
        u64 data = table->probe(pos.state()->key, goal);
        if (!data || !ProofTable::proven(data))
            continue;
        u32    h;
        Result e   = ProofTable::unpack(data, h);
        bool   win = e.status == Status::PROVEN_WIN;
        if (win ? pos.rule50_count() + e.rev > 100 : pos.rule50_count() < e.req)
            continue;
        bool stmIsGoal = (pos.side_to_move() == BLACK) == bool(goal);
        if (win)
        {
            int   m = ply + e.d;
            Value v = !e.tbLeaf && m <= MAX_PLY - 1 ? mate_in(m) : VALUE_TB - std::min(m, MAX_PLY);
            out[n++] = stmIsGoal ? Hit{v, BOUND_LOWER} : Hit{-v, BOUND_UPPER};
        }
        else
            out[n++] = {VALUE_DRAW, stmIsGoal ? BOUND_UPPER : BOUND_LOWER};
    }
    return n;
}

}  // namespace Prover

// H2 (§6.2, §8.5): trigger e prover sincrono alla radice sul main thread, con gli helper ancora fermi. Scrive
// rootProof (H4/H5) e il proofRank in tbRank, somma le do_move del prover a `nodes` (H6), stampa solo esiti
// certificati dal verificatore di §7.9 (H8).
void Search::Worker::prove_root() {
    SearchManager* mm = main_manager();
    mm->rootProof     = {};

    if (Prover::mode == Prover::OFF || Prover::maxNodes <= 0 || tbConfig.rootInTB)
        return;

    const Color us      = rootPos.side_to_move();
    const Value prev    = mm->bestPreviousScore;  // VALUE_INFINITE alla prima mossa della partita
    const int   tbLimit = std::min(int(options["SyzygyProbeLimit"]), Tablebases::MaxCardinality);

    Color goal = us;
    if (Prover::mode == Prover::DRAW)
        goal = ~us;
    else if (Prover::mode == Prover::AUTO)
    {
        bool decisive = prev != VALUE_INFINITE && is_decisive(prev);
        bool few      = rootPos.count<ALL_PIECES>() <= tbLimit + 2;
        if (!limits.mate && !decisive && !few)
            return;
        if (!limits.mate && prev != VALUE_INFINITE && prev < 0)
            goal = ~us;  // in svantaggio si cerca la patta certificata, NOWIN(opp) (§6.2)
    }

    // Budget (§6.2, §8.4): min(ProverBudget‰ · optimum, ProverMaxNodes do_move); con movetime la stessa quota di
    // movetime; senza orologio solo il tetto in do_move (riproducibile con go nodes/depth). Stop UCI cooperativo.
    // shortcut: sotto 10 ms di quota il prover non parte (i costi fissi superano il budget), da rivedere con G3.
    WinTree::Budget b;
    b.maxMoves      = WinTree::u64(Prover::maxNodes);
    b.stop          = &threads.stop;
    const bool clock = limits.use_time_management() || limits.movetime;
    TimePoint  share = limits.use_time_management() ? mm->tm.optimum() : limits.movetime;
    if (clock)
    {
        share = share * Prover::budgetPermille / 1000;
        if (share < 10)
            return;
        b.deadline = WinTree::Clock::now() + std::chrono::milliseconds(share);
    }
    const bool winMode = goal == us;
    b.maxDepth         = Prover::mode == Prover::DRAW ? 4096 : 245;  // tetto profondo solo in analisi di patta (§8.1b)
    const int tbMax = int(options["Syzygy50MoveRule"]) ? tbLimit : 0;  // §7.6: i sink TB assumono le 50 mosse

    WinTree::ChessGame g(rootPos, goal, tbMax, b.maxDepth, WinTree::Order::WARM, {&tt, this});
    WinTree::DfPn<WinTree::ChessGame> dfpn(g, *Prover::table, goal == BLACK,
                                           winMode ? WinTree::Goal::WIN : WinTree::Goal::NOWIN);
    const TimePoint t0 = now();
    WinTree::Result r  = dfpn.solve(b);
    nodes += g.moves;  // H6
    mm->callsCnt = 0;  // controllo del tempo al prossimo nodo, come dopo una sonda TB

    if (r.status == WinTree::Status::UNKNOWN_BUDGET)
        return;
    const bool win = r.status == WinTree::Status::PROVEN_WIN;
    // Con un orologio si certifica solo ciò che cambia la mossa (PROVEN_WIN(us), H4/H5): il replay del verificatore
    // non ha budget e si ripeterebbe a ogni mossa; gli altri esiti restano bound search-grade tramite i probe (§7.8).
    if (clock && !(win && winMode))
        return;
    WinTree::Certificate c =
      win ? WinTree::WinTreeVerifier(rootPos, goal, tbMax, *Prover::table, b.maxDepth).run()
          : WinTree::verify_draw_strategy(rootPos.fen(), goal, tbMax, *Prover::table);
    if (!c.ok)
        return;

    const bool  tbLeaf = c.tbLeaves > 0;
    std::string msg;
    if (!win)
        msg = winMode ? "proven draw-or-worse" : "proven draw-or-better";
    else if (!winMode)
        msg = tbLeaf ? "proven loss (tablebase leaves)"
                     : "proven mated in " + std::to_string((c.depth + 1) / 2);
    else
    {
        Move m = Move(r.move);
        msg    = tbLeaf ? "proven win (tablebase leaves)"
                        : "proven mate in " + std::to_string((c.depth + 1) / 2);
        if (m)
        {
            msg += " move " + UCIEngine::move(m, rootPos.is_chess960());
            mm->rootProof = {true, c.depth, tbLeaf ? VALUE_TB - c.depth : mate_in(c.depth), m,
                             Prover::principal_pv(rootPos, goal, *Prover::table, tbMax, c.depth)};
            // proofRank (H2): la mossa provata in testa in ogni worker, come il tbRank di tbprobe.cpp; rootInTB resta false
            for (auto&& th : threads)
            {
                RootMoves& rms = th->worker->rootMoves;
                for (RootMove& rm : rms)
                    rm.tbRank = rm == m;
                std::stable_sort(rms.begin(), rms.end(), [](const RootMove& x, const RootMove& y) {
                    return x.tbRank > y.tbRank;
                });
            }
        }
    }
    sync_cout << "info string prover: " << msg << " nodes " << g.moves << " time " << (now() - t0)
              << " ms" << sync_endl;
}

}  // namespace Stockfish
