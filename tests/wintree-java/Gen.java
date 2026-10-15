import engine.Outcome;
import java.util.*;

// Generatore di game graph casuali con seme (Def. 2.6: turni alternati su ogni arco, nessun self-loop sui
// non-sink, radice non-sink) e parser del formato testuale.
//
// Formato: righe "id turno esito figli...", id = 0..n-1 in ordine, radice = 0; turno 1|2 (PLAYER_1|PLAYER_2);
// esito "." per i non-sink, "1"/"2" = sink vinto da PLAYER_1/PLAYER_2, "0" = sink di patta; "#" = commento.
//
// Uso: java Gen.java SEED                      (parametri derivati dal seme, come in Fuzz)
//      java Gen.java SEED N GRADO CICLI SINK   (CICLI = prob. che un arco punti a un id minore: crea i cicli)
public class Gen {

    public static void main(String[] a) {
        long seed = Long.parseLong(a[0]);
        System.out.print(a.length == 1 ? generate(seed)
            : generate(new Random(seed), seed, Integer.parseInt(a[1]), Integer.parseInt(a[2]),
                       Double.parseDouble(a[3]), Double.parseDouble(a[4])));
    }

    static String generate(long seed) {
        Random r = new Random(seed);
        // n in [10,40]: con n <= 16 il caso P2 non compariva mai in 10^4 grafi
        return generate(r, seed, 10 + r.nextInt(31), 1 + r.nextInt(3), r.nextDouble() * 0.7, 0.1 + r.nextDouble() * 0.3);
    }

    static String generate(Random r, long seed, int n, int deg, double cyc, double sinks) {
        if (n < 2 || deg < 1) throw new IllegalArgumentException("serve n >= 2, grado >= 1");
        int[] turn = new int[n]; char[] win = new char[n];
        for (int v = 0; v < n; v++) {
            turn[v] = 1 + r.nextInt(2);
            win[v] = v > 0 && r.nextDouble() < sinks ? "120".charAt(r.nextInt(3)) : '.';
        }
        turn[1] = 3 - turn[0]; // entrambi i turni presenti: ogni non-sink ha almeno un figlio possibile
        StringBuilder sb = new StringBuilder(String.format(Locale.ROOT,
            "# seed=%d n=%d grado=%d cicli=%.3f sink=%.3f%n", seed, n, deg, cyc, sinks));
        for (int u = 0; u < n; u++) {
            sb.append(u).append(' ').append(turn[u]).append(' ').append(win[u]);
            if (win[u] == '.') {
                LinkedHashSet<Integer> adj = new LinkedHashSet<>();
                for (int k = 1 + r.nextInt(deg); k > 0; k--) {
                    boolean back = r.nextDouble() < cyc;
                    List<Integer> cand = new ArrayList<>(), other = new ArrayList<>();
                    for (int w = 0; w < n; w++)
                        if (turn[w] != turn[u]) ((w < u) == back ? cand : other).add(w);
                    if (cand.isEmpty()) cand = other;
                    adj.add(cand.get(r.nextInt(cand.size())));
                }
                for (int w : adj) sb.append(' ').append(w);
            }
            sb.append('\n');
        }
        return sb.toString();
    }

    // Legge il formato e verifica i vincoli di Def. 2.6; il primo vertice (id 0) è la radice.
    static List<Test.Node> parse(String text) {
        List<String[]> rows = new ArrayList<>();
        for (String line : text.split("\n"))
            if (!line.isBlank() && !line.startsWith("#")) rows.add(line.trim().split("\\s+"));
        List<Test.Node> g = new ArrayList<>();
        for (int i = 0; i < rows.size(); i++) {
            String[] f = rows.get(i);
            if (f.length < 3 || !f[0].equals("" + i) || !f[1].matches("[12]") || !f[2].matches("[.012]"))
                throw new IllegalArgumentException("riga non valida: " + String.join(" ", f));
            g.add(new Test.Node(f[0], player(f[1]), f[2].equals(".") ? null
                : f[2].equals("0") ? Outcome.DRAW : player(f[2])));
        }
        for (int i = 0; i < rows.size(); i++) {
            String[] f = rows.get(i);
            Test.Node u = g.get(i);
            if ((u.sinkWin == null) != (f.length > 3)) throw new IllegalArgumentException(i + ": un non-sink deve avere figli, un sink nessuno");
            for (int j = 3; j < f.length; j++) {
                Test.Node w = g.get(Integer.parseInt(f[j]));
                if (w.turn == u.turn) throw new IllegalArgumentException(i + " -> " + w + ": turni non alternati (o self-loop)");
                u.adj.add(w);
            }
        }
        if (g.isEmpty() || g.get(0).sinkWin != null) throw new IllegalArgumentException("la radice 0 deve essere un non-sink");
        return g;
    }

    static Outcome player(String s) { return s.equals("1") ? Outcome.PLAYER_1 : Outcome.PLAYER_2; }
}
