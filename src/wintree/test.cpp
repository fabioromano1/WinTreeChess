// Test di M1 (spec §9.2-9.3): regressioni P1a/P1b/P2/Sanity, fuzzing su grafi casuali con seme contro
// l'oracolo a minimo punto fisso e la trascrizione di Sez. 3, budget casuali, interruzione, confronto col
// Java con P1 corretto (file prodotto da tests/wintree-java/Cmp.java).
// Uso: wintree_test [GRAFI=10000] [SEME=1]   |   wintree_test --java FILE
#include "graph.h"

#include <algorithm>
#include <fstream>
#include <iostream>
#include <map>

using namespace WinTree;

static const int INF = 1 << 29;
static const int PT_LOG2 = 10;  // 1024 entry: chiavi = id, nessuna collisione di indice fino a 1024 vertici

// Oracolo: d_g come minimo punto fisso (Def. 2.8); eval(v) = g <=> d_g(v) < INF (Lemma 2.9).
static std::vector<int> oracle(const Graph& g, int goal) {
    std::vector<int> d(g.nodes.size(), INF);
    for (bool ch = true; ch;) {
        ch = false;
        for (size_t v = 0; v < g.nodes.size(); v++) {
            const auto& n = g.nodes[v]; int x;
            if (n.sink >= 0) x = n.sink == goal ? 0 : INF;
            else { x = n.turn == goal ? INF : 0;
                   for (int w : n.adj) x = n.turn == goal ? std::min(x, d[w] + 1) : std::max(x, d[w] + 1); }
            if (x < d[v]) { d[v] = x; ch = true; }
        }
    }
    return d;
}

// Sez. 3 letterale: win 0 FALSE, 1 TRUE, 2 NIL; color 0 mai visitato, 1 GRAY, 2 WHITE, 3 BLACK.
struct Sez3 {
    const Graph& g; int goal; std::vector<int> color, win;
    Sez3(const Graph& gr, int gl) : g(gr), goal(gl), color(gr.nodes.size()), win(gr.nodes.size(), 2) {}
    void run(int v) {
        color[v] = 1; const auto& n = g.nodes[v]; int vw;
        if (n.sink >= 0) vw = n.sink == goal;
        else {
            vw = n.turn != goal;
            for (size_t i = 0; i < n.adj.size() && !(vw != (n.turn != goal) && vw != 2); i++) {
                int w = n.adj[i], cw;
                if (color[w] == 1) cw = 2; else { if (color[w] != 3) run(w); cw = win[w]; }
                if (cw != (n.turn != goal)) vw = cw;
            }
        }
        win[v] = vw; color[v] = vw == 2 ? 2 : 3;
    }
};

struct State { char c; int d; };  // c: '-' mai visitato, 'N' UNKNOWN_*, 'F' PROVEN_NOWIN, 'W' PROVEN_WIN con d
static State state(const ProofTable& pt, int id, int goal) {
    u64 data = pt.probe(u64(id));
    if (!data || int(data >> 61 & 1) != (goal & 1)) return {'-', 0};
    u32 h; Result r = ProofTable::unpack(data, h);
    return {r.status == Status::PROVEN_WIN ? 'W' : r.status == Status::PROVEN_NOWIN ? 'F' : 'N', r.d};
}
static std::string tok(State s) { return s.c == 'W' ? "W" + std::to_string(s.d) : std::string(1, s.c); }

// Model checker del Thm 4.8 (come Fuzz.drawStrategyFails): il difensore sceglie figli NIL (da un nodo NIL) o
// FALSE, l'attaccante qualsiasi; fallisce se manca la scelta o se si raggiunge un sink vinto da g.
static bool drawStrategyFails(const Graph& g, const std::vector<State>& st, int goal) {
    std::vector<int> todo{0}; std::vector<char> seen(g.nodes.size(), 0); seen[0] = 1;
    while (!todo.empty()) {
        int v = todo.back(); todo.pop_back(); const auto& n = g.nodes[v];
        if (n.sink >= 0) { if (n.sink == goal) return true; continue; }
        std::vector<int> next;
        for (int w : n.adj)
            if (n.turn == goal || st[w].c == 'F' || (st[v].c == 'N' && st[w].c == 'N')) next.push_back(w);
        if (next.empty()) return true;
        for (int w : next) if (!seen[w]) { seen[w] = 1; todo.push_back(w); }
    }
    return false;
}

