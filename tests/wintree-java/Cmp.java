import engine.Outcome;
import engine.Position;
import java.util.*;

// M1 (§9.3): esiti del Java con P1 corretto (enginep1) per il confronto con src/wintree. Per ogni seme stampa il
// grafo standard (Gen.generate(seme)) e la variante con cicli=0, ciascuno come
//   G seme std|acyc, righe del grafo, poi per goal 1 e 2: "R g preordine..." e "S g stato per vertice"
// (stato: - mai visitato, N NIL, F FALSE, W<d> TRUE). Il preordine è l'ordine delle chiamate a legalMoves(),
// una per invocazione di winTree/solve. Uso: java Cmp.java [GRAFI=10000] [SEME=1]
public class Cmp {

    static final List<Node> log = new ArrayList<>();

    static final class Node implements Position {
        final int id; final Outcome turn; final Outcome sinkWin; final LinkedHashSet<Node> adj = new LinkedHashSet<>();
        Node(int id, Outcome turn, Outcome sinkWin) { this.id = id; this.turn = turn; this.sinkWin = sinkWin; }
        @SuppressWarnings("unchecked") public <T extends Position> Set<T> legalMoves() { log.add(this); return (Set<T>) adj; }
        public Outcome turn() { return turn; }
        public Outcome win() { return sinkWin; }
    }

    static List<Node> parse(String text) {
        List<String[]> rows = new ArrayList<>();
        for (String line : text.split("\n")) if (!line.isBlank() && !line.startsWith("#")) rows.add(line.trim().split("\\s+"));
        List<Node> g = new ArrayList<>();
        for (String[] f : rows)
            g.add(new Node(g.size(), Gen.player(f[1]), f[2].equals(".") ? null : f[2].equals("0") ? Outcome.DRAW : Gen.player(f[2])));
        for (int i = 0; i < rows.size(); i++) for (int j = 3; j < rows.get(i).length; j++) g.get(i).adj.add(g.get(Integer.parseInt(rows.get(i)[j])));
        return g;
    }

    public static void main(String[] a) throws Exception {
        int graphs = a.length > 0 ? Integer.parseInt(a[0]) : 10000;
        long seed = a.length > 1 ? Long.parseLong(a[1]) : 1;
        StringBuilder out = new StringBuilder();
        for (int i = 0; i < graphs; i++) {
            long s = seed + i;
            Random r = new Random(s);
            int n = 10 + r.nextInt(31), deg = 1 + r.nextInt(3); double cyc = r.nextDouble() * 0.7, sinks = 0.1 + r.nextDouble() * 0.3;
            String[] texts = {Gen.generate(s), Gen.generate(r, s, n, deg, 0.0, sinks)};
            for (int k = 0; k < 2; k++) {
                out.append("G ").append(s).append(k == 0 ? " std\n" : " acyc\n").append(texts[k]);
                List<Node> g = parse(texts[k]);
                for (Outcome goal : new Outcome[] {Outcome.PLAYER_1, Outcome.PLAYER_2}) {
                    log.clear();
                    enginep1.Engine e = new enginep1.Engine(goal); e.solve(g.get(0));
                    Map<Position,Object> colors = Test.field(e, "colors"), evals = Test.field(e, "evals");
                    int gi = goal == Outcome.PLAYER_1 ? 1 : 2;
                    out.append("R ").append(gi);
                    for (Node v : log) out.append(' ').append(v.id);
                    out.append("\nS ").append(gi);
                    for (Node v : g) {
                        Object ev = evals.get(v);
                        if (ev == null) out.append(colors.containsKey(v) ? " N" : " -");
                        else {
                            java.lang.reflect.Field w = ev.getClass().getDeclaredField("win"), d = ev.getClass().getDeclaredField("depth");
                            w.setAccessible(true); d.setAccessible(true);
                            out.append(w.getBoolean(ev) ? " W" + (int) d.getDouble(ev) : " F");
                        }
                    }
                    out.append('\n');
                }
            }
            out.append('\n');
            if (out.length() > 1 << 20) { System.out.print(out); out.setLength(0); }
        }
        System.out.print(out);
    }
}
