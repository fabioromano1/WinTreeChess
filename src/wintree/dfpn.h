// WinTree: nucleo di produzione con attraversamento df-pn (spec §8.1b). C++17, nessuna dipendenza.
// MID di Nagai (2002) con soglie proof/disproof e 1+ε (Pawlewicz & Lew 2007) sulla stessa interfaccia G, la stessa
// ProofTable, gli stessi esiti (§7.1), rev/req (§7.5), d (Thm 3.6: profondità dell'albero di prova trovato) e budget
// di wintree.h. Una sola tabella: l'entry PT di un nodo irrisolto porta (pn, dn) al posto dei campi dejavu; i PROVEN
// sono protetti nel rimpiazzo (ProofTable::store). Taint di ciclo (semantica di WinTree, I1/I3): un figlio che ripete
// una posizione del cammino vale pn = ∞, dn = 0 per la selezione, ma la confutazione che ne deriva è "tainted": un
// nodo OR confutato è tainted se lo è una confutazione dei figli; un nodo AND sceglie una confutazione pulita se
// esiste, altrimenti è tainted. Le confutazioni tainted restano in tabella come UNKNOWN_CYCLE (pn = ∞, dn = 0 per la
// selezione), mai come PROVEN_NOWIN; le prove (pn = 0) non usano mai ripetizioni né figli tainted. Tetto di
// profondità (245 in WIN, 4096 in NOWIN): confutazione tainted. Taglio di budget (nodi, do_move, tempo, stop):
// nessuna scrittura, UNKNOWN_BUDGET con cut alla radice (I2).
#ifndef WINTREE_DFPN_H
#define WINTREE_DFPN_H

#include "wintree.h"

#include <algorithm>

namespace WinTree {

template<class G>
class DfPn {
    using Move = typename G::Move;
    static constexpr int MAX_CHILDREN = 256;
    static constexpr u64 CHECK_EVERY = 1024;
    static constexpr u64 INF = u64(1) << 62;                       // infinito vero: prova (dn) o confutazione (pn)
    static constexpr u64 SAT = u64((u32(1) << 25) - 1) << 31;     // massimo finito codificabile in tabella (~2^56)

    // Codifica a 30 bit di un numero finito (5 bit di esponente, 25 di mantissa), arrotondata per eccesso: con un
    // arrotondamento per difetto il progresso di un figlio potrebbe non essere visibile al padre, che lo
    // risceglierebbe senza fine; per eccesso il padre vede sempre un valore >= alla soglia data al figlio. Esatta
    // sotto 2^25. Le somme in memoria saturano a SAT, così un valore memorizzato è sempre esatto o >= al vero;
    // INF resta riservato alle prove e confutazioni (in M2b la saturazione a 2^30 chiudeva la radice come ignota).
    static u32 enc(u64 x) {
        if (x < (u64(1) << 25)) return u32(x);
        int e = 64 - __builtin_clzll(x) - 25;
        u64 m = (x + ((u64(1) << e) - 1)) >> e;
        if (m >> 25) { m >>= 1; e++; }
        return e > 31 ? ~u32(0) >> 2 : u32(e) << 25 | u32(m);
    }
    static u64 dec(u32 c) { return u64(c & ((1u << 25) - 1)) << (c >> 25); }
    static u64 add(u64 a, u64 b) { return a == INF || b == INF ? INF : std::min(a + b, SAT); }
    // Entry di un nodo irrisolto: status UNKNOWN, kind 0, pn (30 bit) in 54..25, dn (30 bit) in 60..56 + 24..0.
    static u64 pack_open(u64 pn, u64 dn, u8 goal) {
        u32 p = enc(pn), d = enc(dn);
        return u64(1) << 62 | u64(goal & 1) << 61 | u64(d >> 25) << 56 | u64(p) << 25 | u64(d & ((1u << 25) - 1));
    }

    struct Node { u64 pn = 1, dn = 1; bool tainted = false, fresh = false, invalid = false, proven = false; u8 rr = 0; u16 d = 0, move = 0; };

    G& g; ProofTable& pt; u8 goal; Goal mode; double eps;
    Budget b; u64 nodes_ = 0, moves_ = 0, andNodes_ = 0, plySum_ = 0; bool aborted_ = false; int maxPly_ = 0;
    std::vector<u64> path;                     // chiavi del cammino corrente (ripetizioni, §7.3)

