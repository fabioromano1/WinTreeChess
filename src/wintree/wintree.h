// WinTree: nucleo generico (spec §8.1, §8.3). C++17, nessuna dipendenza.
// Algoritmo di Sez. 3 del paper con O1 corretto (P1), esiti a quattro livelli con flag cut (I1-I4),
// budget cooperativo, tabella delle prove a due parole con xor-check, dejavu compatto (ply del
// bloccante più profondo + hash del prefisso) con propagazione ai padri, intervalli di validità sul
// contatore delle 50 mosse (rev/req, §7.5) e modalità Goal (§8.1: tetto 245 ply in WIN).
#ifndef WINTREE_H
#define WINTREE_H

#include <atomic>
#include <bitset>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <memory>
#include <type_traits>
#include <vector>

namespace WinTree {

using u8  = std::uint8_t;
using u16 = std::uint16_t;
using i16 = std::int16_t;
using u32 = std::uint32_t;
using u64 = std::uint64_t;

// Assert attivi anche in release: i test di §9.2 li richiedono.
inline void assert_fail(const char* c, const char* f, int l) {
    std::fprintf(stderr, "WT_ASSERT fallito: %s (%s:%d)\n", c, f, l);
    std::abort();
}
#define WT_ASSERT(c) ((c) ? void(0) : ::WinTree::assert_fail(#c, __FILE__, __LINE__))

enum class Status : u8 { UNKNOWN_BUDGET, UNKNOWN_CYCLE, PROVEN_WIN, PROVEN_NOWIN };
// Modalità (§8.1): WIN limita la profondità a 245 ply (valori mate_in) e nei nodi AND si ferma al primo figlio NIL;
// NOWIN esplora tutti i figli (Sez. 3) perché cerca le confutazioni (PROVEN_NOWIN) e il grafo di strategia completo.
enum class Goal : u8 { WIN, NOWIN };
// WIN_GOAL_C0: vittoria valida solo con contatore 0 (sink TB, §7.6); NOWIN_GOAL_C100: patta valida solo con
// contatore >= 100 (50 mosse, §7.5). Gli altri sink valgono per ogni contatore.
enum class Terminal : u8 { NONE, WIN_GOAL, NOWIN_GOAL, WIN_GOAL_C0, NOWIN_GOAL_C100 };

struct Result {
    Status status = Status::UNKNOWN_BUDGET;
    u16  d     = 0;      // PROVEN_WIN: limite superiore della distanza alla vittoria (Thm 3.6)
    u16  resid = 0;      // UNKNOWN_BUDGET: profondità residua tentata (0 = ricerca interrotta)
    i16  p     = -1;     // UNKNOWN_CYCLE: ply del bloccante più profondo sopra il nodo (-1 = nessuno)
    bool cut   = false;  // I2: un taglio di budget ha contribuito al NIL
    u8   rev   = 0;      // PROVEN_WIN: valida con contatore c se c + rev <= 100 (§7.5)
    u8   req   = 0;      // PROVEN_NOWIN: valida con contatore c se c >= req (§7.5)
    u16  move  = 0;      // PROVEN: mossa testimone (OR vinto / AND confutato) codificata da G::code, 0 = nessuna
    bool proven() const { return status >= Status::PROVEN_WIN; }
};

using Clock = std::chrono::steady_clock;

struct Budget {
    u64 maxNodes = ~u64(0);
    int maxDepth = 1024;                               // ply oltre cui un non-terminale è UNKNOWN_BUDGET
    Clock::time_point deadline = Clock::time_point::max();
    const std::atomic<bool>* stop = nullptr;
};

template<class T> struct Span { const T* first; int n; };

// Proof table (§8.3): entry di due parole, w0 = key ^ data, w1 = data; l'entry vale solo se (w0 ^ w1) == key.
// data: bit 63..62 status (00 vuoto, 01 UNKNOWN, 10 PROVEN_WIN, 11 PROVEN_NOWIN), 61 goal, 54..47 d (PROVEN_WIN)
// o resid (UNKNOWN_BUDGET). PROVEN: 46..40 rev/req (7 bit), 39..24 mossa (16 bit). UNKNOWN: 55 kind (0 BUDGET,
// 1 CYCLE), 46..39 p+1 (UNKNOWN_CYCLE), 38..7 hash del prefisso (32 bit: la spec ne prevede 16, 32 rendono
// trascurabili i falsi positivi nei test).
class ProofTable {
    struct Entry { std::atomic<u64> w0, w1; };
    std::unique_ptr<Entry[]> e;
    u64 mask;
  public:
    explicit ProofTable(unsigned log2Entries) : e(new Entry[u64(1) << log2Entries]), mask((u64(1) << log2Entries) - 1) { clear(); }
    void clear() { for (u64 i = 0; i <= mask; i++) { e[i].w0.store(0, std::memory_order_relaxed); e[i].w1.store(0, std::memory_order_relaxed); } }
    u64 probe(u64 key) const {
        const Entry& x = e[key & mask];
        u64 a = x.w0.load(std::memory_order_relaxed), b = x.w1.load(std::memory_order_relaxed);
        return (a ^ b) == key ? b : 0;
    }
    // Rimpiazzo: mai un PROVEN con un UNKNOWN (I4), qualunque sia la chiave; per il resto l'ultimo scrive
    // (a parità di chiave una riprova avviene solo se l'intervallo rev/req non copre il contatore corrente,
    // e produce un intervallo più ampio).
    void store(u64 key, u64 data) {
        Entry& x = e[key & mask];
        if (proven(x.w1.load(std::memory_order_relaxed)) && !proven(data)) return;
        x.w1.store(data, std::memory_order_relaxed);
        x.w0.store(key ^ data, std::memory_order_relaxed);
    }
    static bool proven(u64 d) { return (d >> 63) != 0; }
    static u64 pack_proven(bool win, u16 d, u8 goal, u8 r50, u16 move) {
        return (win ? u64(2) : u64(3)) << 62 | u64(goal & 1) << 61 | u64(d > 255 ? 255 : d) << 47
             | u64(r50 > 127 ? 127 : r50) << 40 | u64(move) << 24;
    }
    static u64 pack_unknown(bool cycle, u16 resid, i16 p, u32 h, u8 goal) {
        return u64(1) << 62 | u64(goal & 1) << 61 | u64(cycle) << 55 | u64(resid > 255 ? 255 : resid) << 47
             | u64(u8(p + 1)) << 39 | u64(h) << 7;
    }
    static Result unpack(u64 d, u32& h) {
        Result r;
        u64 top = d >> 62;
        r.status = top == 2 ? Status::PROVEN_WIN : top == 3 ? Status::PROVEN_NOWIN
                 : (d >> 55 & 1) ? Status::UNKNOWN_CYCLE : Status::UNKNOWN_BUDGET;
        r.d = r.resid = u16(d >> 47 & 255);
        if (r.proven()) {
            (r.status == Status::PROVEN_WIN ? r.rev : r.req) = u8(d >> 40 & 127);
            r.move = u16(d >> 24);
            h = 0;
        } else {
            r.p = i16(int(d >> 39 & 255) - 1);
            r.cut = r.status == Status::UNKNOWN_BUDGET;
            h = u32(d >> 7);
        }
        return r;
    }
};

inline u64 mix64(u64 x) {  // finalizer di splitmix64
    x ^= x >> 30; x *= 0xbf58476d1ce4e5b9ULL; x ^= x >> 27; x *= 0x94d049bb133111ebULL; return x ^ (x >> 31);
}

// Interfaccia richiesta a G (§8.1):
//   u64 key() const; bool side_is_goal() const; Terminal terminal();
//   Span<Move> gen_children(int ply); void do_move(Move, int ply); void undo_move(Move, int ply);
//   int rule50() const; bool reversible(Move) const;      regola delle 50 mosse (§7.5; grafi: 0 e false)
//   static u16 code(Move); static Move move(u16);         mossa testimone nell'entry della tabella
template<class G>
class Prover {
    static_assert(std::is_same<decltype(std::declval<const G&>().key()), u64>::value, "G::key() deve restituire u64");
    static_assert(std::is_same<decltype(std::declval<const G&>().side_is_goal()), bool>::value, "G::side_is_goal() deve restituire bool");
    static_assert(std::is_same<decltype(std::declval<G&>().terminal()), Terminal>::value, "G::terminal() deve restituire Terminal");
    using Move = typename G::Move;
    static constexpr int MAX_CHILDREN = 256;
    static constexpr u64 CHECK_EVERY = 1024;