struct Run { Result root; u64 nodes; bool aborted; std::vector<State> st; std::vector<int> pre; };
static Run run(const Graph& g, int goal, ProofTable& pt, const Budget& b = Budget()) {
    GraphGame game(g, u8(goal), b.maxDepth); Run r; game.log = &r.pre;
    Prover<GraphGame> p(game, pt, u8(goal));
    r.root = p.solve(b); r.nodes = p.nodes(); r.aborted = p.aborted();
    for (size_t v = 0; v < g.nodes.size(); v++) r.st.push_back(state(pt, int(v), goal));
    return r;
}

static int failures = 0;
static void expect(bool ok, const std::string& what) { std::cout << "  " << (ok ? "OK   " : "FAIL ") << what << "\n"; failures += !ok; }

// Grafi dell'Appendice A nel formato di Gen (i sink hanno il turno opposto al padre, irrilevante per l'algoritmo).
static void regression() {
    struct Case { const char* name; const char* text; const char* expect; } cases[] = {
        {"P1a (AND non solido con O1)", "0 1 . 1\n1 2 . 2 3\n2 1 . 4 5\n3 1 . 6\n4 2 . 3\n5 2 1\n6 2 2\n", "F F W1 F F W0 F"},
        {"P1b (OR non solido con O1)",  "0 2 . 1\n1 1 . 2 3\n2 2 . 4 5\n3 2 . 6\n4 1 . 3\n5 1 2\n6 1 1\n", "W3 W2 F W1 W2 F W0"},
        {"P2 (dejavu incompleto)",      "0 1 . 1 2\n1 2 . 3 4\n2 2 . 5\n3 1 . 6 7\n4 1 2\n5 1 . 8\n6 2 . 9 5\n7 2 1\n8 2 . 3\n9 1 . 8\n", "W5 F W4 W1 F W3 N W0 W2 N"},
        {"Sanity (ciclo + vittoria)",   "0 2 . 1\n1 1 . 2 3\n2 2 . 1\n3 2 1\n", "W2 W1 N W0"},
    };
    for (const Case& c : cases) {
        std::istringstream in(c.text); Graph g = Graph::parse(in);
        ProofTable pt(PT_LOG2); Run r = run(g, 1, pt);
        std::string got; for (const State& s : r.st) got += (got.empty() ? "" : " ") + tok(s);
        std::cout << "=== " << c.name << " (g=1): " << got << "\n";
        expect(got == c.expect, std::string("atteso ") + c.expect);
        std::vector<int> d = oracle(g, 1);
        for (size_t v = 0; v < g.nodes.size(); v++)
            expect(r.st[v].c != 'W' || (d[v] < INF && r.st[v].d >= d[v]), "vertice " + std::to_string(v) + ": PROVEN_WIN => oracolo, d >= d_g");
    }
}