    // Lettura di un'entry della posizione corrente: PROVEN valido per il contatore (§7.5) => (0, ∞) o (∞, 0) con
    // rev/req, d e mossa; PROVEN fuori intervallo => `invalid` (irrisolto da ricercare); UNKNOWN_CYCLE => confutazione
    // tainted; irrisolto => (pn, dn) memorizzati; assente => fresh (pn = dn = 1, poi G::init sui figli).
    Node look(u64 k) const {
        Node n; u64 data = pt.probe(k, goal & 1);
        if (data && (data >> 61 & 1) == (goal & 1)) {
            if (ProofTable::proven(data)) {
                u32 h; Result e = ProofTable::unpack(data, h);
                bool win = e.status == Status::PROVEN_WIN;
                n.proven = true;
                if (win ? g.rule50() + e.rev <= 100 : g.rule50() >= e.req) {
                    n.pn = win ? 0 : INF; n.dn = win ? INF : 0; n.rr = win ? e.rev : e.req; n.d = e.d; n.move = e.move;
                    return n;
                }
                n.invalid = true; return n;
            } else if (data >> 55 & 1) { n.pn = INF; n.dn = 0; n.tainted = true; return n; }
            else {
                n.pn = dec(u32(data >> 25) & ((1u << 30) - 1));
                n.dn = dec(u32(data >> 56 & 31) << 25 | u32(data & ((1u << 25) - 1)));
                return n;
            }
        }
        n.fresh = true;
        return n;
    }
    // Base, poi ombra (ProofTable::SHADOW) se la base è un PROVEN fuori intervallo; un'ombra a sua volta fuori
    // intervallo vale come assente (la riprova la sovrascrive: terzo regime del contatore, raro).
    Node lookup(u64 k) const {
        Node n = look(k);
        if (n.invalid) { n = look(k ^ ProofTable::SHADOW); if (n.invalid) { n = Node(); n.fresh = true; } }
        return n;
    }
    Node child(Move m, int ply, int i) {
        g.do_move(m, ply); moves_++;
        u64 kc = g.key(); Node c;
        bool rep = false;
        for (int q = 0; q <= ply && !rep; q++) rep = path[q] == kc;
        if (rep) { c.pn = INF; c.dn = 0; c.tainted = true; }           // ripetizione: sconfitta per la selezione, tainted
        else { c = lookup(kc); if (c.fresh) g.init(ply, i, c.pn, c.dn); }
        g.undo_move(m, ply);
        return c;
    }

