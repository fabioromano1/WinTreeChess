// Verificatore di §7.9: controlla un oggetto di prova esplicito con posizioni ricostruite per replay, mai con le
// sole chiavi. La tabella serve solo a ritrovare le mosse scelte. Solo un esito "ok" autorizza "certificato" (I5, I8).
#ifndef WINTREE_CHESS_VERIFIER_H
#define WINTREE_CHESS_VERIFIER_H

#include "chess_game.h"

#include <deque>
#include <string>
#include <unordered_map>
#include <unordered_set>

namespace WinTree {

struct Certificate {
    bool ok = true;
    std::string why;                       // primo errore
    u64 nodes = 0, leaves = 0, tbLeaves = 0;
    int depth = 0;                         // albero vincente: ply della foglia più lontana (= d verificato)
    bool fail(const std::string& w) { if (ok) { ok = false; why = w; } return false; }
};

inline std::string fen_no_ply(const Stockfish::Position& pos) {    // posizione + contatore 50 mosse, senza numero di mossa
    std::string f = pos.fen(); return f.substr(0, f.rfind(' '));
}
inline std::string fen_no_counters(const Stockfish::Position& pos) {  // identità FIDE 9.2: pezzi, tratto, arrocco, ep
    std::string f = pos.fen(); return f.substr(0, f.rfind(' ', f.rfind(' ') - 1));
}
inline bool same_goal(u64 data, Stockfish::Color goal) { return data && (data >> 61 & 1) == u64(goal == Stockfish::BLACK); }
// Entry di una posizione fra base e ombra (ProofTable::SHADOW, riprove a contatore diverso): quella del genere cercato
// (PROVEN_WIN per l'albero vincente; PROVEN_NOWIN o UNKNOWN_CYCLE per il grafo di patta), altrimenti la base.
inline u64 probe_for(const ProofTable& pt, u64 key, Stockfish::Color goal, bool win) {
    return pt.probe_kind(key, u8(goal == Stockfish::BLACK), win ? 1u << int(Status::PROVEN_WIN) : 1u << int(Status::PROVEN_NOWIN) | 1u << int(Status::UNKNOWN_CYCLE));
}

// Albero vincente (§7.9.1), replay dalla radice con contatore e storia reali: ai nodi del goal la mossa memorizzata
// nella tabella, ai nodi avversari tutte le mosse legali; foglie = matti dati dal goal o TB WDLWin con contatore 0.
// Memo su (posizione, contatore): un sottoalbero verificato con quel contatore resta valido lungo ogni altro
// cammino (una ripetizione sul cammino fa crescere il contatore fino alla patta per 50 mosse, quindi non sfugge;
// una triplice dalla storia non dipende dal cammino).
class WinTreeVerifier {
    Stockfish::Position& pos; Stockfish::Color goal; int tbMax; const ProofTable& pt;
    std::vector<Stockfish::StateInfo> st;
    std::unordered_map<std::string, int> memo;   // posizione+contatore -> altezza del sottoalbero verificato
    Certificate c;

    int node(int ply) {                     // altezza del sottoalbero (ply fino alla foglia più lontana), -1 = errore
        using namespace Stockfish;
        c.nodes++;
        MoveList<LEGAL> ml(pos);
        Leaf l = classify(pos, goal, tbMax, unsigned(ml.size()));
        if (l.kind != Kind::NONE) {
            c.leaves++;
            if (l.kind == Kind::MATE && pos.side_to_move() != goal) return 0;
            if (l.kind == Kind::TB) { c.tbLeaves++; if (l.wdl == Tablebases::WDLWin) return 0; }
            return c.fail(std::string("foglia non vinta dal goal (") + kind_name(l.kind) + ") in " + pos.fen()), -1;
        }
        std::string k = fen_no_ply(pos);
        auto it = memo.find(k);
        if (it != memo.end()) return it->second;
        if (ply + 1 >= int(st.size())) return c.fail("albero più profondo del tetto"), -1;
        int h = 0;
        if (pos.side_to_move() == goal) {
            u64 data = probe_for(pt, pos.state()->key, goal, true); u32 hh;
            if (!same_goal(data, goal) || !ProofTable::proven(data)) return c.fail("entry mancante in " + pos.fen()), -1;
            Result e = ProofTable::unpack(data, hh);
            if (e.status != Status::PROVEN_WIN) return c.fail("entry PROVEN_NOWIN in un albero vincente: " + pos.fen()), -1;
            Move m(e.move);
            if (!ml.contains(m)) return c.fail("mossa memorizzata illegale in " + pos.fen()), -1;
            pos.do_move(m, st[ply]); h = node(ply + 1); pos.undo_move(m);
            if (h < 0) return -1;
        } else
            for (Move m : ml) {
                pos.do_move(m, st[ply]); int hc = node(ply + 1); pos.undo_move(m);
                if (hc < 0) return -1;
                h = std::max(h, hc);
            }
        memo[k] = ++h;
        return h;
    }

