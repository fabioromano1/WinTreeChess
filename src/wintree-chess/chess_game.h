// Adapter scacchi per il nucleo WinTree (spec §8.2): Position di Stockfish con uno StateInfo per ply agganciato
// alla storia della partita, chiave st->key (§7.2), terminali di §7.4, sink TB di §7.6, ordinamento "freddo" di §8.4.
#ifndef WINTREE_CHESS_GAME_H
#define WINTREE_CHESS_GAME_H

#include "../wintree/wintree.h"

#include "movegen.h"
#include "position.h"
#include "syzygy/tbprobe.h"

#include <algorithm>
#include <utility>
#include <vector>

namespace WinTree {

// Classificazione terminale di una posizione reale (§7.4), condivisa da adapter e verificatore (§7.9).
// Ordine: matto/stallo (il matto al 100° ply prevale sulle 50 mosse, position.cpp:1498), 50 mosse, triplice
// ripetizione dalla catena StateInfo (storia compresa, §7.3), posizione morta elementare, TB (§7.6: solo con
// contatore 0, senza arrocco, pezzi <= tbMax). `wdl` è dal punto di vista del goal.
enum class Kind : u8 { NONE, MATE, STALEMATE, RULE50, REPETITION, DEAD, TB };
struct Leaf { Kind kind = Kind::NONE; int wdl = 0; };

inline const char* kind_name(Kind k) {
    static const char* n[] = {"nessuno", "matto", "stallo", "50 mosse", "ripetizione", "posizione morta", "TB"};
    return n[int(k)];
}

inline Leaf classify(Stockfish::Position& pos, Stockfish::Color goal, int tbMax, unsigned nLegal) {
    using namespace Stockfish;
    if (nLegal == 0) return {pos.checkers() ? Kind::MATE : Kind::STALEMATE, 0};
    if (pos.rule50_count() > 99) return {Kind::RULE50, 0};
    if (pos.state()->repetition < 0) return {Kind::REPETITION, 0};
    int n = pos.count<ALL_PIECES>();
    if (n == 2 || (n == 3 && pos.pieces(BISHOP, KNIGHT))) return {Kind::DEAD, 0};   // FIDE 5.2.2: K-K, KB-K, KN-K
    if (tbMax > 0 && n <= tbMax && !pos.can_castle(ANY_CASTLING) && pos.rule50_count() == 0) {
        Tablebases::ProbeState s;
        int wdl = Tablebases::probe_wdl(pos, &s);
        if (s != Tablebases::FAIL) return {Kind::TB, pos.side_to_move() == goal ? wdl : -wdl};
    }
    return {};
}

class ChessGame {
    Stockfish::Position& pos;
    Stockfish::Color goal;
    int tbMax;
    std::vector<Stockfish::StateInfo> st;        // uno per ply; previous -> storia della partita
    std::vector<Stockfish::Move> buf;            // MAX_MOVES per ply
    Stockfish::Move scratch[Stockfish::MAX_MOVES]; int nScratch = 0; u64 scratchKey = 0;   // lista legale di terminal()

  public:
    using Move = Stockfish::Move;
    ChessGame(Stockfish::Position& p, Stockfish::Color g, int tbCardinality, int maxDepth) :
        pos(p), goal(g), tbMax(tbCardinality), st(maxDepth + 2), buf(size_t(maxDepth + 2) * Stockfish::MAX_MOVES) {}

    u64  key() const { return pos.state()->key; }                      // non pos.key(): niente bucket a 50 mosse
    bool side_is_goal() const { return pos.side_to_move() == goal; }
    int  rule50() const { return pos.rule50_count(); }
    bool reversible(Move m) const { return !pos.capture(m) && Stockfish::type_of(pos.moved_piece(m)) != Stockfish::PAWN; }
    static u16  code(Move m) { return m.raw(); }
    static Move move(u16 c) { return Move(c); }

    Terminal terminal() {
        nScratch = int(Stockfish::generate<Stockfish::LEGAL>(pos, scratch) - scratch); scratchKey = key();
        Leaf l = classify(pos, goal, tbMax, nScratch);
        switch (l.kind) {
        case Kind::NONE:   return Terminal::NONE;
        case Kind::MATE:   return side_is_goal() ? Terminal::NOWIN_GOAL : Terminal::WIN_GOAL;
        case Kind::RULE50: return Terminal::NOWIN_GOAL_C100;
        case Kind::TB:     return l.wdl == Stockfish::Tablebases::WDLWin ? Terminal::WIN_GOAL_C0 : Terminal::NOWIN_GOAL;
        default:           return Terminal::NOWIN_GOAL;                 // stallo, ripetizione, posizione morta
        }
    }

    // §8.4 "freddo": scacchi, catture con SEE >= 0, promozioni, quiete, catture perdenti; stabile entro la classe
    // (ordine di generazione). ttMove e storie (ordinamento a caldo) sono rinviati a M2b.
    Span<Move> gen_children(int ply) {
        using namespace Stockfish;
        int n = scratchKey == key() ? nScratch : int(generate<LEGAL>(pos, scratch) - scratch);
        std::pair<int, Move> t[MAX_MOVES];
        for (int i = 0; i < n; i++) {
            Move m = scratch[i];
            t[i] = {pos.gives_check(m) ? 4 : pos.capture(m) ? (pos.see_ge(m) ? 3 : 0) : m.type_of() == PROMOTION ? 2 : 1, m};
        }
        std::stable_sort(t, t + n, [](const std::pair<int, Move>& a, const std::pair<int, Move>& b) { return a.first > b.first; });
        Move* out = &buf[size_t(ply) * MAX_MOVES];
        for (int i = 0; i < n; i++) out[i] = t[i].second;
        return {out, n};
    }

    void do_move(Move m, int ply) { pos.do_move(m, st[ply]); }
    void undo_move(Move m, int) { pos.undo_move(m); }
};

}  // namespace WinTree

#endif
