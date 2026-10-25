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
    static constexpr u64 CHECK_EVERY = 64;                        // scadenza controllata ogni ~1 ms (M3: budget di pochi ms in partita)
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

    // cyc: confutazione tainted da ciclo (ripetizione sul cammino o entry da ciclo), altrimenti da tetto di profondità.
    struct Node { u64 pn = 1, dn = 1; bool tainted = false, cyc = false, fresh = false, invalid = false, proven = false, tb = false; u8 rr = 0; u16 d = 0, move = 0; };

    G& g; ProofTable& pt; u8 goal; Goal mode; double eps;
    Budget b; u64 nodes_ = 0, moves_ = 0, andNodes_ = 0, plySum_ = 0; bool aborted_ = false; int maxPly_ = 0;
    std::vector<u64> path;                     // chiavi del cammino corrente (ripetizioni, §7.3)
    // Cache della mobilità (G::mobility) dei figli senza entry, a indirizzamento diretto: chiave con gli 8 bit bassi
    // sostituiti dalla mobilità (< 256). Evita la do_move della rilettura (ciclo MID e frame successivi dello stesso
    // nodo, trasposizioni); una collisione sui 56 bit alti darebbe solo un pn/dn iniziale diverso (euristica, mai una prova).
    static constexpr int MOB_LOG2 = 16;
    std::vector<u64> mobc = std::vector<u64>(size_t(1) << MOB_LOG2, 0);

    // Lettura di un'entry della posizione a `ply` con contatore `r50`: PROVEN valido per il contatore (§7.5) e, con
    // capD, con la prova entro il tetto (ply + d <= maxDepth: ri-ricerca a d minimo) => (0, ∞) o (∞, 0) con rev/req,
    // d e mossa; PROVEN fuori intervallo => `invalid` (irrisolto da ricercare nell'entry ombra); UNKNOWN_CYCLE =>
    // confutazione tainted; irrisolto => (pn, dn) memorizzati; assente => fresh (pn = dn = 1, poi G::init sui figli).
    Node look(u64 k, int ply, int r50) const {
        Node n; u64 data = pt.probe(k, goal & 1);
        if (data && (data >> 61 & 1) == (goal & 1)) {
            if (ProofTable::proven(data)) {
                u32 h; Result e = ProofTable::unpack(data, h);
                bool win = e.status == Status::PROVEN_WIN;
                n.proven = true; n.tb = e.tbLeaf;
                if (win ? r50 + e.rev <= 100 && (!capD || ply + e.d <= b.maxDepth) : r50 >= e.req) {
                    n.pn = win ? 0 : INF; n.dn = win ? INF : 0; n.rr = win ? e.rev : e.req; n.d = e.d; n.move = e.move;
                    return n;
                }
                n.invalid = true; return n;
            } else if (data >> 55 & 1) {
                // Confutazione tainted con la profondità residua al tetto + 1 in `resid` (bit 7: da ciclo). Da tetto:
                // vale solo con residuo <= resid - 1 (come UNKNOWN_BUDGET(r) di I4), con più spazio il nodo va ri-cercato.
                // Da ciclo: riusata come oggi (incompletezza nota di §8.1b), ma con capD vale lo stesso limite di residuo:
                // le ri-ricerche a d minimo con tetto basso ne producono molte (il tetto taglia anche le vie d'uscita
                // dai cicli) e bloccherebbero i tetti successivi.
                int r = int(data >> 47 & 255); bool cyc = data >> 7 & 1;
                if ((capD || !cyc) && b.maxDepth - ply > r - 1) { n.fresh = true; return n; }
                n.pn = INF; n.dn = 0; n.tainted = true; n.cyc = cyc; return n;
            }
            else {
                n.pn = dec(u32(data >> 25) & ((1u << 30) - 1));
                n.dn = dec(u32(data >> 56 & 31) << 25 | u32(data & ((1u << 25) - 1)));
                return n;
            }
        }
        n.fresh = true;
        return n;
    }
    // Base, poi ombra (ProofTable::SHADOW) se la base è un PROVEN fuori intervallo, poi SHADOW2 (ri-ricerche a tetto con
    // capD); un'entry a sua volta fuori intervallo vale come assente (la riprova la sovrascrive: regime raro).
    Node lookup(u64 k, int ply, int r50) const {
        Node n = look(k, ply, r50);
        if (n.invalid) n = look(k ^ ProofTable::SHADOW, ply, r50);
        if (n.invalid) n = look(k ^ ProofTable::SHADOW2, ply, r50);
        if (n.invalid) { n = Node(); n.fresh = true; }
        return n;
    }
    // Figlio i-esimo del nodo a `ply`: chiave e contatore senza do_move (G::key_after, G::reversible); do_move solo
    // per un figlio senza entry la cui mobilità non è in cache (G::init ne ricava pn/dn), con la chiave calcolata
    // verificata. Il ciclo MID rilegge tutti i figli a ogni giro: con una do_move per lettura, ~35 per espansione.
    Node child(Move m, int ply, int i) {
        u64 kc = g.key_after(m); Node c;
        bool rep = false;
        for (int q = 0; q <= ply && !rep; q++) rep = path[q] == kc;
        if (rep) { c.pn = INF; c.dn = 0; c.tainted = c.cyc = true; return c; }   // ripetizione: sconfitta per la selezione, tainted
        c = lookup(kc, ply + 1, g.reversible(m) ? g.rule50() + 1 : 0);
        if (c.fresh) {
            u64& e = mobc[kc >> (64 - MOB_LOG2)]; int n;
            if ((e & ~u64(255)) == (kc & ~u64(255))) n = int(e & 255);
            else {
                g.do_move(m, ply); moves_++;
                WT_ASSERT(g.key() == kc);                                 // key_after esatta: una chiave sbagliata leggerebbe l'entry di un'altra posizione
                n = g.mobility();
                g.undo_move(m, ply);
                e = (kc & ~u64(255)) | u64(n);
            }
            g.init(ply, i, n, c.pn, c.dn);
        }
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
        // Con capD una ri-ricerca a tetto di un nodo la cui ombra è un PROVEN (altro regime del contatore) scrive in
        // SHADOW2: sovrascrivere l'ombra con l'esito di un tetto fallito perderebbe la prova e il suo certificato.
        Node own = look(k, ply, g.rule50()); u64 sk = k; bool force = false;
        if (own.invalid) {
            sk = k ^ ProofTable::SHADOW; force = look(sk, ply, g.rule50()).proven;
            if (capD && force) { sk = k ^ ProofTable::SHADOW2; force = look(sk, ply, g.rule50()).proven; }
        }
        auto put = [&](u64 data) {
            pt.store(sk, data, force);
            if (own.invalid && ProofTable::proven(data))                          // stesso genere: anche le entry degli altri regimi
                for (u64 s : {k, k ^ ProofTable::SHADOW}) if (s != sk && (data >> 62) == (pt.probe(s, goal & 1) >> 62)) pt.store(s, data);
        };
        Terminal t = g.terminal();
        if (t != Terminal::NONE) {
            bool tw = t == Terminal::WIN_GOAL || t == Terminal::WIN_GOAL_C0;
            u8 r50 = t == Terminal::WIN_GOAL_C0 || t == Terminal::NOWIN_GOAL_C100 ? 100 : 0;
            put(ProofTable::pack_proven(tw, 0, goal, r50, 0, t == Terminal::WIN_GOAL_C0));   // foglia TB: tbLeaf (§7.8)
            return;
        }
        if (ply >= b.maxDepth) { put(ProofTable::pack_unknown(true, 1, -1, 0, goal)); return; }   // tetto: confutazione tainted con residuo 0
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
        // Taint: da ciclo (`cycle`, relativa al cammino) se un OR confutato ha una confutazione da ciclo fra i figli
        // o un AND non ha né una confutazione pulita né una da tetto; altrimenti da tetto. Entrambe con il residuo corrente.
        bool win = pn == 0, tainted = false, clean = false, cycle = false, tb = false; u16 mv = 0, d = 0; u8 rr = 0;
        for (int i = 0; i < ch.n; i++) {
            Move m = ch.first[i]; bool rv = g.reversible(m);
            Node c = child(m, ply, i);
            if (win) {
                if (c.pn != 0) { WT_ASSERT(orNode); continue; }                  // AND provato: tutti i figli provati
                rr = std::max<int>(rr, rv ? std::min(127, c.rr + 1) : 1); d = u16(std::max<int>(d, std::min(255, c.d + 1))); tb |= c.tb;
                if (orNode) { mv = G::code(m); break; }
            } else {
                if (c.dn != 0) { WT_ASSERT(!orNode); continue; }                 // OR confutato: tutti i figli confutati
                u8 cr = rv && c.rr ? u8(c.rr - 1) : 0;
                if (orNode) { tainted |= c.tainted; cycle |= c.cyc; rr = std::max(rr, cr); }
                else if (!c.tainted) { rr = cr; mv = G::code(m); clean = true; tainted = false; break; }
                else if (!c.cyc && !tainted) { tainted = true; rr = cr; }      // AND: confutazione da tetto (meglio di una da ciclo), salvo una pulita dopo
            }
        }
        if (!win && !orNode && !clean && !tainted) { tainted = true; cycle = true; }
        WT_ASSERT(!(win && tainted));                                              // I1: una prova non usa ripetizioni
        put(tainted ? ProofTable::pack_unknown(true, u16(b.maxDepth - ply + 1), -1, cycle, goal) : ProofTable::pack_proven(win, d, goal, rr, mv, tb));
    }

  public:
    // capD: una PROVEN_WIN vale solo se la sua prova sta entro il tetto (ply + d <= maxDepth); la ri-ricerca di un
    // nodo PROVEN con d troppo grande passa dall'entry ombra come una riprova a contatore diverso (§7.5) e, se trova
    // una prova più corta, aggiorna la base. Serve alla minimizzazione di d (solve.h, prover.cpp con `go mate N`).
    bool capD = false;
    DfPn(G& game, ProofTable& table, u8 goalSide, Goal m, double epsilon = 0.25) : g(game), pt(table), goal(goalSide), mode(m), eps(epsilon) {}

    // retaint: ricerca anche una radice con entry UNKNOWN_CYCLE (confutazione tainted da un altro cammino), con il
    // cammino che parte da qui; l'esito risolto sovrascrive l'entry (verificatore §7.9.2, ri-ricerca locale).
    Result solve(const Budget& budget, bool retaint = false) {
        b = budget; nodes_ = moves_ = andNodes_ = plySum_ = 0; aborted_ = false; maxPly_ = 0;
        int cap = mode == Goal::WIN ? 245 : 4096;        // §8.1: MAX_PLY - 1 in WIN; memoria (stack) in NOWIN
        if (b.maxDepth > cap) b.maxDepth = cap;
        path.assign(b.maxDepth + 2, 0);
        u64 k = g.key();
        Node n = lookup(k, 0, g.rule50());
        if ((n.pn != 0 && n.dn != 0) || (retaint && n.tainted)) { mid(0, INF, INF); n = lookup(k, 0, g.rule50()); }
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
