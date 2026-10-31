#!/usr/bin/env python3
"""G3 LTC, set scelto per esito (README-ltc.md §6): candidati da mates2000. Matti lunghi (N >= 10, lato al tratto che
matta, >= 6 pezzi) che `Prover=off` con `go movetime MS` (Hash 16, Syzygy 5-man) non vede: tiene la posizione se il
punteggio finale non è un matto a favore del lato al tratto. 4 processi in parallelo.
Uso: mate_candidates.py <stockfish> <mates2000.epd> <out.csv> [ms=1500]"""
import csv, os, re, subprocess, sys
from multiprocessing import Pool

exe, mates, out_csv = sys.argv[1:4]
MS = int(sys.argv[4]) if len(sys.argv) > 4 else 1500

def work(job):
    global E
    if "E" not in globals():
        E = subprocess.Popen([exe], stdin=subprocess.PIPE, stdout=subprocess.PIPE, text=True, bufsize=1)
        E.stdin.write("setoption name Hash value 16\nsetoption name Threads value 1\nsetoption name Prover value off\n"
                      f"setoption name SyzygyPath value {os.path.expanduser('~/syzygy/5man')}\n")
    idx, fen, n = job
    E.stdin.write(f"ucinewgame\nposition fen {fen}\ngo movetime {MS}\n"); E.stdin.flush()
    last = ""
    while not (l := E.stdout.readline()).startswith("bestmove"):
        if " score " in l: last = l
    t = last.split(); i = t.index("score")
    sc = f"{t[i + 1]} {t[i + 2]}"
    return None if t[i + 1] == "mate" and int(t[i + 2]) > 0 else dict(fen=fen, mate=n, idx=idx, score=sc,
                pieces=sum(c.isalpha() for c in fen.split()[0]), pawns=sum(c in "pP" for c in fen.split()[0]))

if __name__ == "__main__":
    jobs = [(i, m[1] + " 0 1", int(m[2])) for i, l in enumerate(open(mates))
            if (m := re.match(r"(.*?) bm #(-?\d+);", l.strip())) and int(m[2]) >= 10 and sum(c.isalpha() for c in m[1].split()[0]) >= 6]
    with Pool(4) as pool: rows = [r for r in pool.map(work, jobs, chunksize=1) if r]
    with open(out_csv, "w", newline="") as f:
        w = csv.DictWriter(f, fieldnames=list(rows[0])); w.writeheader(); w.writerows(rows)
    print(len(jobs), "matti lunghi,", len(rows), "non visti in", MS, "ms")