    G& g; ProofTable& pt; u8 goal; Goal mode;
    Budget b; u64 nodes_ = 0; bool aborted_ = false;
    u64 andNodes_ = 0, reexp_ = 0, plySum_ = 0; int maxPly_ = 0;   // statistiche per l'analisi di G1 (§11)
    std::vector<u64> path;                    // chiavi del cammino corrente (GRAY implicito)
    std::vector<u64> H;                       // H[i] = hash del prefisso path[0..i-1]; H[0] = prefisso vuoto
    std::vector<std::bitset<MAX_CHILDREN>> done;  // O1: figli letti nel primo ciclo, per ply
    // Ply dei bloccanti GRAY incontrati nel sottoalbero del nodo a ogni ply (W parole da 64 bit per ply). Serve
    // l'insieme, non solo il massimo: un padre esclude il proprio ply e deve conoscere il massimo dei restanti.
    std::vector<u64> blk; int W = 1;

  public:
    // Interruttori per le ablazioni (B5, §9.4): O6 = arresto al primo figlio NIL nei nodi AND in modalità WIN;
    // dejavu = salto delle entry UNKNOWN_CYCLE con prefisso uguale (§8.3).
    bool firstNil = true, dejavu = true;
    Prover(G& game, ProofTable& table, u8 goalSide, Goal m) : g(game), pt(table), goal(goalSide), mode(m) {}

