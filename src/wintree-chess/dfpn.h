// Baseline df-pn "da manuale" (Nagai 2002; Kishimoto et al. 2012 §3.2) con il trucco 1+ε (Pawlewicz & Lew 2007)
// per il gate G1 (§11): stesso adapter, stessi terminali e stesso ordinamento di WinTree, nessun trattamento GHI
// (le coppie (pn, dn) vanno in tabella così come calcolate). Figlio che ripete una posizione del cammino:
// sconfitta dell'attaccante (pn = ∞, dn = 0), come in Nagai; l'alternativa "ignoto" (pn = dn = ∞) è disponibile
// per misurarla (repLoss = false): rende dn = ∞ in ogni nodo OR che ha quel figlio e il ciclo MID si ferma lì.
// Tetto di profondità (245 ply, come WinTree in modalità WIN): confutazione. Tabella diretta di coppie (pn, dn),
// l'ultimo scrive. I nodi risolti e la loro mossa (di prova nei nodi OR, di confutazione nei nodi AND) vanno anche
// nella ProofTable di WinTree, così il verificatore di §7.9 controlla gli esiti con lo stesso codice. Nodo = una
// chiamata di mid(); le do_move le conta l'adapter (anche quelle della lettura dei figli, come nel ciclo O1 di WinTree).
#ifndef WINTREE_CHESS_DFPN_H
#define WINTREE_CHESS_DFPN_H

#include "../wintree/wintree.h"

#include <algorithm>
#include <vector>

namespace WinTree {

template<class G>
class DfPn {
    using Move = typename G::Move;
    static constexpr u32 INF = 1u << 30;
    static constexpr int MAX_CHILDREN = 256;
    struct Entry { u64 key = 0; u32 pn = 1, dn = 1; };
    std::vector<Entry> tab; u64 mask;
    G& g; ProofTable& pt; u8 goal; double eps; bool repLoss;
    Budget b; u64 nodes_ = 0; bool aborted_ = false; int maxPly_ = 0;
    std::vector<u64> path;

    static u32 sat(u64 x) { return x >= INF ? INF : u32(x); }
    static bool solved(u32 pn, u32 dn) { return pn == 0 || dn == 0; }
    void look(u64 k, u32& pn, u32& dn) const { const Entry& e = tab[k & mask]; if (e.key == k) { pn = e.pn; dn = e.dn; } else pn = dn = 1; }
    // L'ultimo scrive, sempre: se un figlio appena visitato non potesse scrivere il suo nuovo (pn, dn), il padre lo
    // rileggerebbe invariato e lo sceglierebbe di nuovo (ciclo MID senza progresso). Un risolto rimpiazzato si
    // ridimostra; la sua mossa resta nella ProofTable, che protegge i PROVEN.
    void store(u64 k, u32 pn, u32 dn) { tab[k & mask] = {k, pn, dn}; }

