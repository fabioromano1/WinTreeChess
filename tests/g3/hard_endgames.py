#!/usr/bin/env python3
"""G3 LTC (tests/g3/README-ltc.md): set di finali sensibile da un PGN fastchess (notation=uci, eval nei commenti).
Classi: A = 6-7 pezzi (re inclusi, fuori dalle 5-man); B = 8-10 pezzi con <= 3 pedoni. Per partita: posizioni con il
lato al tratto a +1..+8 nel commento della mossa (prefiltro a 2+0.02), almeno 8 semimosse fra loro, al massimo 4 per
classe; poi `go depth D` (Prover=off, Syzygy 5-man): tiene 150 <= cp <= 600 (lato al tratto), al massimo 3 per partita.
WDL a 6 pezzi dalle 6-man (`go depth 1` con la radice in TB, tbhits > 0: cp >= 10000 W, <= -10000 L, altrimenti D). 4 processi in parallelo.
Uso: hard_endgames.py <stockfish> <pgn> <out.epd> <out.csv> [depth=20] [tb6=~/syzygy/5man:~/syzygy/6man] [maxgames=0]"""
import csv, os, re, subprocess, sys
from multiprocessing import Pool
from pgn_games import games

exe, pgn, out_epd, out_csv = sys.argv[1:5]
DEPTH = int(sys.argv[5]) if len(sys.argv) > 5 else 20
TB6 = ":".join(map(os.path.expanduser, (sys.argv[6] if len(sys.argv) > 6 else "~/syzygy/5man:~/syzygy/6man").split(":")))
MAXG = int(sys.argv[7]) if len(sys.argv) > 7 else 0
START = "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1"
EVAL = re.compile(r"^([+-]?\d+\.\d+)/")

def engine(tb):
    p = subprocess.Popen([exe], stdin=subprocess.PIPE, stdout=subprocess.PIPE, text=True, bufsize=1)
    p.stdin.write(f"setoption name Hash value 16\nsetoption name Threads value 1\nsetoption name Prover value off\n"
                  f"setoption name SyzygyPath value {tb}\n")
    return p

def cmd(p, s, until):
    p.stdin.write(s + "\n"); p.stdin.flush()
    out = []
    while not (l := p.stdout.readline()).startswith(until): out.append(l)
    return out, l

def score(p, fen, depth):   # cp dal lato al tratto, None se matto o nessuna mossa
    lines, _ = cmd(p, f"ucinewgame\nposition fen {fen}\ngo depth {depth}", "bestmove")
    t = next((l.split() for l in reversed(lines) if " score " in l), None)
    if not t: return None
    i = t.index("score")
    return None if t[i + 1] == "mate" else int(t[i + 2])

def wdl(p, fen):   # esito per il lato al tratto dalle tablebase della radice, "" se la radice non è in TB
    lines, _ = cmd(p, f"ucinewgame\nposition fen {fen}\ngo depth 1", "bestmove")
    t = next((l.split() for l in reversed(lines) if " score " in l), None)
    if not t or "tbhits" not in t or int(t[t.index("tbhits") + 1]) == 0: return ""
    v = int(t[t.index("score") + 2]) if t[t.index("score") + 1] == "cp" else 0
    return "W" if v >= 10000 else "L" if v <= -10000 else "D"

def cls(fen):
    b = fen.split()[0]; n = sum(c.isalpha() for c in b); pawns = sum(c in "pP" for c in b)
    return "A" if n in (6, 7) else "B" if 8 <= n <= 10 and pawns <= 3 else None

def candidates(g, p):   # FEN prima di ogni mossa (incrementale con `d`), filtrate per classe e commento
    fen, res, last = g["tags"].get("FEN", START), [], -99
    for k, (mv, c) in enumerate(zip(g["moves"], g["comments"])):
        m, cl = EVAL.match(c), cls(fen)
        if cl and m and 1 <= float(m.group(1)) <= 8 and k - last >= 8 and \
           sum(r[1] == cl for r in res) < 4:
            res.append((fen, cl, k)); last = k
        lines, _ = cmd(p, f"position fen {fen} moves {mv}\nd", "Checkers")
        fen = next(l[5:].strip() for l in lines if l.startswith("Fen:"))
    return res

def work(g):
    global E, T
    if "E" not in globals(): E, T = engine(os.path.expanduser("~/syzygy/5man")), engine(TB6)
    rows = []
    for fen, cl, k in candidates(g, E):
        if len(rows) == 3: break
        s = score(E, fen, DEPTH)
        if s is None or not 150 <= s <= 600: continue
        n = sum(c.isalpha() for c in fen.split()[0])
        stm = fen.split()[1]; r = g["tags"]["Result"]
        res = "D" if r == "1/2-1/2" else "W" if (r == "1-0") == (stm == "w") else "L"
        pawns = sum(c in "pP" for c in fen.split()[0])
        rows.append(dict(fen=fen, cls=cl, pieces=n, pawns=pawns, cp=s, wdl6=wdl(T, fen) if n <= 6 else "", game_result=res,
                         round=g["tags"].get("Round", ""), ply=k))
    return rows

if __name__ == "__main__":
    gs = list(games(pgn)); gs = gs[:MAXG] if MAXG else gs
    with Pool(4) as pool: rows = [r for rs in pool.imap(work, gs, chunksize=4) for r in rs]
    seen, uniq = set(), []
    for r in rows:
        key = " ".join(r["fen"].split()[:4])
        if key not in seen: seen.add(key); uniq.append(r)
    with open(out_epd, "w") as f: f.writelines(r["fen"] + "\n" for r in uniq)
    with open(out_csv, "w", newline="") as f:
        w = csv.DictWriter(f, fieldnames=list(uniq[0])); w.writeheader(); w.writerows(uniq)
    assert all(150 <= r["cp"] <= 600 and r["cls"] for r in uniq)
    print(len(gs), "partite,", len(uniq), "posizioni")
