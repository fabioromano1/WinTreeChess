#!/usr/bin/env python3
"""G3 tuning: riepilogo delle righe `info string prover: ...` (ProverLog=true) nel PGN di tests/g3/sprt.sh, dal
commento della mossa (`line="..."`, match_line): avvii, esiti certificati, ricerche senza esito e tempo speso senza
esito per prover-auto. fastchess 1.8.2 conserva solo l'ULTIMA riga che combacia per mossa: la riga `start` viene
sovrascritta da `no result ...` o `proven ...`, quindi un avvio = una riga finale (il budget in ms resta noto solo se
la riga `start` è l'unica della mossa). Uso: prover_log.py <games.pgn> [starts.csv]  (CSV: una riga per avvio)."""
import csv, re, statistics as st, sys
from collections import Counter
from pgn_games import games

AUTO, OFF = "prover-auto", "prover-off"
START = re.compile(r"info string prover: start (\w+) budget (?:(\d+) ms )?(\d+) do_move")
NORES = re.compile(r"info string prover: no result after (\d+) do_move in (\d+) ms \(([^)]*)\)")
PROVEN = re.compile(r"info string prover: proven [^\"]*? nodes (\d+) time (\d+) ms")
INFO = re.compile(r"^([+-]?(?:M\d+|\d+\.\d+))/(\d+) ([\d.]+)s, tl=(-?[\d.]+)s, n=(\d+)")

def rows(pgn):
    """Una riga per avvio: round, white, ply, budget ms, do_move, ms, esito (proven/budget/...), tempo della mossa."""
    for g in games(pgn):
        t = g["tags"]; start = t.get("FEN", "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1")
        stm_white = start.split()[1] == "w"; white = t["White"]
        for i, c in enumerate(g["comments"]):
            side = white if (i % 2 == 0) == stm_white else (OFF if white == AUTO else AUTO)
            s = START.search(c); m = INFO.match(c); n = NORES.search(c); p = PROVEN.search(c)
            if not (s or n or p): continue
            yield dict(round=t["Round"], white=white, side=side, ply=i, mode=s[1] if s else "?", budget_ms=int(s[2]) if s and s[2] else -1,
                       cap=int(s[3]) if s else -1, do_move=int((n or p)[1]) if (n or p) else -1, ms=int((n or p)[2]) if (n or p) else -1,
                       outcome="proven" if p else n[3] if n else "start only", move_s=float(m[3]) if m else -1,
                       timeloss="loses on time" in c)

if __name__ == "__main__":
    if len(sys.argv) < 2:   # self-check sul formato di fastchess (solo l'ultima riga match_line per mossa)
        import os, tempfile
        f = tempfile.NamedTemporaryFile("w", suffix=".pgn", delete=False)
        f.write('[White "prover-auto"]\n[Round "1"]\n[Result "1-0"]\n[FEN "8/8/8/8/8/8/8/K6k w - - 0 1"]\n\n1. a1a2 {+1.00/9 0.040s, tl=1s, n=5, line="info string prover: no result after 60000 do_move in 31 ms (budget)"} h1h2 {0.00/9 0.2s, tl=1s, n=9}\n2. a2a3 {+M4/1 0.001s, tl=1s, n=1, line="info string prover: proven mate in 4 move a2a3 nodes 400 time 1 ms"} 1-0\n')
        f.close(); r = list(rows(f.name)); os.unlink(f.name)
        assert [x["outcome"] for x in r] == ["budget", "proven"] and r[0]["ms"] == 31 and r[0]["budget_ms"] == -1 and r[1]["do_move"] == 400, r
        print("ok"); sys.exit()
    R = list(rows(sys.argv[1]))
    if len(sys.argv) > 2:
        with open(sys.argv[2], "w", newline="") as f:
            w = csv.DictWriter(f, fieldnames=list(R[0].keys()) if R else ["round"]); w.writeheader(); w.writerows(R)
    ngames = sum(1 for _ in games(sys.argv[1]))
    by = Counter(x["outcome"] for x in R)
    g_start = len({(x["round"], x["white"]) for x in R}); g_res = len({(x["round"], x["white"]) for x in R if x["outcome"] == "proven"})
    nores = [x for x in R if x["outcome"] != "proven"]
    ms = [x["ms"] for x in nores if x["ms"] >= 0]
    over = [x["ms"] - x["budget_ms"] for x in nores if x["ms"] >= 0 and x["budget_ms"] >= 0 and x["outcome"] == "budget"]
    dm = [x["do_move"] for x in nores if x["do_move"] >= 0]
    print(f"Partite: {ngames}; avvii del prover: {len(R)} in {g_start} partite; esiti certificati: {by['proven']} in {g_res} partite")
    print(f"Senza esito: {len(nores)} ({dict((k, v) for k, v in by.items() if k != 'proven')}); tempo speso senza esito: "
          f"totale {sum(ms) / 1000:.1f} s, mediana {st.median(ms) if ms else 0:.0f} ms, max {max(ms) if ms else 0} ms; "
          f"per partita con avvio {sum(ms) / max(g_start, 1):.0f} ms")
    if dm: print(f"do_move senza esito: mediana {st.median(dm):.0f}, max {max(dm)}")
    if over: print(f"Sforamento della quota (ms - budget, solo 'budget'): mediana {st.median(over):.0f} ms, p99 {sorted(over)[int(len(over) * .99)]} ms, max {max(over)} ms")
    print(f"Modalita' degli avvii: {Counter(x['mode'] for x in R)}; avvii nella mossa di una perdita per il tempo: {sum(x['timeloss'] for x in R)}")
