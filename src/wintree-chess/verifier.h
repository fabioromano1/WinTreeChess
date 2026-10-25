// Verificatore di §7.9: controlla un oggetto di prova esplicito con posizioni ricostruite per replay, mai con le
// sole chiavi. La tabella serve solo a ritrovare le mosse scelte. Solo un esito "ok" autorizza "certificato" (I5, I8).
#ifndef WINTREE_CHESS_VERIFIER_H
#define WINTREE_CHESS_VERIFIER_H

#include "chess_game.h"

#include "../wintree/dfpn.h"

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
    int researches = 0;                    // grafo di patta: ri-ricerche locali di nodi del difensore senza mossa
    bool fail(const std::string& w) { if (ok) { ok = false; why = w; } return false; }
};

inline std::string fen_no_ply(const Stockfish::Position& pos) {    // posizione + contatore 50 mosse, senza numero di mossa
    std::string f = pos.fen(); return f.substr(0, f.rfind(' '));
}
inline std::string fen_no_counters(const Stockfish::Position& pos) {  // identità FIDE 9.2: pezzi, tratto, arrocco, ep
    std::string f = pos.fen(); return f.substr(0, f.rfind(' ', f.rfind(' ') - 1));
}
inline const char* status_name(Status s) { static const char* n[] = {"UNKNOWN_BUDGET", "UNKNOWN_CYCLE", "PROVEN_WIN", "PROVEN_NOWIN"}; return n[int(s)]; }
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
// Ri-ricerca locale (G2): un nodo del difensore con entry UNKNOWN_CYCLE può non avere alcun figlio PROVEN_NOWIN o
// UNKNOWN_CYCLE, perché la sua confutazione era la ripetizione di un antenato del cammino di ricerca rimasto aperto
// (pn, dn) quando il padre lo ha abbandonato: il certificato è perso, non falso. Il nodo viene allora ri-cercato con
// il nucleo df-pn nella stessa tabella (cammino che parte da lì, RESEARCH_MOVES do_move, al più MAX_RESEARCH nodi
// distinti per certificato) e la chiusura riprende; se il nodo resta senza mossa (es. è risultato PROVEN_WIN: la
// scelta a monte era sbagliata) la chiusura riparte dalla radice con le entry nuove. La solidità resta nel controllo
// di chiusura, non nelle entry.
// `chess960`: le posizioni ricostruite dal FEN ne hanno bisogno (legalità dell'arrocco e lettere dei diritti nel FEN).
inline Certificate verify_draw_strategy(const std::string& rootFen, Stockfish::Color goal, int tbMax, ProofTable& pt, bool chess960) {
    using namespace Stockfish;
    const u64 RESEARCH_MOVES = 1000000; const int MAX_RESEARCH = 64;   // G0 10^3: 224 → 278/283 cicli certificati (+2 s su 52); 10^5/16: 266
    Certificate c; Position pos; StateInfo st, st2;
    std::string root = rootFen.substr(0, rootFen.rfind(' ', rootFen.rfind(' ') - 1));
    std::deque<std::string> todo{root}; std::unordered_set<std::string> seen{root}, researched;
    auto probe = [&](Move m, Result& e) {      // entry del figlio dopo m (stesso goal), false se assente
        pos.do_move(m, st2); u64 d = probe_for(pt, pos.state()->key, goal, false); pos.undo_move(m);
        u32 h; if (same_goal(d, goal)) e = ProofTable::unpack(d, h); return same_goal(d, goal);
    };
    auto push = [&](Move m) {
        pos.do_move(m, st2); std::string k = fen_no_counters(pos); pos.undo_move(m);
        if (seen.insert(k).second) todo.push_back(k);
    };
    // Mossa del difensore: quella memorizzata se PROVEN_NOWIN; altrimenti un figlio PROVEN_NOWIN, o UNKNOWN_CYCLE da
    // un nodo UNKNOWN_CYCLE. `e` resta l'entry del nodo (per il motivo del fallimento), `present` se esiste.
    auto pick = [&](const MoveList<LEGAL>& ml, Result& e, bool& present) {
        u64 data = probe_for(pt, pos.state()->key, goal, false); u32 h; e = Result(); present = same_goal(data, goal);
        if (present) e = ProofTable::unpack(data, h);
        if (e.status == Status::PROVEN_NOWIN && e.move && ml.contains(Move(e.move))) return Move(e.move);
        Move nil = Move::none();
        if (e.status == Status::PROVEN_NOWIN || e.status == Status::UNKNOWN_CYCLE)
            for (Move m : ml) {
                Result ce;
                if (!probe(m, ce)) continue;
                if (ce.status == Status::PROVEN_NOWIN) return m;
                if (ce.status == Status::UNKNOWN_CYCLE && e.status == Status::UNKNOWN_CYCLE && !nil) nil = m;
            }
        return nil;
    };
    while (!todo.empty() && c.ok) {
        std::string cur = todo.front(); todo.pop_front(); c.nodes++;
        pos.set(cur + " 0 1", chess960, &st);
        MoveList<LEGAL> ml(pos);
        Leaf l = classify(pos, goal, tbMax, unsigned(ml.size()));
        if (l.kind == Kind::MATE) { c.leaves++; if (pos.side_to_move() != goal) c.fail("matto dato dal goal in " + pos.fen()); continue; }
        if (l.kind == Kind::STALEMATE || l.kind == Kind::DEAD) { c.leaves++; continue; }
        if (l.kind == Kind::TB) { c.leaves++; c.tbLeaves++; if (l.wdl == Tablebases::WDLWin) c.fail("TB vinta dal goal in " + pos.fen()); continue; }
        if (pos.side_to_move() == goal) { for (Move m : ml) push(m); continue; }
        Result e; bool present; Move choice = pick(ml, e, present);
        if (!choice && c.researches < MAX_RESEARCH && researched.insert(cur).second) {
            c.researches++;
            ChessGame g(pos, goal, tbMax, 4096); DfPn<ChessGame> p(g, pt, goal == BLACK, Goal::NOWIN);
            Budget b; b.maxDepth = 4096; b.maxMoves = RESEARCH_MOVES;
            p.solve(b, true);
            choice = pick(ml, e, present);
            if (!choice) { todo.assign(1, root); seen = {root}; c.nodes = c.leaves = c.tbLeaves = 0; continue; }   // scelte a monte da rifare
        }
        if (!choice) {                     // lo stato dell'entry distingue entry sostituita, sink dipendente dal contatore e taint non chiuso (G2)
            c.fail(std::string("nessuna mossa del difensore (entry ") + (!present ? "assente" : status_name(e.status)) + ") in " + pos.fen());
            break;
        }
        push(choice);
    }
    return c;
}

}  // namespace WinTree

#endif
