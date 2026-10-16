// Adapter "grafo esplicito" per il nucleo WinTree: legge il formato testuale di tests/wintree-java/Gen.java
// e riproduce il suo generatore (port di java.util.Random) per usare gli stessi semi di run.sh.
#ifndef WINTREE_GRAPH_H
#define WINTREE_GRAPH_H

#include "wintree.h"

#include <algorithm>
#include <cctype>
#include <climits>
#include <istream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace WinTree {

struct Graph {
    struct Node { int turn; int sink; std::vector<int> adj; };  // sink: -1 non-sink, 0 patta, 1/2 vinto da quel giocatore
    std::vector<Node> nodes;

    bool operator==(const Graph& o) const {
        if (nodes.size() != o.nodes.size()) return false;
        for (size_t i = 0; i < nodes.size(); i++)
            if (nodes[i].turn != o.nodes[i].turn || nodes[i].sink != o.nodes[i].sink || nodes[i].adj != o.nodes[i].adj) return false;
        return true;
    }

    // Formato: "id turno esito figli...", id = 0..n-1 in ordine, radice 0, turno 1|2, esito . | 0 | 1 | 2; "#" commento.
    // Verifica Def. 2.6 come Gen.parse. Legge fino alla prima riga che non è un vertice (o alla fine).
    static Graph parse(std::istream& in) {
        Graph g; std::string line;
        for (int c; (c = in.peek()) != EOF;) {
            if (c == '#' || c == '\n') { std::getline(in, line); if (c == '\n' && !g.nodes.empty()) break; continue; }
            if (!std::isdigit(c)) break;
            std::getline(in, line);
            std::istringstream ls(line);
            int id, turn; std::string sink;
            if (!(ls >> id >> turn >> sink) || id != int(g.nodes.size()) || (turn != 1 && turn != 2) || sink.size() != 1
                || std::string(".012").find(sink[0]) == std::string::npos)
                throw std::runtime_error("riga non valida: " + line);
            Node n; n.turn = turn; n.sink = sink[0] == '.' ? -1 : sink[0] - '0';
            for (int w; ls >> w;) n.adj.push_back(w);
            if ((n.sink < 0) != !n.adj.empty()) throw std::runtime_error(std::to_string(id) + ": un non-sink deve avere figli, un sink nessuno");
            g.nodes.push_back(n);
        }
        if (g.nodes.empty() || g.nodes[0].sink >= 0) throw std::runtime_error("la radice 0 deve essere un non-sink");
        for (size_t u = 0; u < g.nodes.size(); u++)
            for (int w : g.nodes[u].adj)
                if (w < 0 || w >= int(g.nodes.size()) || g.nodes[w].turn == g.nodes[u].turn)
                    throw std::runtime_error(std::to_string(u) + " -> " + std::to_string(w) + ": turni non alternati (o self-loop)");
        return g;
    }

    std::string text() const {
        std::string s;
        for (size_t u = 0; u < nodes.size(); u++) {
            s += std::to_string(u) + ' ' + std::to_string(nodes[u].turn) + ' ' + (nodes[u].sink < 0 ? '.' : char('0' + nodes[u].sink));
            for (int w : nodes[u].adj) s += ' ' + std::to_string(w);
            s += '\n';
        }
        return s;
    }

    bool acyclic() const {   // DFS con colori
        std::vector<int> c(nodes.size(), 0);
        std::vector<std::pair<int, size_t>> st;
        for (size_t s = 0; s < nodes.size(); s++) {
            if (c[s]) continue;
            st.push_back({int(s), 0}); c[s] = 1;
            while (!st.empty()) {
                auto& [v, i] = st.back();
                if (i == nodes[v].adj.size()) { c[v] = 2; st.pop_back(); continue; }
                int w = nodes[v].adj[i++];
                if (c[w] == 1) return false;
                if (c[w] == 0) { c[w] = 1; st.push_back({w, 0}); }
            }
        }
        return true;
    }
};

