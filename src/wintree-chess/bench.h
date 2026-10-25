// Benchmark di §9.4 sulla suite di matti (B6; B4 e B5 scegliendo le varianti) e B2 (nodi/s vs perft), con CSV e
// tabelle markdown per il gate G1 (§11). Protocollo B6: per ogni posizione un thread, un tetto di T secondi per
// solver; "risolta entro 1 s" = risolta con tempo <= 1 s nella stessa ricerca (i solver sono deterministici a
// parità di ordinamento: la ricerca a 1 s è il prefisso di quella a T s). Le varianti a caldo condividono una sola
// ricerca di Stockfish di `warmMs` per posizione, che entra nel loro tempo; `go mate N movetime T` di Stockfish
// gira per ultimo, su tabella e storie azzerate.
#ifndef WINTREE_CHESS_BENCH_H
#define WINTREE_CHESS_BENCH_H

#include "solve.h"
#include "warm.h"

#include "perft.h"

#include <atomic>
#include <cmath>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <map>
#include <memory>
#include <mutex>
#include <sstream>
#include <thread>

namespace WinTree {

// Riga EPD di matetrack: 4 campi FEN poi `bm #N;` (N > 0: il tratto matta in N mosse, 2N-1 ply; N < 0: il tratto è
// mattato in |N| mosse, 2|N| ply). Si tengono le righe 1, 1+step, 1+2·step, …, al più `count` (0 = tutte).
struct Epd { int id = 0; std::string fen; int mate = 0; Stockfish::Color goal = Stockfish::WHITE; int plies = 0; };

inline std::vector<Epd> read_epd(const std::string& path, int step, int count, std::string& err) {
    std::ifstream in(path); std::vector<Epd> v; std::string line; int n = 0;
    if (!in) { err = "impossibile aprire " + path; return v; }
    while (std::getline(in, line)) {
        n++;
        if ((n - 1) % step) continue;
        std::istringstream is(line); std::string f[4], tok; Epd e; e.id = n;
        if (!(is >> f[0] >> f[1] >> f[2] >> f[3])) { err = "riga " + std::to_string(n) + ": FEN incompleto"; return {}; }
        while (is >> tok) if (tok == "bm" && is >> tok && tok.size() > 1 && tok[0] == '#') { e.mate = std::atoi(tok.c_str() + 1); break; }
        if (!e.mate) { err = "riga " + std::to_string(n) + ": manca bm #N"; return {}; }
        e.fen = f[0] + " " + f[1] + " " + f[2] + " " + f[3] + " 0 1";
        Stockfish::Color stm = f[1] == "w" ? Stockfish::WHITE : Stockfish::BLACK;
        e.goal = e.mate > 0 ? stm : ~stm; e.plies = e.mate > 0 ? 2 * e.mate - 1 : -2 * e.mate;
        v.push_back(e);
        if (count && int(v.size()) >= count) break;
    }
    return v;
}

// Variante di solver: cold | warm | random (WinTree depth-first) | dfpn (nucleo §8.1b) | base (baseline df-pn di M2b)
// | sf, con i suffissi -warm/-cold, -noo6 e -nodejavu (ablazioni B5), -noinit (df-pn senza euristica), -epsN (ε = N %),
// -mind (df-pn con minimizzazione di d).
struct Variant { std::string name; enum Kind { WT, DFPN, BASE, SF } kind = WT; Order order = Order::COLD; bool firstNil = true, dejavu = true, heur = true, minD = false; double eps = 0.25; };

inline bool parse_variant(const std::string& s, Variant& v) {
    v = {}; v.name = s;
    std::istringstream is(s); std::string tok; bool first = true;
    while (std::getline(is, tok, '-')) {
        if (first) {
            first = false;
            if (tok == "warm") v.order = Order::WARM; else if (tok == "random") v.order = Order::RANDOM;
            else if (tok == "dfpn") v.kind = Variant::DFPN; else if (tok == "base") v.kind = Variant::BASE;
            else if (tok == "sf") v.kind = Variant::SF; else if (tok != "cold") return false;
        } else if (tok == "warm") v.order = Order::WARM; else if (tok == "cold") v.order = Order::COLD;
        else if (tok == "noo6") v.firstNil = false; else if (tok == "nodejavu") v.dejavu = false; else if (tok == "noinit") v.heur = false; else if (tok == "mind") v.minD = true;
        else if (tok.rfind("eps", 0) == 0 && tok.size() > 3) v.eps = std::atoi(tok.c_str() + 3) / 100.0; else return false;
    }
    return !first;
}

struct Row {
    int id = 0; std::string fen; int mate = 0, plies = 0; std::string variant, status; bool solved = false, cert = false;
    double secs = 0, cpu = 0; u64 nodes = 0, moves = 0, andNodes = 0, reexp = 0; int d = 0, d0 = 0, dver = 0, iters = 0, maxPly = 0; double meanPly = 0; std::string note;
};

struct BenchOpts { std::vector<Variant> variants; double secs = 10; int warmMs = 100; int threads = 4; std::string nnue, csv; int hashLog2 = 22; Cfg base; };

inline std::string csv_quote(const std::string& s) { return "\"" + s + "\""; }

inline void write_csv(const std::string& path, const std::vector<Row>& rows) {
    std::ofstream out(path);
    out << "id,fen,mate,plies,variant,solved,status,secs,cpu,nodes,moves,d,dver,cert,and_nodes,reexp,mean_ply,max_ply,iters,note,d0\n";
    for (const Row& r : rows)
        out << r.id << ',' << csv_quote(r.fen) << ',' << r.mate << ',' << r.plies << ',' << r.variant << ',' << r.solved << ',' << csv_quote(r.status) << ','
            << r.secs << ',' << r.cpu << ',' << r.nodes << ',' << r.moves << ',' << r.d << ',' << r.dver << ',' << r.cert << ',' << r.andNodes << ','
            << r.reexp << ',' << r.meanPly << ',' << r.maxPly << ',' << r.iters << ',' << csv_quote(r.note) << ',' << r.d0 << '\n';
}

inline double quantile(std::vector<double> v, double q) { if (v.empty()) return 0; std::sort(v.begin(), v.end()); return v[size_t(q * (v.size() - 1) + 0.5)]; }
inline std::string fmt(double x, int prec = 0) { char b[64]; std::snprintf(b, sizeof b, "%.*f", prec, x); return b; }

// Tabelle di B6/B4/B5 e verdetto G1 (§11) in markdown. Le mediane di nodi/do_move/tempo sono sulle posizioni
// risolte da quella variante; i rapporti WinTree/df-pn sono per posizione, sulle risolte da entrambi.
inline void summary(const std::vector<Row>& rows, const std::vector<Variant>& vars, int n, double secs, int warmMs) {
    std::map<std::string, std::map<int, const Row*>> by;                 // variante -> id -> riga
    for (const Row& r : rows) by[r.variant][r.id] = &r;
    std::cout << "\n### B6 — suite (" << n << " posizioni, tetto " << secs << " s per solver, un thread per posizione; a caldo: " << warmMs << " ms di Stockfish compresi)\n\n"
              << "| Solver | Risolte ≤ 1 s | Risolte ≤ " << secs << " s | Certificate | d = dichiarato | d > dichiarato | Mediana espansioni | Mediana do_move | Mediana s | p90 do_move |\n|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|\n";
    for (const Variant& v : vars) {
        auto& m = by[v.name]; int s1 = 0, sT = 0, cert = 0, deq = 0, dgt = 0; std::vector<double> nodes, moves, time;
        for (auto& [id, r] : m) if (r->solved) {
            sT++; s1 += r->secs <= 1.0; cert += r->cert; int d = v.kind == Variant::BASE ? r->dver : r->d;
            deq += d == r->plies; dgt += d > r->plies; nodes.push_back(double(r->nodes)); moves.push_back(double(r->moves)); time.push_back(r->secs);
        }
        std::cout << "| " << v.name << " | " << s1 << " | " << sT << " | " << (v.kind == Variant::SF ? std::string("—") : std::to_string(cert)) << " | " << deq << " | " << dgt << " | "
                  << fmt(quantile(nodes, 0.5)) << " | " << fmt(quantile(moves, 0.5)) << " | " << fmt(quantile(time, 0.5), 3) << " | " << fmt(quantile(moves, 0.9)) << " |\n";
    }
    // G1: frazione risolta vs Stockfish, rapporti WinTree/df-pn per posizione
    const Row* dummy = nullptr; (void) dummy;
    auto ratios = [&](const std::string& a, const std::string& b) {
        std::vector<double> rm, rn; int both = 0, onlyA = 0, onlyB = 0; double sumA = 0, sumB = 0;
        for (auto& [id, ra] : by[a]) {
            auto it = by[b].find(id); if (it == by[b].end()) continue; const Row* rb = it->second;
            if (ra->solved && rb->solved) { both++; rm.push_back(double(ra->moves) / std::max<u64>(rb->moves, 1)); rn.push_back(double(ra->nodes) / std::max<u64>(rb->nodes, 1)); sumA += ra->moves; sumB += rb->moves; }
            else if (ra->solved) onlyA++; else if (rb->solved) onlyB++;
        }
        if (!both && !onlyA && !onlyB) return;
        std::cout << "| " << a << " / " << b << " | " << both << " | " << onlyA << " | " << onlyB << " | " << fmt(quantile(rm, 0.5), 2) << " | " << fmt(quantile(rm, 0.9), 2) << " | "
                  << fmt(quantile(rm, 1.0), 2) << " | " << fmt(quantile(rn, 0.5), 2) << " | " << fmt(quantile(rn, 0.9), 2) << " | " << fmt(sumB ? sumA / sumB : 0, 2) << " |\n";
    };
    std::cout << "\n### Rapporti per posizione (sulle risolte da entrambi)\n\n| A / B | Risolte da entrambi | Solo A | Solo B | Mediana do_move A/B | p90 | max | Mediana espansioni A/B | p90 | Σ do_move A / Σ do_move B |\n|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|\n";
    for (const Variant& a : vars) for (const Variant& b : vars)        // nuclei diversi a pari ordinamento (G1: WinTree / df-pn, df-pn / baseline) e ogni variante / la prima dello stesso nucleo (ablazioni)
        if (a.kind != Variant::SF && b.kind != Variant::SF && a.name != b.name
            && ((a.kind < b.kind && a.order == b.order && a.heur == b.heur) || (&b == &vars[0] && a.kind == b.kind)))
            ratios(a.name, b.name);
    if (by.count("sf")) {
        std::cout << "\n### Confronto con Stockfish `go mate N` (stesso tempo, un thread)\n\n| Solver | Risolte dove Stockfish risolve ≤ 1 s | Stockfish ≤ 1 s | Risolte e certificate dove Stockfish risolve ≤ " << secs << " s | Stockfish ≤ " << secs << " s | Solo il solver | Solo Stockfish |\n|---|---:|---:|---:|---:|---:|---:|\n";
        for (const Variant& v : vars) {
            if (v.kind == Variant::SF) continue;
            int sf1 = 0, sfT = 0, a1 = 0, aT = 0, onlyA = 0, onlySf = 0;
            for (auto& [id, r] : by[v.name]) {
                auto it = by["sf"].find(id); if (it == by["sf"].end()) continue; const Row* s = it->second;
                bool ok = r->solved && (v.kind == Variant::SF || r->cert);
                if (s->solved) { sfT++; aT += ok; if (s->secs <= 1.0) { sf1++; a1 += ok && r->secs <= 1.0; } }
                onlyA += ok && !s->solved; onlySf += s->solved && !ok;
            }
            std::cout << "| " << v.name << " | " << a1 << " (" << fmt(sf1 ? 100.0 * a1 / sf1 : 0, 1) << " %) | " << sf1 << " | " << aT << " (" << fmt(sfT ? 100.0 * aT / sfT : 0, 1) << " %) | " << sfT << " | " << onlyA << " | " << onlySf << " |\n";
        }
    }
    std::cout << "\n### Dove WinTree spende i nodi (medie sulle posizioni della variante)\n\n| Solver | Esito | Posizioni | Espansioni medie | Nodi AND (%) | Riespansioni (%) | Ply medio | Ply massimo medio | Iterazioni medie |\n|---|---|---:|---:|---:|---:|---:|---:|---:|\n";
    for (const Variant& v : vars) {
        if (v.kind == Variant::SF) continue;
        for (int solved = 1; solved >= 0; solved--) {
            double n0 = 0, nodes = 0, andN = 0, re = 0, ply = 0, maxPly = 0, it = 0;
            for (auto& [id, r] : by[v.name]) if (r->solved == bool(solved)) { n0++; nodes += r->nodes; andN += r->andNodes; re += r->reexp; ply += r->meanPly * r->nodes; maxPly += r->maxPly; it += r->iters; }
            if (!n0) continue;
            std::cout << "| " << v.name << " | " << (solved ? "risolte" : "irrisolte") << " | " << fmt(n0) << " | " << fmt(nodes / n0) << " | " << fmt(nodes ? 100 * andN / nodes : 0, 1) << " | "
                      << fmt(nodes ? 100 * re / nodes : 0, 1) << " | " << fmt(nodes ? ply / nodes : 0, 1) << " | " << fmt(maxPly / n0, 1) << " | " << fmt(it / n0, 1) << " |\n";
        }
    }
}

inline void bench_suite(const std::vector<Epd>& suite, const BenchOpts& o) {
    bool needEngine = false;
    for (const Variant& v : o.variants) needEngine |= v.kind == Variant::SF || v.order == Order::WARM;
    if (needEngine && o.nnue.empty()) { std::cerr << "le varianti a caldo e sf richiedono --nnue PATH\n"; std::exit(2); }
    std::vector<Row> rows; std::mutex mu; std::atomic<int> next{0}; auto t0 = Clock::now();
    auto work = [&] {
        ProofTable pt(o.hashLog2);
        std::unique_ptr<SfEngine> sf; if (needEngine) sf.reset(new SfEngine(o.nnue));
        for (int i; (i = next++) < int(suite.size());) {
            const Epd& e = suite[i]; Game g; std::string err; std::vector<Row> local; std::string line = "#" + std::to_string(e.id) + " bm #" + std::to_string(e.mate) + ":";
            bool ok = g.set(e.fen, {}, err), warmed = false; WarmSources ws;
            for (int pass = 0; pass < 3; pass++)                        // 0: a freddo/casuale, 1: a caldo (una sola ricerca di Stockfish), 2: sf
                for (const Variant& v : o.variants) {
                    int cls = v.kind == Variant::SF ? 2 : v.order == Order::WARM ? 1 : 0;
                    if (cls != pass) continue;
                    Row r; r.id = e.id; r.fen = e.fen; r.mate = e.mate; r.plies = e.plies; r.variant = v.name;
                    if (!ok) { r.status = "FEN non valida: " + err; local.push_back(r); continue; }
                    if (v.kind == Variant::SF) {
                        sf->clear(); int pl = sf->go(e.fen, {}, int(o.secs * 1000), std::abs(e.mate));
                        r.secs = sf->ms() / 1000.0; r.nodes = sf->nodes(); r.d = std::abs(pl);
                        r.solved = pl && (pl > 0) == (e.mate > 0) && std::abs(pl) <= e.plies;
                        r.status = !pl ? "nessun matto" : r.solved ? "matto" : "matto oltre il dichiarato";
                    } else {
                        Cfg c = o.base; c.order = v.order; c.firstNil = v.firstNil; c.dejavu = v.dejavu; c.heur = v.heur; c.eps = v.eps; c.minD = v.minD; c.secs = o.secs; double extra = 0;
                        c.core = v.kind == Variant::WT ? Core::WINTREE : v.kind == Variant::DFPN ? Core::DFPN : Core::BASE;
                        if (v.order == Order::WARM) {
                            if (!warmed) { sf->clear(); sf->go(e.fen, {}, o.warmMs); ws = sf->sources(); warmed = true; }
                            c.warm = ws; extra = o.warmMs / 1000.0; c.secs -= extra;
                        }
                        pt.clear();
                        Outcome out = solve_any(g.pos, e.goal, Goal::WIN, c, pt);
                        r.status = status_name(out.r.status); r.secs = out.secs + extra; r.cpu = out.cpu + extra; r.nodes = out.nodes; r.moves = out.moves; r.d = out.r.d; r.d0 = out.d0;
                        r.andNodes = out.andNodes; r.reexp = out.reexp; r.meanPly = out.meanPly; r.maxPly = out.maxPly; r.iters = out.iters;
                        if (out.r.status == Status::PROVEN_WIN) {
                            Certificate cert = verify(g.pos, e.goal, out.r, c.tbMax, pt, 245);
                            r.solved = true; r.cert = cert.ok; r.dver = cert.depth; if (!cert.ok) r.note = cert.why;
                        } else if (out.r.status == Status::PROVEN_NOWIN) r.note = "confutazione alla radice (non attesa su una suite di matti)";
                    }
                    line += " " + v.name + (r.solved ? " ✓" : " ✗") + fmt(r.secs, 2) + "s/" + std::to_string(r.moves);
                    local.push_back(r);
                }
            std::lock_guard<std::mutex> lock(mu);
            rows.insert(rows.end(), local.begin(), local.end());
            std::cerr << line << "\n";
        }
    };
    std::vector<std::thread> pool;
    for (int t = 0; t < std::max(o.threads, 1); t++) pool.emplace_back(work);
    for (auto& th : pool) th.join();
    std::map<std::string, int> vidx; for (size_t k = 0; k < o.variants.size(); k++) vidx[o.variants[k].name] = int(k);
    std::sort(rows.begin(), rows.end(), [&](const Row& a, const Row& b) { return a.id != b.id ? a.id < b.id : vidx[a.variant] < vidx[b.variant]; });
    if (!o.csv.empty()) write_csv(o.csv, rows);
    double secs = std::chrono::duration<double>(Clock::now() - t0).count();
    std::cout << "suite: " << suite.size() << " posizioni, " << o.variants.size() << " varianti, " << o.threads << " thread, " << fmt(secs) << " s" << (o.csv.empty() ? "" : ", CSV in " + o.csv) << "\n";
    summary(rows, o.variants, int(suite.size()), o.secs, o.warmMs);
}

// B2 (§9.4): nodi/s del prover con terminali solo matto/stallo (modalità NOWIN al tetto `depth`, ordinamento freddo,
// un thread, sequenziale) contro perft di Stockfish alla stessa profondità sulle stesse posizioni: le prime `count`
// della suite con |N| >= 10, dove entro `depth` ply non ci sono matti e il prover attraversa l'albero come perft
// (meno le trasposizioni, che la tabella taglia). perft conta le foglie (a profondità 2 con la sola dimensione
// della MoveList); il prover conta espansioni e do_move (ogni figlio è letto in tabella con una do_move prima della
// visita, ciclo O1).
inline void bench_b2(const std::vector<Epd>& all, int depth, int count, const std::string& csv) {
    std::vector<Epd> suite;
    for (const Epd& e : all) if (std::abs(e.mate) >= 10 && (!count || int(suite.size()) < count)) suite.push_back(e);
    std::ofstream out; if (!csv.empty()) { out.open(csv); out << "id,fen,depth,perft_nodes,perft_moves,perft_secs,prover_nodes,prover_moves,prover_secs\n"; }
    std::cout << "\n### B2 — nodi/s del prover (terminali solo matto/stallo, NOWIN, profondità " << depth << ") vs perft " << depth << " (posizioni con |N| >= 10)\n\n"
              << "| # | Posizione | perft foglie | perft do_move | perft s | perft Mfoglie/s | perft Mdo_move/s | prover espansioni | prover do_move | prover s | prover Mespansioni/s | prover Mdo_move/s | do_move/s prover ÷ perft |\n|---:|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|\n";
    double sp = 0, spm = 0, sn = 0, sm = 0, tp = 0, tn = 0;
    ProofTable pt(22);
    for (const Epd& e : suite) {
        Game g; std::string err; if (!g.set(e.fen, {}, err)) continue;
        auto t0 = Clock::now(); u64 pn = Stockfish::Benchmark::perft<false>(g.pos, depth); double ps = std::chrono::duration<double>(Clock::now() - t0).count();
        u64 pm = Stockfish::MoveList<Stockfish::LEGAL>(g.pos).size();          // do_move di perft (conteggio in blocco all'ultimo ply): perft(1) + … + perft(depth-1)
        for (int k = 2; k < depth; k++) pm += Stockfish::Benchmark::perft<false>(g.pos, k);
        Cfg c; c.core = Core::WINTREE; c.nodes = ~u64(0) >> 1; c.depth = depth; c.mateOnly = true; pt.clear();
        Outcome o = solve(g.pos, e.goal, Goal::NOWIN, c, pt);
        sp += pn; spm += pm; sn += o.nodes; sm += o.moves; tp += ps; tn += o.secs;
        std::cout << "| " << e.id << " | `" << e.fen.substr(0, e.fen.rfind(' ', e.fen.rfind(' ') - 1)) << "` | " << pn << " | " << pm << " | " << fmt(ps, 3) << " | " << fmt(pn / ps / 1e6, 1) << " | " << fmt(pm / ps / 1e6, 1) << " | "
                  << o.nodes << " | " << o.moves << " | " << fmt(o.secs, 3) << " | " << fmt(o.nodes / o.secs / 1e6, 2) << " | " << fmt(o.moves / o.secs / 1e6, 2) << " | " << fmt((o.moves / o.secs) / (pm / ps), 2) << " |\n";
        if (out) out << e.id << ',' << csv_quote(e.fen) << ',' << depth << ',' << pn << ',' << pm << ',' << ps << ',' << o.nodes << ',' << o.moves << ',' << o.secs << '\n';
    }
    std::cout << "| **totale** | " << suite.size() << " posizioni | " << fmt(sp) << " | " << fmt(spm) << " | " << fmt(tp, 2) << " | " << fmt(sp / tp / 1e6, 1) << " | " << fmt(spm / tp / 1e6, 1) << " | " << fmt(sn) << " | " << fmt(sm) << " | "
              << fmt(tn, 2) << " | " << fmt(sn / tn / 1e6, 2) << " | " << fmt(sm / tn / 1e6, 2) << " | " << fmt((sm / tn) / (spm / tp), 2) << " |\n";
}

}  // namespace WinTree

#endif