    void mid(int ply, u64 thpn, u64 thdn) {
        if (nodes_ >= b.maxNodes || moves_ >= b.maxMoves || (nodes_ % CHECK_EVERY == 0 && nodes_ > 0
            && ((b.stop && b.stop->load(std::memory_order_relaxed)) || Clock::now() > b.deadline))) { aborted_ = true; return; }
        nodes_++; plySum_ += ply; if (ply > maxPly_) maxPly_ = ply;
        u64 k = g.key();
        // Riprova di un nodo PROVEN fuori intervallo (§7.5): l'esito (anche irrisolto, altrimenti il padre lo rilegge
        // invariato e lo risceglie senza progresso) va nell'entry ombra, che si sovrascrive se a sua volta PROVEN; la
        // base resta per gli altri contatori. Un PROVEN dello stesso genere ha intervallo più ampio e aggiorna anche la base.
        Node own = look(k); u64 sk = k; bool force = false;
        if (own.invalid) { sk = k ^ ProofTable::SHADOW; force = look(sk).proven; }
        auto put = [&](u64 data) { pt.store(sk, data, force); if (own.invalid && ProofTable::proven(data) && (data >> 62) == (pt.probe(k, goal & 1) >> 62)) pt.store(k, data); };
        Terminal t = g.terminal();
        if (t != Terminal::NONE) {
            bool tw = t == Terminal::WIN_GOAL || t == Terminal::WIN_GOAL_C0;
            u8 r50 = t == Terminal::WIN_GOAL_C0 || t == Terminal::NOWIN_GOAL_C100 ? 100 : 0;
            put(ProofTable::pack_proven(tw, 0, goal, r50, 0));
            return;
        }
        if (ply >= b.maxDepth) { put(ProofTable::pack_unknown(true, 0, -1, 0, goal)); return; }   // tetto: confutazione tainted
        path[ply] = k;
        Span<Move> ch = g.gen_children(ply);
        WT_ASSERT(ch.n > 0 && ch.n <= MAX_CHILDREN);
        bool orNode = g.side_is_goal();
        andNodes_ += !orNode;
        u64 pn, dn;
        for (;;) {
            u64 spn = 0, sdn = 0, m1 = INF, m2 = INF; int best = 0; Node cb;   // somme, minimo e secondo minimo
            for (int i = 0; i < ch.n; i++) {
                Node c = child(ch.first[i], ply, i);
                spn = add(spn, c.pn); sdn = add(sdn, c.dn);
                u64 v = orNode ? c.pn : c.dn;
                if (v < m1) { m2 = m1; m1 = v; best = i; cb = c; } else if (v < m2) m2 = v;   // a parità vince l'ordine (§8.4)
            }
            pn = orNode ? m1 : spn; dn = orNode ? sdn : m1;
            if (pn == 0 || dn == 0 || pn >= thpn || dn >= thdn) break;
            u64 th = std::max(m2 + 1, u64(double(m2) * (1 + eps)));            // soglia 1+ε sul secondo migliore
            u64 cpn = orNode ? std::min(thpn, th) : std::min(INF, thpn - pn + cb.pn);
            u64 cdn = orNode ? std::min(INF, thdn - dn + cb.dn) : std::min(thdn, th);
            g.do_move(ch.first[best], ply); moves_++;
            mid(ply + 1, cpn, cdn);
            g.undo_move(ch.first[best], ply);
            if (aborted_) return;
        }
        if (pn != 0 && dn != 0) { put(pack_open(pn, dn, goal)); return; }
        // Risolto: testimone, rev/req (§7.5), d (Thm 3.6) e taint dai figli (stessa tabella dell'ultima scansione).
        bool win = pn == 0, tainted = false, clean = false; u16 mv = 0, d = 0; u8 rr = 0;
        for (int i = 0; i < ch.n; i++) {
            Move m = ch.first[i]; bool rv = g.reversible(m);
            Node c = child(m, ply, i);
            if (win) {
                if (c.pn != 0) { WT_ASSERT(orNode); continue; }                  // AND provato: tutti i figli provati
                rr = std::max<int>(rr, rv ? std::min(127, c.rr + 1) : 1); d = u16(std::max<int>(d, std::min(255, c.d + 1)));
                if (orNode) { mv = G::code(m); break; }
            } else {
                if (c.dn != 0) { WT_ASSERT(!orNode); continue; }                 // OR confutato: tutti i figli confutati
                u8 cr = rv && c.rr ? u8(c.rr - 1) : 0;
                if (orNode) { tainted |= c.tainted; rr = std::max(rr, cr); }
                else if (!c.tainted) { rr = cr; mv = G::code(m); clean = true; break; }
            }
        }
        if (!win && !orNode && !clean) tainted = true;
        WT_ASSERT(!(win && tainted));                                              // I1: una prova non usa ripetizioni
        put(tainted ? ProofTable::pack_unknown(true, 0, -1, 0, goal) : ProofTable::pack_proven(win, d, goal, rr, mv));
    }

  public:
    DfPn(G& game, ProofTable& table, u8 goalSide, Goal m, double epsilon = 0.25) : g(game), pt(table), goal(goalSide), mode(m), eps(epsilon) {}

    Result solve(const Budget& budget) {
        b = budget; nodes_ = moves_ = andNodes_ = plySum_ = 0; aborted_ = false; maxPly_ = 0;
        int cap = mode == Goal::WIN ? 245 : 4096;        // §8.1: MAX_PLY - 1 in WIN; memoria (stack) in NOWIN
        if (b.maxDepth > cap) b.maxDepth = cap;
        path.assign(b.maxDepth + 2, 0);
        u64 k = g.key();
        Node n = lookup(k);
        if (n.pn != 0 && n.dn != 0) { mid(0, INF, INF); n = lookup(k); }
        Result r;
        if (n.pn == 0) { r.status = Status::PROVEN_WIN; r.d = n.d; r.rev = n.rr; r.move = n.move; }
        else if (n.dn == 0 && !n.tainted) { r.status = Status::PROVEN_NOWIN; r.req = n.rr; r.move = n.move; }
        else if (n.dn == 0) r.status = Status::UNKNOWN_CYCLE;
        else { r.status = Status::UNKNOWN_BUDGET; r.cut = true; WT_ASSERT(aborted_); }   // la radice esce solo risolta o interrotta
        return r;
    }
    u64 nodes() const { return nodes_; }                  // espansioni (chiamate di mid)
    u64 moves() const { return moves_; }                  // do_move (lettura dei figli compresa)
    bool aborted() const { return aborted_; }
    u64 and_nodes() const { return andNodes_; }
    double mean_ply() const { return nodes_ ? double(plySum_) / nodes_ : 0; }
    int max_ply() const { return maxPly_; }
};

}  // namespace WinTree

#endif