static void fuzz(int graphs, long long seed) {
    int rootWins = 0, nowin = 0, draw48 = 0, sez3Nil = 0, cppNil = 0, budgetRoot = 0, cycleRoot = 0;
    u64 maxNodes = 0, splitmix = u64(seed) * 0x9E3779B97F4A7C15ULL;
    ProofTable pt(PT_LOG2);
    for (int i = 0; i < graphs; i++) {
        Graph g = generate(seed + i);
        for (int goal = 1; goal <= 2; goal++) {
            std::vector<int> d = oracle(g, goal);
            rootWins += d[0] < INF;
            Sez3 s3(g, goal); s3.run(0);
            pt.clear(); Run r = run(g, goal, pt);
            maxNodes = std::max(maxNodes, r.nodes);
            // solidità (Thm 3.1, Cor 3.3), Thm 3.6, completezza alla radice (Thm 3.4), nessun taglio senza budget
            for (size_t v = 0; v < g.nodes.size(); v++) {
                WT_ASSERT(r.st[v].c != 'W' || (d[v] < INF && r.st[v].d >= d[v]));
                WT_ASSERT(r.st[v].c != 'F' || d[v] == INF);
            }
            WT_ASSERT((d[0] < INF) == (r.root.status == Status::PROVEN_WIN));
            WT_ASSERT(r.root.status != Status::UNKNOWN_BUDGET && !r.root.cut && !r.aborted);
            WT_ASSERT((s3.win[0] == 1) == (r.root.status == Status::PROVEN_WIN));   // Sez. 3 e C++ concordano sulle vittorie
            if (r.root.status != Status::PROVEN_WIN) { nowin++; draw48 += drawStrategyFails(g, r.st, goal); }
            sez3Nil += s3.win[0] == 2; cppNil += r.root.status == Status::UNKNOWN_CYCLE;
            // seconda chiamata a tabella piena: stesso esito, nessuna espansione (§9.2 punto 7)
            Run r2 = run(g, goal, pt);
            WT_ASSERT(r2.root.status == r.root.status && r2.root.d == r.root.d && r2.nodes == 0);
            // budget casuale: mai un PROVEN errato; abort => radice UNKNOWN_BUDGET; UNKNOWN_CYCLE => eval != g
            Budget b; splitmix += 0x9E3779B97F4A7C15ULL; u64 x = mix64(splitmix);
            b.maxNodes = 1 + x % 50; b.maxDepth = 1 + int((x >> 32) % 15);
            pt.clear(); Run rb = run(g, goal, pt, b);
            for (size_t v = 0; v < g.nodes.size(); v++) {
                WT_ASSERT(rb.st[v].c != 'W' || (d[v] < INF && rb.st[v].d >= d[v]));
                WT_ASSERT(rb.st[v].c != 'F' || d[v] == INF);
            }
            WT_ASSERT(rb.nodes <= b.maxNodes);
            WT_ASSERT(!rb.aborted || (rb.root.status == Status::UNKNOWN_BUDGET && rb.root.cut));
            WT_ASSERT(rb.root.status != Status::UNKNOWN_CYCLE || d[0] == INF);
            budgetRoot += rb.root.status == Status::UNKNOWN_BUDGET; cycleRoot += rb.root.status == Status::UNKNOWN_CYCLE;
            // stop già alzato e scadenza passata: controllo cooperativo ogni 1024 nodi
            std::atomic<bool> stop{true}; Budget bs; bs.stop = &stop; pt.clear(); Run rs = run(g, goal, pt, bs);
            WT_ASSERT(rs.nodes <= 1024 && (!rs.aborted || rs.root.status == Status::UNKNOWN_BUDGET));
            Budget bt; bt.deadline = Clock::now(); pt.clear(); Run rt = run(g, goal, pt, bt);
            WT_ASSERT(rt.nodes <= 1024 && (!rt.aborted || rt.root.status == Status::UNKNOWN_BUDGET));
        }
    }
    std::cout << "Fuzz: " << graphs << " grafi (semi " << seed << ".." << seed + graphs - 1 << "), " << 2 * graphs
              << " ricerche, eval(radice)=g in " << rootWins << "\n"
              << "  senza budget: NOWIN " << nowin << " (radice UNKNOWN_CYCLE " << cppNil << ", Sez. 3 NIL " << sez3Nil
              << "), PROVEN in contrasto con l'oracolo 0, radici incomplete (Thm 3.4) 0, tagli 0, patta (Thm 4.8) fallita "
              << draw48 << ", max espansioni " << maxNodes << "\n"
              << "  con budget casuale (nodi 1..50, profondità 1..15): PROVEN errati 0, radice UNKNOWN_BUDGET " << budgetRoot
              << ", UNKNOWN_CYCLE " << cycleRoot << " (tutte con eval != g)\n";
    expect(draw48 == 0, "Thm 4.8: nessuna strategia di patta fallita");
}

// Interruzione a 1024 nodi su un grafo grande (il controllo di stop/deadline è ogni 1024 espansioni). La ricerca
// completa su questo grafo non termina in tempi utili (riesplorazione esponenziale dei WHITE, §2.9): non si esegue.
static void bigStop() {
    JavaRandom r(12345); Graph g = generate(r, 3000, 3, 0.5, 0.05);
    std::atomic<bool> stop{true}; Budget b; b.stop = &stop; ProofTable pt(12); Run rs = run(g, 1, pt, b);
    std::cout << "Grafo n=3000 con stop alzato: espansioni " << rs.nodes << ", interrotta " << rs.aborted << "\n";
    expect(rs.aborted && rs.nodes == 1024 && rs.root.status == Status::UNKNOWN_BUDGET, "stop cooperativo al 1024-esimo nodo");
}