    Result solve(const Budget& budget) {
        b = budget; nodes_ = 0; aborted_ = false; andNodes_ = reexp_ = plySum_ = 0; maxPly_ = 0;
        int cap = mode == Goal::WIN ? 245 : 4096;        // §8.1: MAX_PLY - 1 in WIN; memoria (stack) in NOWIN
        if (b.maxDepth > cap) b.maxDepth = cap;
        path.assign(b.maxDepth + 2, 0); H.assign(b.maxDepth + 3, 0); done.assign(b.maxDepth + 2, {});
        W = (b.maxDepth + 64) / 64; blk.assign(size_t(b.maxDepth + 2) * W, 0);
        Result r = visit(0);
        WT_ASSERT((r.status == Status::UNKNOWN_BUDGET) == r.cut);                 // I2
        WT_ASSERT(!aborted_ || r.status == Status::UNKNOWN_BUDGET);
        return r;
    }
    u64 nodes() const { return nodes_; }      // espansioni (chiamate ricorsive)
    bool aborted() const { return aborted_; }
    u64 and_nodes() const { return andNodes_; }           // espansioni di nodi AND
    u64 reexpansions() const { return reexp_; }           // espansioni di nodi già in tabella (UNKNOWN o PROVEN fuori intervallo)
    double mean_ply() const { return nodes_ ? double(plySum_) / nodes_ : 0; }
    int max_ply() const { return maxPly_; }

  private:
    static Result cycle(int p) { Result r; r.status = Status::UNKNOWN_CYCLE; r.p = i16(p); return r; }
    static Result budget_cut(u16 resid = 0) { Result r; r.status = Status::UNKNOWN_BUDGET; r.resid = resid; r.cut = true; return r; }
    bool same_goal(u64 data) const { return data && (data >> 61 & 1) == (goal & 1); }
    // §7.5: un'entry PROVEN vale per il contatore corrente solo dentro il suo intervallo.
    bool valid(const Result& e) const {
        return e.status == Status::PROVEN_WIN ? g.rule50() + e.rev <= 100 : g.rule50() >= e.req;
    }
    u64* blockers(int ply) { return &blk[size_t(ply) * W]; }
    void mark(int ply, int q) { blockers(ply)[q >> 6] |= u64(1) << (q & 63); }
    int deepest(int ply) {
        for (int w = W - 1; w >= 0; w--)
            for (int bit = 63; bit >= 0; bit--) if (blockers(ply)[w] >> bit & 1) return w * 64 + bit;
        return -1;
    }
    // Un bloccante a ply q del figlio appena esaminato (a `ply`) entra nell'insieme del padre (ply-1): se è il
    // padre stesso non conta (induzione sull'altezza dell'albero vincente); se è più in alto ma il figlio è stato
    // saltato se ne conosce solo il massimo, quindi se quel massimo è il padre si assume tutto il prefisso sopra.
    void inherit(int ply, int q) {
        if (ply == 0 || q < 0) return;
        if (q < ply - 1) mark(ply - 1, q); else if (ply >= 2) mark(ply - 1, ply - 2);
    }

    // Esito del nodo corrente a `ply` senza espanderlo se non serve: GRAY (I3), PROVEN in tabella,
    // salto dejavu (solo UNKNOWN_CYCLE con prefisso uguale), salto UNKNOWN_BUDGET con residuo non maggiore.
    Result visit(int ply) {
        u64 k = g.key();
        for (int q = 0; q < ply; q++)
            if (path[q] == k) { inherit(ply, q); return cycle(q); }
        u64 data = pt.probe(k);
        if (same_goal(data)) {
            u32 h; Result e = ProofTable::unpack(data, h);
            if (e.proven() && valid(e)) return e;
            if (dejavu && e.status == Status::UNKNOWN_CYCLE && e.p < ply && u32(H[e.p + 1]) == h) { inherit(ply, e.p); return cycle(e.p); }
            if (e.status == Status::UNKNOWN_BUDGET && e.resid > 0 && e.resid >= b.maxDepth - ply) return budget_cut(e.resid);
            reexp_++;
        }
        return rec(ply);
    }

