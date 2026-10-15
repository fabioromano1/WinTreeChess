import engine.Outcome;
import java.util.*;

// Fuzzing su grafi casuali (Gen): per ogni grafo e per ogni goal g in {PLAYER_1, PLAYER_2} esegue una ricerca
// completa dalla radice con Java originale, Java con P1 corretto e trascrizione di Sez. 3, e conta le violazioni di
// solidità (Thm 3.1 / Cor 3.3), completezza alla radice (Thm 3.4) e strategia di patta (Thm 4.8).
// Oracolo: Test.forced (minimo punto fisso). Uso: java Fuzz.java [GRAFI=10000] [SEME=1]; il grafo i ha seme SEME+i.
public class Fuzz {

    static final String[] IMPL = {"java-orig", "java-p1fix", "sez3"};
    static final String[] CHECK = {"TRUE non solido", "FALSE non solido", "radice incompleta (3.4)", "patta (4.8)"};

    public static void main(String[] a) throws Exception {
        int graphs = a.length > 0 ? Integer.parseInt(a[0]) : 10000;
        long seed = a.length > 1 ? Long.parseLong(a[1]) : 1;
        int[][] viol = new int[IMPL.length][CHECK.length];
        int[] nowin = new int[IMPL.length];
        List<List<List<String>>> examples = new ArrayList<>();
        for (String i : IMPL) { List<List<String>> l = new ArrayList<>(); for (String c : CHECK) l.add(new ArrayList<>()); examples.add(l); }
        int runs = 0, rootWins = 0;

        for (int i = 0; i < graphs; i++) {
            long s = seed + i;
            List<Test.Node> g = Gen.parse(Gen.generate(s));
            Test.Node root = g.get(0);
            for (Outcome goal : new Outcome[] {Outcome.PLAYER_1, Outcome.PLAYER_2}) {
                runs++;
                Set<Test.Node> ref = Test.forced(g, goal);
                if (ref.contains(root)) rootWins++;

                engine.Engine e = new engine.Engine(goal); e.solve(root);
                enginep1.Engine f = new enginep1.Engine(goal); f.solve(root);
                Test.Base b = new Test.Base(goal); b.run(root);
                List<Map<Test.Node,Boolean>> res = List.of(Test.outcomes(e), Test.outcomes(f), b.win);

                for (int k = 0; k < IMPL.length; k++) {
                    Map<Test.Node,Boolean> r = res.get(k);
                    boolean rootTrue = Boolean.TRUE.equals(r.get(root));
                    if (!rootTrue) nowin[k]++;
                    boolean[] v = {
                        r.entrySet().stream().anyMatch(x -> Boolean.TRUE.equals(x.getValue()) && !ref.contains(x.getKey())),
                        r.entrySet().stream().anyMatch(x -> Boolean.FALSE.equals(x.getValue()) && ref.contains(x.getKey())),
                        ref.contains(root) && !rootTrue,
                        !rootTrue && drawStrategyFails(root, r, goal),
                    };
                    for (int c = 0; c < CHECK.length; c++)
                        if (v[c]) {
                            viol[k][c]++;
                            if (examples.get(k).get(c).size() < 3) examples.get(k).get(c).add(s + "/" + goal);
                        }
                }
            }
        }

        System.out.printf("Fuzz: %d grafi (semi %d..%d), %d ricerche (2 goal per grafo), eval(radice)=g in %d%n",
            graphs, seed, seed + graphs - 1, runs, rootWins);
        System.out.printf("%-12s %8s", "", "NOWIN");
        for (String c : CHECK) System.out.printf(" %24s", c);
        System.out.println();
        for (int k = 0; k < IMPL.length; k++) {
            System.out.printf("%-12s %8d", IMPL[k], nowin[k]);
            for (int c = 0; c < CHECK.length; c++) System.out.printf(" %24d", viol[k][c]);
            System.out.println();
        }
        for (int k = 0; k < IMPL.length; k++)
            for (int c = 0; c < CHECK.length; c++)
                if (!examples.get(k).get(c).isEmpty())
                    System.out.println("  es. " + IMPL[k] + " / " + CHECK[c] + ": seme/goal " + examples.get(k).get(c));
        // la trascrizione di Sez. 3 è coperta dai teoremi: una sua violazione indica un errore dell'oracolo o del checker
        if (Arrays.stream(viol[2]).sum() > 0) { System.out.println("ERRORE: violazioni sulla trascrizione di Sez. 3"); System.exit(1); }
    }

    // Model checker del Thm 4.8 sugli esiti finali di una ricerca completa con radice non TRUE (modalità NOWIN):
    // il difensore (turno != g) sceglie solo figli visitati con esito NIL o FALSE, l'attaccante qualsiasi figlio.
    // Fallisce se un nodo del difensore raggiunto non ha una tale scelta o se è raggiungibile un sink vinto da g.
    // Da un nodo FALSE il difensore sceglie solo figli FALSE: il Thm 4.8 copre i cammini tutti NIL, e un figlio
    // WHITE di un nodo FALSE può avere eval = g. La radice conta come NIL: Java la marca FALSE se un figlio è NIL.
    static boolean drawStrategyFails(Test.Node root, Map<Test.Node,Boolean> res, Outcome g) {
        Deque<Test.Node> todo = new ArrayDeque<>(List.of(root));
        Set<Test.Node> seen = new HashSet<>(todo);
        while (!todo.isEmpty()) {
            Test.Node v = todo.pop();
            if (v.adj.isEmpty()) { if (v.sinkWin == g) return true; continue; }
            boolean nilOk = v == root || res.get(v) == null;
            List<Test.Node> next = v.turn == g ? List.copyOf(v.adj)
                : v.adj.stream().filter(w -> Boolean.FALSE.equals(res.get(w)) || nilOk && res.containsKey(w) && res.get(w) == null).toList();
            if (next.isEmpty()) return true;
            for (Test.Node w : next) if (seen.add(w)) todo.push(w);
        }
        return false;
    }
}