// Interfaccia G di §8.1 sul grafo. `log`, se impostato, riceve il preordine delle espansioni (una chiamata di
// terminal() per nodo espanso, sink compresi: stesso criterio di legalMoves() nel Java).
struct GraphGame {
    using Move = int;
    const Graph& g; u8 goal; int cur = 0;
    std::vector<int> st; std::vector<int>* log = nullptr;
    GraphGame(const Graph& graph, u8 goalPlayer, int maxDepth = 1024) : g(graph), goal(goalPlayer), st(maxDepth + 2) {}
    u64 key() const { return u64(cur); }
    bool side_is_goal() const { return g.nodes[cur].turn == goal; }
    int rule50() const { return 0; }                  // nessuna regola delle 50 mosse: ogni mossa è irreversibile
    bool reversible(int) const { return false; }
    static u16 code(int m) { return u16(m); }        // i grafi di test hanno < 65536 vertici
    static int move(u16 c) { return c; }
    Terminal terminal() { if (log) log->push_back(cur); int s = g.nodes[cur].sink; return s < 0 ? Terminal::NONE : s == goal ? Terminal::WIN_GOAL : Terminal::NOWIN_GOAL; }
    Span<int> gen_children(int) { const auto& a = g.nodes[cur].adj; return {a.data(), int(a.size())}; }
    void do_move(int m, int ply) { st[ply] = cur; cur = m; }
    void undo_move(int, int ply) { cur = st[ply]; }
};

// java.util.Random (LCG a 48 bit), quanto basta per Gen.java.
struct JavaRandom {
    u64 seed;
    explicit JavaRandom(long long s) : seed((u64(s) ^ 0x5DEECE66DULL) & ((1ULL << 48) - 1)) {}
    int next(int bits) { seed = (seed * 0x5DEECE66DULL + 0xBULL) & ((1ULL << 48) - 1); return int(seed >> (48 - bits)); }
    int nextInt(int bound) {
        if ((bound & -bound) == bound) return int(((long long)bound * next(31)) >> 31);
        int bits, val;
        do { bits = next(31); val = bits % bound; } while ((long long)bits - val + (bound - 1) > INT_MAX);
        return val;
    }
    double nextDouble() { return double(((long long)next(26) << 27) + next(27)) * (1.0 / 9007199254740992.0); }
};

// Gen.generate(Random, seed, n, deg, cyc, sinks)
inline Graph generate(JavaRandom& r, int n, int deg, double cyc, double sinks) {
    Graph g; g.nodes.resize(n);
    for (int v = 0; v < n; v++) {
        g.nodes[v].turn = 1 + r.nextInt(2);
        g.nodes[v].sink = v > 0 && r.nextDouble() < sinks ? "120"[r.nextInt(3)] - '0' : -1;
    }
    g.nodes[1].turn = 3 - g.nodes[0].turn;
    for (int u = 0; u < n; u++) {
        if (g.nodes[u].sink >= 0) continue;
        for (int k = 1 + r.nextInt(deg); k > 0; k--) {
            bool back = r.nextDouble() < cyc;
            std::vector<int> cand, other;
            for (int w = 0; w < n; w++)
                if (g.nodes[w].turn != g.nodes[u].turn) ((w < u) == back ? cand : other).push_back(w);
            if (cand.empty()) cand = other;
            int w = cand[r.nextInt(int(cand.size()))];
            auto& a = g.nodes[u].adj;
            if (std::find(a.begin(), a.end(), w) == a.end()) a.push_back(w);
        }
    }
    return g;
}

// Gen.generate(seed): n in [10,40], grado in [1,3], cicli in [0,0.7), sink in [0.1,0.4). Con acyclic = true la
// densità di cicli estratta viene sostituita da 0 (variante usata dal confronto Java-C++ di §9.3).
inline Graph generate(long long seed, bool acyclic = false) {
    JavaRandom r(seed);
    int n = 10 + r.nextInt(31), deg = 1 + r.nextInt(3);
    double cyc = r.nextDouble() * 0.7, sinks = 0.1 + r.nextDouble() * 0.3;
    return generate(r, n, deg, acyclic ? 0.0 : cyc, sinks);
}

}  // namespace WinTree

#endif
