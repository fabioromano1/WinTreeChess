// WinTree: nucleo generico (spec §8.1, §8.3). C++17, nessuna dipendenza.
// Algoritmo di Sez. 3 del paper con O1 corretto (P1), esiti a quattro livelli con flag cut (I1-I4),
// budget cooperativo, tabella delle prove a due parole con xor-check, dejavu compatto (ply del
// bloccante più profondo + hash del prefisso) con propagazione ai padri.
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
enum class Terminal : u8 { NONE, WIN_GOAL, NOWIN_GOAL };

struct Result {
    Status status = Status::UNKNOWN_BUDGET;
    u16  d     = 0;      // PROVEN_WIN: limite superiore della distanza alla vittoria (Thm 3.6)
    u16  resid = 0;      // UNKNOWN_BUDGET: profondità residua tentata (0 = ricerca interrotta)
    i16  p     = -1;     // UNKNOWN_CYCLE: ply del bloccante più profondo sopra il nodo (-1 = nessuno)
    bool cut   = false;  // I2: un taglio di budget ha contribuito al NIL
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
// data: bit 63..62 status (00 vuoto, 01 UNKNOWN, 10 PROVEN_WIN, 11 PROVEN_NOWIN), 61 goal, 55 kind
// (UNKNOWN: 0 BUDGET, 1 CYCLE), 54..47 d (PROVEN_WIN) o resid (UNKNOWN_BUDGET), 46..39 p+1 (UNKNOWN_CYCLE),
// 38..7 hash del prefisso (32 bit: la spec ne prevede 16, 32 rendono trascurabili i falsi positivi nei test).
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
    // Rimpiazzo (M1): mai un PROVEN con un UNKNOWN (I4); per il resto l'ultimo scrive.
    void store(u64 key, u64 data) {
        Entry& x = e[key & mask];
        u64 old = x.w0.load(std::memory_order_relaxed) ^ x.w1.load(std::memory_order_relaxed);
        if (proven(old) && !proven(data)) return;
        x.w1.store(data, std::memory_order_relaxed);
        x.w0.store(key ^ data, std::memory_order_relaxed);
    }
    static bool proven(u64 d) { return (d >> 63) != 0; }
    static u64 pack_proven(bool win, u16 d, u8 goal) {
        return (win ? u64(2) : u64(3)) << 62 | u64(goal & 1) << 61 | u64(d > 255 ? 255 : d) << 47;
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
        r.p = i16(int(d >> 39 & 255) - 1);
        r.cut = r.status == Status::UNKNOWN_BUDGET;
        h = u32(d >> 7);
        return r;
    }
};

inline u64 mix64(u64 x) {  // finalizer di splitmix64
    x ^= x >> 30; x *= 0xbf58476d1ce4e5b9ULL; x ^= x >> 27; x *= 0x94d049bb133111ebULL; return x ^ (x >> 31);
}

// Interfaccia richiesta a G (§8.1):
//   u64 key() const; bool side_is_goal() const; Terminal terminal();
//   Span<Move> gen_children(int ply); void do_move(Move, int ply); void undo_move(Move, int ply);
// reversible()/rule50() (regola delle 50 mosse) arrivano con l'adapter scacchi (M2).
template<class G>
class Prover {
    static_assert(std::is_same<decltype(std::declval<const G&>().key()), u64>::value, "G::key() deve restituire u64");
    static_assert(std::is_same<decltype(std::declval<const G&>().side_is_goal()), bool>::value, "G::side_is_goal() deve restituire bool");
    static_assert(std::is_same<decltype(std::declval<G&>().terminal()), Terminal>::value, "G::terminal() deve restituire Terminal");
    using Move = typename G::Move;
    static constexpr int MAX_CHILDREN = 256;
    static constexpr u64 CHECK_EVERY = 1024;

    G& g; ProofTable& pt; u8 goal;
    Budget b; u64 nodes_ = 0; bool aborted_ = false;
    std::vector<u64> path;                    // chiavi del cammino corrente (GRAY implicito)
    std::vector<u64> H;                       // H[i] = hash del prefisso path[0..i-1]; H[0] = prefisso vuoto
    std::vector<std::bitset<MAX_CHILDREN>> done;  // O1: figli letti nel primo ciclo, per ply
    // Ply dei bloccanti GRAY incontrati nel sottoalbero del nodo a ogni ply (W parole da 64 bit per ply). Serve
    // l'insieme, non solo il massimo: un padre esclude il proprio ply e deve conoscere il massimo dei restanti.
    std::vector<u64> blk; int W = 1;

