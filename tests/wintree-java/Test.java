import engine.*;
import java.lang.reflect.Field;
import java.util.*;

// Harness (Appendice A): esegue l'Engine Java originale e la copia con P1 e P2 corretti (enginep1) su grafi
// sintetici e confronta con (a) un solver a minimo punto fisso e (b) una trascrizione letterale di
// WinTreeRecursion (Sez. 3). Esce con codice 1 se P1a/P1b o P2 non risultano corretti.
public class Test {

    static final class Node implements Position {
        final String name; final Outcome turn; final Outcome sinkWin; // sinkWin == null -> non è un sink
        final LinkedHashSet<Node> adj = new LinkedHashSet<>();
        Node(String name, Outcome turn, Outcome sinkWin) { this.name = name; this.turn = turn; this.sinkWin = sinkWin; }
        Node to(Node... ns) { adj.addAll(Arrays.asList(ns)); return this; }
        @SuppressWarnings("unchecked") public <T extends Position> Set<T> legalMoves() { return (Set<T>) adj; }
        public Outcome turn() { return turn; }
        public Outcome win() { return sinkWin; }
        public String toString() { return name; }
    }

    // riferimento: minimo punto fisso di "g può forzare un sink con win = g"
    static Set<Node> forced(Collection<Node> all, Outcome g) {
        Set<Node> w = new HashSet<>();
        boolean changed = true;
        while (changed) {
            changed = false;
            for (Node v : all) {
                if (w.contains(v)) continue;
                boolean ok;
                if (v.adj.isEmpty()) ok = v.sinkWin == g;
                else if (v.turn == g) ok = v.adj.stream().anyMatch(w::contains);
                else ok = v.adj.stream().allMatch(w::contains);
                if (ok) { w.add(v); changed = true; }
            }
        }
        return w;
    }

    // Sez. 3 del paper, letterale (NIL = null; colori: null/GRAY/WHITE/BLACK)
    static final class Base {
        final Outcome g; final Map<Node,String> color = new HashMap<>(); final Map<Node,Boolean> win = new HashMap<>();
        Base(Outcome g) { this.g = g; }
        void run(Node v) {
            color.put(v, "GRAY");
            Boolean vwin;
            if (v.adj.isEmpty()) vwin = v.sinkWin == g;
            else {
                vwin = v.turn != g; // OR: FALSE, AND: TRUE
                boolean end = false;
                for (Iterator<Node> it = v.adj.iterator(); !end && it.hasNext();) {
                    Node w = it.next();
                    Boolean cw;
                    if ("GRAY".equals(color.get(w))) cw = null;
                    else { if (!"BLACK".equals(color.get(w))) run(w); cw = win.get(w); }
                    if (!Objects.equals(cw, v.turn != g)) { vwin = cw; if (vwin != null) end = true; }
                }
            }
            win.put(v, vwin);
            color.put(v, vwin == null ? "WHITE" : "BLACK");
        }
    }

    // e: engine.Engine o enginep1.Engine (stessi campi privati)
    @SuppressWarnings("unchecked")
    static Map<Position,Object> field(Object e, String f) throws Exception {
        Field fl = e.getClass().getDeclaredField(f); fl.setAccessible(true); return (Map<Position,Object>) fl.get(e);
    }
    static String javaResult(Object e, Node n) throws Exception {
        Object c = field(e, "colors").get(n); Object ev = field(e, "evals").get(n);
        return c + (ev == null ? "/NIL" : "/" + ev);
    }
    // esiti nello stesso formato di Base.win: chiave assente = mai visitato, valore null = NIL (WHITE)
    static Map<Node,Boolean> outcomes(Object e) throws Exception {
        Map<Position,Object> evals = field(e, "evals");
        Map<Node,Boolean> r = new HashMap<>();
        for (Position p : field(e, "colors").keySet()) {
            Object ev = evals.get(p);
            if (ev == null) { r.put((Node) p, null); continue; }
            Field w = ev.getClass().getDeclaredField("win"); w.setAccessible(true);
            r.put((Node) p, w.getBoolean(ev));
        }
        return r;
    }

    static int failures = 0;
    static void expect(boolean ok, String what) {
        System.out.println("  " + (ok ? "OK   " : "FAIL ") + what);
        if (!ok) failures++;
    }

