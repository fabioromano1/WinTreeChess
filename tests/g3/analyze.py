#!/usr/bin/env python3
"""G3: statistiche di un PGN di tests/g3/sprt.sh (fastchess, notation=uci, nodes, timeleft, match_line del prover).
(A) per motore: tempo per mossa, profondità, nodi, perdite per il tempo; (B) partite con `info string prover`, coppie
con esito diverso dalla speculare; (C) classi di posizioni (§9.5): TB, matto lungo (N >= LONG), altro; con <stockfish>
ogni mossa provata viene ricercata con Prover=off
(nodi = mediana dei nodi di prover-off nella partita) per contare le mosse diverse da Stockfish.
Uso: analyze.py <games.pgn> [stockfish] [LONG=10]"""
import re, statistics as st, subprocess, sys
from collections import Counter, defaultdict
from pgn_games import games

AUTO, OFF = "prover-auto", "prover-off"
INFO = re.compile(r"^([+-]?(?:M\d+|\d+\.\d+))/(\d+) ([\d.]+)s, tl=(-?[\d.]+)s, n=(\d+)")
PROVER = re.compile(r'line="info string prover: ([^"]*)"')
pgn, exe = sys.argv[1], (sys.argv[2] if len(sys.argv) > 2 else None)
LONG = int(sys.argv[3]) if len(sys.argv) > 3 else 10
START = "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1"
POINTS = {"1-0": (1, 0), "0-1": (0, 1), "1/2-1/2": (.5, .5)}

eng = None
if exe:
    eng = subprocess.Popen([exe], stdin=subprocess.PIPE, stdout=subprocess.PIPE, text=True, bufsize=1)
    eng.stdin.write("setoption name Hash value 16\nsetoption name Threads value 1\nsetoption name Prover value off\n"
                    f"setoption name SyzygyPath value {__import__('os').path.expanduser('~/syzygy/5man')}\n")

def off_move(fen, moves, nodes):
    eng.stdin.write(f"ucinewgame\nposition fen {fen} moves {' '.join(moves)}\ngo nodes {max(nodes, 1)}\n"); eng.stdin.flush()
    while True:
        t = eng.stdout.readline().split()
        if t and t[0] == "bestmove": return t[1]

def cls(msgs):   # classe della partita dalle righe del prover (priorità TB > matto lungo > altro)
    if any("tablebase leaves" in m for m in msgs): return "finale con TB"
    if any(int(x) >= LONG for m in msgs for x in re.findall(r"mate in (\d+)", m)): return f"matto lungo (N >= {LONG})"
    return "altro"

per = {e: defaultdict(list) for e in (AUTO, OFF)}
timeloss, term, kinds = Counter(), Counter(), Counter()
pairs = defaultdict(list)   # Round -> [(punti di auto, ha righe del prover, classe, mosse diverse da off)]
for g in games(pgn):
    t = g["tags"]; start = t.get("FEN", START); white = t["White"]
    stm_white = start.split()[1] == "w"; msgs, proved, offn = [], [], []
    for i, (mv, c) in enumerate(zip(g["moves"], g["comments"])):
        side = white if (i % 2 == 0) == stm_white else (OFF if white == AUTO else AUTO)
        m = INFO.match(c)
        if m:
            d = per[side]; d["time"].append(float(m[3])); d["depth"].append(int(m[2])); d["nodes"].append(int(m[5]))
            d["tl"].append(float(m[4]))
            if not PROVER.search(c): d["depth_np"].append(int(m[2]))
            if side == OFF: offn.append(int(m[5]))
        for col in re.findall(r"(White|Black) loses on time", c):   # chi perde, non chi ha mosso
            timeloss[white if col == "White" else (OFF if white == AUTO else AUTO)] += 1
        for p in PROVER.findall(c):
            msgs.append(p); kinds[re.sub(r" (in \d+ )?move.*| nodes.*", "", p)] += 1; proved.append((i, mv))
    # budget di Prover=off: i nodi mediani di prover-off nella stessa partita (le mosse provate hanno pochi nodi: H5)
    nodes = int(st.median(offn)) if offn else 100000
    differs = sum(off_move(start, g["moves"][:i], nodes) != mv for i, mv in proved) if eng else 0
    term[t.get("Termination", "?")] += 1
    pts = POINTS[t["Result"]][0 if white == AUTO else 1]
    pairs[t["Round"]].append((pts, bool(msgs), cls(msgs) if msgs else None, differs))

print("## (A) Statistiche per motore\n")
print("| Motore | mosse | tempo/mossa medio (s) | mediana (s) | p99 (s) | profondità media | prof. media senza mosse provate | nodi medi | tl minimo (s) | perdite per il tempo |")
print("|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|")
for e, d in per.items():
    if not d["time"]: continue
    p99 = sorted(d["time"])[int(.99 * len(d["time"]))]
    print(f"| {e} | {len(d['time'])} | {st.mean(d['time']):.3f} | {st.median(d['time']):.3f} | {p99:.3f} | {st.mean(d['depth']):.2f} | "
          f"{st.mean(d['depth_np']):.2f} | {st.mean(d['nodes']):.0f} | {min(d['tl']):.3f} | {timeloss[e]} |")
print("\nTerminazioni (tag):", dict(term))

full = {r: v for r, v in pairs.items() if len(v) == 2}
games_p = sum(x[1] for v in full.values() for x in v)
pairs_p = [v for v in full.values() if v[0][1] or v[1][1]]
diff = [v for v in full.values() if v[0][0] + v[1][0] != 1]
print(f"\n## (B) Prover\n\nCoppie complete: {len(full)}; partite con almeno una `info string prover` (di prover-auto): {games_p} "
      f"su {2 * len(full)}; coppie con almeno una riga del prover: {len(pairs_p)}")
print(f"Coppie con esito diverso dalla speculare (punti di auto nella coppia != 1): {len(diff)}; di queste con una riga del "
      f"prover: {sum(1 for v in diff if v[0][1] or v[1][1])}")
penta = lambda vs: [sum(1 for v in vs if v[0][0] + v[1][0] == k / 2) for k in range(5)]
print(f"Pentanomiale (0..2 punti di auto) delle coppie con il prover: {penta(pairs_p)}; di tutte: {penta(list(full.values()))}")
print("Righe del prover per tipo:", dict(kinds))

print(f"\n## (C) Classi (partite con righe del prover; matto lungo = mate in N >= {LONG})\n")
print("| Classe | partite | V/P/S di auto | coppie: punti di auto 0/0.5/1/1.5/2 | partite con mossa provata != Prover=off (nodi mediani di off) | V/P/S di quelle |")
print("|---|---:|---|---|---:|---|")
by = defaultdict(list)
for v in full.values():
    for x in v:
        if x[1]: by[x[2]].append((x, v))
for k in sorted(by):
    xs = by[k]; wdl = lambda ys: "/".join(str(sum(1 for y in ys if y[0] == s)) for s in (1, .5, 0))
    pv = penta({id(v): v for _, v in xs}.values()); dif = [x for x, _ in xs if x[3]]
    print(f"| {k} | {len(xs)} | {wdl([x for x, _ in xs])} | {pv} | {len(dif) if eng else 'n/d'} | {wdl(dif) if eng else 'n/d'} |")
if eng: eng.stdin.write("quit\n"); eng.stdin.flush()