// Confronto con il Java con P1 corretto (§9.3): file di Cmp.java. Grafi aciclici: identici (stati, d, preordine).
// Ciclici: win uguali sui PROVEN comuni salvo errori del Java (P2b). Il contenimento "PROVEN C++ ⊇ PROVEN Java" di
// §9.3 non vale in generale: dove il Java (incompleto, P2) non chiude un nodo OR prosegue con altri figli e può
// dimostrarli; il C++ si ferma al primo testimone. Si riporta il conteggio con esempi; la radice è a parte (il
// Java marca FALSE una radice NIL: deviazione 3 di §2.6).
static void javaCompare(const char* file) {
    std::ifstream in(file); if (!in) { std::cerr << "non apro " << file << "\n"; failures++; return; }
    int blocks = 0, genMismatch = 0, exact = 0, exactFail = 0, cyclic = 0, contain = 0, rootDev = 0, winDiff = 0, javaWrong = 0;
    std::vector<std::string> examples;
    std::string line;
    while (std::getline(in, line)) {
        if (line.rfind("G ", 0) != 0) continue;
        std::istringstream hs(line.substr(2)); long long seed; std::string kind; hs >> seed >> kind;
        Graph g = Graph::parse(in);
        blocks++; genMismatch += !(generate(seed, kind == "acyc") == g);
        bool acyc = g.acyclic(); cyclic += !acyc;
        for (int goal = 1; goal <= 2; goal++) {
            std::getline(in, line); std::istringstream rs(line.substr(4)); std::vector<int> pre; for (int v; rs >> v;) pre.push_back(v);
            std::getline(in, line); std::istringstream ss(line.substr(4)); std::vector<std::string> jst; for (std::string t; ss >> t;) jst.push_back(t);
            ProofTable pt(PT_LOG2); Run r = run(g, goal, pt);
            std::vector<std::string> cst; for (const State& s : r.st) cst.push_back(tok(s));
            if (acyc) { exact++; if (pre != r.pre || jst != cst) { exactFail++; std::cout << "  DIFF seme " << seed << " goal " << goal << "\n"; } continue; }
            std::vector<int> d = oracle(g, goal);
            for (size_t v = 0; v < g.nodes.size(); v++) {
                bool jp = jst[v][0] == 'W' || jst[v][0] == 'F', cp = cst[v][0] == 'W' || cst[v][0] == 'F';
                if (jp && !cp) { (v == 0 ? rootDev : contain)++; if (v && examples.size() < 3) examples.push_back(std::to_string(seed) + "/" + kind + "/g" + std::to_string(goal) + "/v" + std::to_string(v)); }
                if (jp && cp && jst[v][0] != cst[v][0]) { winDiff++; javaWrong += (jst[v][0] == 'W') != (d[v] < INF);
                    std::cout << "  win diverso: seme " << seed << " goal " << goal << " vertice " << v << " java=" << jst[v] << " cpp=" << cst[v] << "\n"; }
            }
        }
    }
    std::cout << "Java-C++: " << blocks << " grafi, generatore diverso " << genMismatch << "; aciclici " << exact / 2
              << " (ricerche diverse " << exactFail << "); ciclici " << cyclic << ": PROVEN Java non PROVEN in C++ " << contain
              << " (radici FALSE nel Java e UNKNOWN_CYCLE nel C++: " << rootDev << "), win diversi " << winDiff << " (di cui errori del Java " << javaWrong << ")\n";
    expect(blocks > 0 && genMismatch == 0, "generatore C++ identico a Gen.java");
    expect(exactFail == 0, "grafi aciclici: risultati identici al Java con P1 corretto");
    if (!examples.empty()) { std::cout << "  esempi di PROVEN Java non PROVEN in C++ (seme/variante/goal/vertice):"; for (const auto& e : examples) std::cout << ' ' << e; std::cout << "\n"; }
    expect(winDiff == javaWrong, "grafi ciclici: ogni disaccordo sui PROVEN è un errore del Java (P2b)");
}

int main(int argc, char** argv) {
    if (argc == 3 && std::string(argv[1]) == "--java") javaCompare(argv[2]);
    else {
        regression();
        fuzz(argc > 1 ? std::atoi(argv[1]) : 10000, argc > 2 ? std::atoll(argv[2]) : 1);
        bigStop();
    }
    std::cout << (failures ? "FALLITI: " + std::to_string(failures) : std::string("tutto OK")) << "\n";
    return failures ? 1 : 0;
}