  public:
    Prover(G& game, ProofTable& table, u8 goalSide) : g(game), pt(table), goal(goalSide) {}

    Result solve(const Budget& budget) {
        b = budget; nodes_ = 0; aborted_ = false;
        path.assign(b.maxDepth + 2, 0); H.assign(b.maxDepth + 3, 0); done.assign(b.maxDepth + 2, {});
        W = (b.maxDepth + 64) / 64; blk.assign(size_t(b.maxDepth + 2) * W, 0);
        Result r = visit(0);
        WT_ASSERT((r.status == Status::UNKNOWN_BUDGET) == r.cut);                 // I2
        WT_ASSERT(!aborted_ || r.status == Status::UNKNOWN_BUDGET);
        return r;
    }
    u64 nodes() const { return nodes_; }      // espansioni (chiamate ricorsive)
    bool aborted() const { return aborted_; }

  private:
    static Result cycle(int p) { Result r; r.status = Status::UNKNOWN_CYCLE; r.p = i16(p); return r; }
    static Result budget_cut(u16 resid = 0) { Result r; r.status = Status::UNKNOWN_BUDGET; r.resid = resid; r.cut = true; return r; }
    bool same_goal(u64 data) const { return data && (data >> 61 & 1) == (goal & 1); }
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
            if (e.proven()) return e;
            if (e.status == Status::UNKNOWN_CYCLE && e.p < ply && u32(H[e.p + 1]) == h) { inherit(ply, e.p); return cycle(e.p); }
            if (e.status == Status::UNKNOWN_BUDGET && e.resid > 0 && e.resid >= b.maxDepth - ply) return budget_cut(e.resid);
        }
        return rec(ply);
    }

    Result rec(int ply) {
        if (nodes_ >= b.maxNodes || (nodes_ % CHECK_EVERY == 0 && nodes_ > 0
            && ((b.stop && b.stop->load(std::memory_order_relaxed)) || Clock::now() > b.deadline))) {
            aborted_ = true; return budget_cut();
        }
        nodes_++;
        u64 k = g.key();
        Terminal t = g.terminal();
        if (t != Terminal::NONE) {
            Result r; r.status = t == Terminal::WIN_GOAL ? Status::PROVEN_WIN : Status::PROVEN_NOWIN;
            pt.store(k, ProofTable::pack_proven(t == Terminal::WIN_GOAL, 0, goal));
            return r;
        }
        if (ply >= b.maxDepth) return budget_cut();
        path[ply] = k; H[ply + 1] = mix64(H[ply] ^ k);
        for (int w = 0; w < W; w++) blockers(ply)[w] = 0;
        Span<Move> ch = g.gen_children(ply);
        WT_ASSERT(ch.n > 0 && ch.n <= MAX_CHILDREN);
        bool orNode = g.side_is_goal();
        // win parte da FALSE (OR) / TRUE (AND); nil = almeno un figlio irrisolto; end = testimone/confutazione
        bool win = !orNode, nil = false, end = false, cut = false;
        u16 d = 0;
        auto apply = [&](const Result& c) {     // i bloccanti dei figli NIL sono già in blockers(ply) (visit/rec)
            if (c.proven()) {
                bool cw = c.status == Status::PROVEN_WIN;
                if (cw == orNode) { win = cw; d = u16(c.d + 1); end = true; }
                else if (!orNode) { d = d > c.d + 1 ? d : u16(c.d + 1); }
            } else { nil = true; cut |= c.cut; }
        };
        std::bitset<MAX_CHILDREN>& first = done[ply];
        first.reset();
        for (int i = 0; i < ch.n && !end; i++) {         // O1: prima i figli già PROVEN, senza ricorsione
            g.do_move(ch.first[i], ply);
            u64 data = pt.probe(g.key());
            g.undo_move(ch.first[i], ply);
            if (same_goal(data) && ProofTable::proven(data)) { u32 h; apply(ProofTable::unpack(data, h)); first.set(i); }
        }
        for (int i = 0; i < ch.n && !end; i++) {         // gli altri: visit legge anche i diventati PROVEN (P1)
            if (first.test(i)) continue;
            g.do_move(ch.first[i], ply);
            Result c = visit(ply + 1);
            g.undo_move(ch.first[i], ply);
            if (aborted_) return budget_cut();
            apply(c);
        }
        Result r;
        if (end || !nil) {                                // I1: PROVEN solo da figli PROVEN o terminali
            r.status = win ? Status::PROVEN_WIN : Status::PROVEN_NOWIN; r.d = win ? d : 0;
            pt.store(k, ProofTable::pack_proven(win, r.d, goal));
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