  public:
    WinTreeVerifier(Stockfish::Position& p, Stockfish::Color g, int tbCardinality, const ProofTable& table, int maxDepth) :
        pos(p), goal(g), tbMax(tbCardinality), pt(table), st(maxDepth + 2) {}
    Certificate run() { c.depth = std::max(node(0), 0); return c; }
};

// Grafo di strategia di patta (§7.9.2): insieme chiuso S di posizioni (senza contatore né storia) con una mossa
// scelta a ogni nodo dell'avversario del goal. Chiusura: dai nodi del goal tutte le mosse legali portano in S o a
// terminali non vinti dal goal; dai nodi avversari la mossa scelta (memorizzata se PROVEN_NOWIN; altrimenti un
// figlio PROVEN_NOWIN, o UNKNOWN_CYCLE da un nodo UNKNOWN_CYCLE: Thm 4.8 con la precisazione di M0). Sink ammessi:
// matto subito dal goal, stallo, posizione morta, TB non vinta dal goal (valida a ogni contatore: §7.6 è monotona).
// Le 50 mosse e le ripetizioni non sono sink: se il gioco resta in S finisce comunque in patta.
inline Certificate verify_draw_strategy(const std::string& rootFen, Stockfish::Color goal, int tbMax, const ProofTable& pt) {
    using namespace Stockfish;
    Certificate c; Position pos; StateInfo st, st2;
    std::string root = rootFen.substr(0, rootFen.rfind(' ', rootFen.rfind(' ') - 1));
    std::deque<std::string> todo{root}; std::unordered_set<std::string> seen{root};
    auto probe = [&](Move m, Result& e) {      // entry del figlio dopo m (stesso goal), false se assente
        pos.do_move(m, st2); u64 d = probe_for(pt, pos.state()->key, goal, false); pos.undo_move(m);
        u32 h; if (same_goal(d, goal)) e = ProofTable::unpack(d, h); return same_goal(d, goal);
    };
    auto push = [&](Move m) {
        pos.do_move(m, st2); std::string k = fen_no_counters(pos); pos.undo_move(m);
        if (seen.insert(k).second) todo.push_back(k);
    };
    while (!todo.empty() && c.ok) {
        pos.set(todo.front() + " 0 1", false, &st); todo.pop_front(); c.nodes++;
        MoveList<LEGAL> ml(pos);
        Leaf l = classify(pos, goal, tbMax, unsigned(ml.size()));
        if (l.kind == Kind::MATE) { c.leaves++; if (pos.side_to_move() != goal) c.fail("matto dato dal goal in " + pos.fen()); continue; }
        if (l.kind == Kind::STALEMATE || l.kind == Kind::DEAD) { c.leaves++; continue; }
        if (l.kind == Kind::TB) { c.leaves++; c.tbLeaves++; if (l.wdl == Tablebases::WDLWin) c.fail("TB vinta dal goal in " + pos.fen()); continue; }
        if (pos.side_to_move() == goal) { for (Move m : ml) push(m); continue; }
        u64 data = probe_for(pt, pos.state()->key, goal, false); u32 h; Result e;
        if (same_goal(data, goal)) e = ProofTable::unpack(data, h);
        Move choice = Move::none(), nil = Move::none();
        if (e.status == Status::PROVEN_NOWIN && e.move && ml.contains(Move(e.move))) choice = Move(e.move);
        else if (e.status == Status::PROVEN_NOWIN || e.status == Status::UNKNOWN_CYCLE)
            for (Move m : ml) {
                Result ce;
                if (!probe(m, ce)) continue;
                if (ce.status == Status::PROVEN_NOWIN) { choice = m; break; }
                if (ce.status == Status::UNKNOWN_CYCLE && e.status == Status::UNKNOWN_CYCLE && !nil) nil = m;
            }
        if (!choice) choice = nil;
        if (!choice) { c.fail("nessuna mossa del difensore in " + pos.fen()); break; }
        push(choice);
    }
    return c;
}

}  // namespace WinTree

#endif