    // stampa i tre risultati e ritorna gli esiti della copia con P1 corretto
    static Map<Node,Boolean> report(String title, Node root, List<Node> all, Outcome g, Node... check) throws Exception {
        System.out.println("=== " + title + " (g=" + g + ", root=" + root + ")");
        Engine e = new Engine(g); e.solve(root);
        enginep1.Engine f = new enginep1.Engine(g); f.solve(root);
        Base b = new Base(g); b.run(root);
        Set<Node> ref = forced(all, g);
        for (Node n : check)
            System.out.printf("  %-3s java=%-38s p1fix=%-38s base=%s/%s ref(eval==g)=%s%n", n, javaResult(e, n),
                javaResult(f, n), b.color.get(n), b.win.get(n), ref.contains(n));
        return outcomes(f);
    }

    public static void main(String[] a) throws Exception {
        Outcome P1 = Outcome.PLAYER_1, P2 = Outcome.PLAYER_2;

        // P1a: il nodo AND v diventa TRUE sebbene il figlio q3 (risolto FALSE durante la ricorsione del fratello q2) lo confuti
        {
            Node s1 = new Node("s1", P2, P1), s2 = new Node("s2", P1, P2);
            Node q3 = new Node("q3", P1, null).to(s2);
            Node r  = new Node("r",  P2, null).to(q3);
            Node q2 = new Node("q2", P1, null).to(r, s1);
            Node v  = new Node("v",  P2, null).to(q2, q3);
            Node v0 = new Node("v0", P1, null).to(v);
            Map<Node,Boolean> f = report("P1a unsound TRUE at AND node", v0, List.of(s1, s2, q3, r, q2, v, v0), P1, v0, v, q2, q3);
            expect(Boolean.FALSE.equals(f.get(v0)) && Boolean.FALSE.equals(f.get(v)), "P1a risolto in p1fix: v0, v FALSE");
        }
        // P1b: il nodo OR v diventa FALSE sebbene il figlio q3 (risolto TRUE durante la ricorsione del fratello q2) vinca
        {
            Node s1 = new Node("s1", P1, P2), s2 = new Node("s2", P2, P1);
            Node q3 = new Node("q3", P2, null).to(s2);
            Node r  = new Node("r",  P1, null).to(q3);
            Node q2 = new Node("q2", P2, null).to(r, s1);
            Node v  = new Node("v",  P1, null).to(q2, q3);
            Node v0 = new Node("v0", P2, null).to(v);
            Map<Node,Boolean> f = report("P1b unsound FALSE at OR node", v0, List.of(s1, s2, q3, r, q2, v, v0), P1, v0, v, q2, q3);
            expect(Boolean.TRUE.equals(f.get(v0)) && Boolean.TRUE.equals(f.get(v)), "P1b risolto in p1fix: v0, v TRUE");
        }
        // P2: il salto dejavu perde la completezza alla radice (Thm 3.4)
        {
            Node y = new Node("y", P2, P1), z = new Node("z", P1, P2);
            Node a2 = new Node("a2", P1, null), q = new Node("q", P2, null).to(a2);
            Node r2 = new Node("r2", P1, null).to(q);
            Node p  = new Node("p",  P1, null).to(q);
            Node r  = new Node("r",  P2, null).to(r2, p);
            a2.to(r, y);
            Node b  = new Node("b",  P2, null).to(a2, z);
            Node p0 = new Node("p0", P2, null).to(p);
            Node v0 = new Node("v0", P1, null).to(b, p0);
            Map<Node,Boolean> f = report("P2 dejavu incompleteness", v0, List.of(y, z, a2, q, r2, p, r, b, p0, v0), P1, v0, p0, p, q, a2, b);
            expect(Boolean.TRUE.equals(f.get(v0)), "P2 risolto in p1fix: v0 TRUE");

            Engine own = new Engine(P1); own.solve(v0);
            Engine opp = new Engine(P2); opp.solve(v0);
            try { System.out.println("  bestMove = " + Engine.bestMove(v0, own, opp)); }
            catch (Throwable t) { System.out.println("  bestMove threw " + t); }
            try { System.out.println("  notLosingMove = " + Engine.notLosingMove(v0, opp)); }
            catch (Throwable t) { System.out.println("  notLosingMove threw " + t); }
        }
        // Sanità: vittoria forzata con un ciclo; i tre metodi concordano sulla radice
        {
            Node s = new Node("s", P2, P1);
            Node c1 = new Node("c1", P1, null), c2 = new Node("c2", P2, null).to(c1);
            c1.to(c2, s);
            Node v0 = new Node("v0", P2, null).to(c1);
            Map<Node,Boolean> f = report("Sanity cycle + win", v0, List.of(s, c1, c2, v0), P1, v0, c1, c2);
            expect(Boolean.TRUE.equals(f.get(v0)), "Sanity: v0 TRUE in p1fix");
        }
        if (failures > 0) System.exit(1);
    }
}