    void mid(int ply, u32 thpn, u32 thdn, u32& pn, u32& dn) {
        if (nodes_ >= b.maxNodes || (nodes_ % 1024 == 0 && nodes_ > 0
            && ((b.stop && b.stop->load(std::memory_order_relaxed)) || Clock::now() > b.deadline))) { aborted_ = true; return; }
        nodes_++; if (ply > maxPly_) maxPly_ = ply;
        u64 k = g.key();
        Terminal t = g.terminal();
        if (t != Terminal::NONE) {
            bool tw = t == Terminal::WIN_GOAL || t == Terminal::WIN_GOAL_C0;
            pn = tw ? 0 : INF; dn = tw ? INF : 0;
            store(k, pn, dn); pt.store(k, ProofTable::pack_proven(tw, 0, goal, 0, 0));
            return;
        }
        if (ply >= b.maxDepth) { pn = INF; dn = 0; store(k, pn, dn); return; }   // tetto di profondità (245): confutazione, come nei risolutori di tsume
        path[ply] = k;
        Span<Move> ch = g.gen_children(ply);
        WT_ASSERT(ch.n > 0 && ch.n <= MAX_CHILDREN);
        bool orNode = g.side_is_goal();
        u32 cpn[MAX_CHILDREN], cdn[MAX_CHILDREN];
        for (;;) {
            u64 spn = 0, sdn = 0; u32 mpn = INF, mdn = INF, mpn2 = INF, mdn2 = INF; int best = 0;
            for (int i = 0; i < ch.n; i++) {
                g.do_move(ch.first[i], ply); u64 kc = g.key(); g.undo_move(ch.first[i], ply);
                bool rep = false; for (int q = 0; q <= ply && !rep; q++) rep = path[q] == kc;
                if (rep) { cpn[i] = INF; cdn[i] = repLoss ? 0 : INF; } else look(kc, cpn[i], cdn[i]);
                spn += cpn[i]; sdn += cdn[i];
                if (orNode) { if (cpn[i] < mpn) { mpn2 = mpn; mpn = cpn[i]; best = i; } else if (cpn[i] < mpn2) mpn2 = cpn[i]; }
                else        { if (cdn[i] < mdn) { mdn2 = mdn; mdn = cdn[i]; best = i; } else if (cdn[i] < mdn2) mdn2 = cdn[i]; }
            }
            pn = orNode ? mpn : sat(spn); dn = orNode ? sat(sdn) : mdn;
            if (pn >= thpn || dn >= thdn) break;
            u32 tpn, tdn;                                    // soglie del figlio scelto (Nagai) con 1+ε sulla seconda migliore
            if (orNode) { tpn = u32(std::min<u64>(thpn, std::max<u64>(u64(mpn2) + 1, u64(mpn2 * (1 + eps))))); tdn = sat(u64(thdn) - dn + cdn[best]); }
            else        { tdn = u32(std::min<u64>(thdn, std::max<u64>(u64(mdn2) + 1, u64(mdn2 * (1 + eps))))); tpn = sat(u64(thpn) - pn + cpn[best]); }
            g.do_move(ch.first[best], ply);
            mid(ply + 1, tpn, tdn, cpn[best], cdn[best]);
            g.undo_move(ch.first[best], ply);
            if (aborted_) return;
        }
        store(k, pn, dn);
        if (solved(pn, dn)) {
            u16 mv = 0;
            for (int i = 0; i < ch.n && !mv; i++)
                if (pn == 0 ? (orNode && cpn[i] == 0) : (!orNode && cdn[i] == 0)) mv = G::code(ch.first[i]);
            pt.store(k, ProofTable::pack_proven(pn == 0, 0, goal, 0, mv));
        }
    }

  public:
    DfPn(G& game, ProofTable& table, u8 goalSide, unsigned log2Entries, double epsilon = 0.25, bool repetitionIsLoss = true) :
        tab(u64(1) << log2Entries), mask((u64(1) << log2Entries) - 1), g(game), pt(table), goal(goalSide), eps(epsilon), repLoss(repetitionIsLoss) {}

    Result solve(const Budget& budget) {
        b = budget; nodes_ = 0; aborted_ = false; maxPly_ = 0; path.assign(b.maxDepth + 2, 0);
        u32 pn, dn; look(g.key(), pn, dn);
        if (!solved(pn, dn)) mid(0, INF, INF, pn, dn);
        Result r;
        if (solved(pn, dn)) {
            r.status = pn == 0 ? Status::PROVEN_WIN : Status::PROVEN_NOWIN;
            u64 d = pt.probe(g.key()); u32 h; if (d) r.move = ProofTable::unpack(d, h).move;
        } else if (aborted_) { r.status = Status::UNKNOWN_BUDGET; r.cut = true; }
        else r.status = Status::UNKNOWN_CYCLE;               // pn = dn = ∞ alla radice (solo con repLoss = false)
        return r;
    }
    u64 nodes() const { return nodes_; }
    bool aborted() const { return aborted_; }
    int max_ply() const { return maxPly_; }
};

}  // namespace WinTree

#endif