    Result rec(int ply) {
        if (nodes_ >= b.maxNodes || (nodes_ % CHECK_EVERY == 0 && nodes_ > 0
            && ((b.stop && b.stop->load(std::memory_order_relaxed)) || Clock::now() > b.deadline))) {
            aborted_ = true; return budget_cut();
        }
        nodes_++; plySum_ += ply; if (ply > maxPly_) maxPly_ = ply;
        u64 k = g.key();
        Terminal t = g.terminal();
        if (t != Terminal::NONE) {
            bool tw = t == Terminal::WIN_GOAL || t == Terminal::WIN_GOAL_C0;
            u8 r50 = t == Terminal::WIN_GOAL_C0 || t == Terminal::NOWIN_GOAL_C100 ? 100 : 0;
            Result r; r.status = tw ? Status::PROVEN_WIN : Status::PROVEN_NOWIN; (tw ? r.rev : r.req) = r50;
            pt.store(k, ProofTable::pack_proven(tw, 0, goal, r50, 0));
            return r;
        }
        if (ply >= b.maxDepth) return budget_cut();
        path[ply] = k; H[ply + 1] = mix64(H[ply] ^ k);
        for (int w = 0; w < W; w++) blockers(ply)[w] = 0;
        Span<Move> ch = g.gen_children(ply);
        WT_ASSERT(ch.n > 0 && ch.n <= MAX_CHILDREN);
        bool orNode = g.side_is_goal();
        andNodes_ += !orNode;
        // win parte da FALSE (OR) / TRUE (AND); nil = almeno un figlio irrisolto; end = testimone/confutazione
        bool win = !orNode, nil = false, end = false, cut = false;
        u16 d = 0, mv = 0; u8 r50 = 0;
        // rev/req (§7.5) del figlio visto dal padre: WIN reversibile 1 + rev, irreversibile 1; NOWIN reversibile
        // req - 1, irreversibile 0. Testimone: il suo valore; altrimenti il massimo su tutti i figli.
        auto apply = [&](const Result& c, bool rv, Move m) {   // i bloccanti dei figli NIL sono già in blockers(ply)
            if (c.proven()) {
                bool cw = c.status == Status::PROVEN_WIN;
                u8 cr = cw ? (rv ? u8(c.rev < 127 ? c.rev + 1 : 127) : 1) : (rv && c.req ? u8(c.req - 1) : 0);
                if (cw == orNode) { win = cw; d = u16(c.d + 1); r50 = cr; mv = G::code(m); end = true; }
                else { if (!orNode) d = d > c.d + 1 ? d : u16(c.d + 1); r50 = r50 > cr ? r50 : cr; }
            } else { nil = true; cut |= c.cut; }
        };
        std::bitset<MAX_CHILDREN>& first = done[ply];
        first.reset();
        for (int i = 0; i < ch.n && !end; i++) {         // O1: prima i figli già PROVEN, senza ricorsione
            Move m = ch.first[i]; bool rv = g.reversible(m);
            g.do_move(m, ply);
            u64 data = pt.probe(g.key()); u32 h; Result c;
            bool ok = same_goal(data) && ProofTable::proven(data) && valid(c = ProofTable::unpack(data, h));
            g.undo_move(m, ply);
            if (ok) { apply(c, rv, m); first.set(i); }
        }
        for (int i = 0; i < ch.n && !end; i++) {         // gli altri: visit legge anche i diventati PROVEN (P1)
            if (first.test(i)) continue;
            Move m = ch.first[i]; bool rv = g.reversible(m);
            g.do_move(m, ply);
            Result c = visit(ply + 1);
            g.undo_move(m, ply);
            if (aborted_) return budget_cut();
            apply(c, rv, m);
            // WIN: un figlio NIL basta a negare la vittoria di un nodo AND in questa iterazione (potatura da
            // risolutore di matti); gli altri figli servirebbero solo a un PROVEN_NOWIN, cercato in modalità NOWIN.
            if (nil && !orNode && mode == Goal::WIN && firstNil) break;
        }
        Result r;
        if (end || !nil) {                                // I1: PROVEN solo da figli PROVEN o terminali
            r.status = win ? Status::PROVEN_WIN : Status::PROVEN_NOWIN; r.d = win ? d : 0;
            (win ? r.rev : r.req) = r50; r.move = mv;
            pt.store(k, ProofTable::pack_proven(win, r.d, goal, r50, mv));
        } else if (cut) {
            r = budget_cut(u16(b.maxDepth - ply));
            pt.store(k, ProofTable::pack_unknown(false, r.resid, -1, 0, goal));
        } else {                                          // I3: irrisolto solo per cicli
            int pmax = deepest(ply);
            r = cycle(pmax);
            pt.store(k, ProofTable::pack_unknown(true, 0, i16(pmax), u32(H[pmax + 1]), goal));
            if (ply > 0)                                  // propagazione al padre, escluso il padre stesso
                for (int w = 0; w < W; w++)
                    blockers(ply - 1)[w] |= blockers(ply)[w] & ~((ply - 1) / 64 == w ? u64(1) << ((ply - 1) & 63) : 0);
        }
        return r;
    }
};

}  // namespace WinTree

#endif
