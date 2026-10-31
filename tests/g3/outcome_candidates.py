#!/usr/bin/env python3
"""G3 LTC, set scelto per esito (README-ltc.md §6): candidati dalle partite già giocate. Per ogni partita e per ogni lato
che non l'ha vinta, la prima posizione con quel lato al tratto a >= +3.00 o a matto nel commento della sua mossa, con
>= 6 pezzi (a 5 l'adjudication a TB chiuderebbe subito). FEN calcolate dal motore (`d`), duplicate tolte (4 campi).
Uso: outcome_candidates.py <stockfish> <out.csv> <pgn>... (stampa il conteggio per PGN)"""
import csv, re, subprocess, sys
from multiprocessing import Pool
from pgn_games import games

exe, out_csv, pgns = sys.argv[1], sys.argv[2], sys.argv[3:]
START = "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1"
EVAL = re.compile(r"^\+(M\d+|\d+\.\d+)/")

def decisive(c):   # commento della mossa: il lato che muove valuta >= +3.00 o matto a favore
    m = EVAL.match(c)
    return bool(m) and (m[1][0] == "M" or float(m[1]) >= 3.0)

def work(arg):
    global E
    if "E" not in globals(): E = subprocess.Popen([exe], stdin=subprocess.PIPE, stdout=subprocess.PIPE, text=True, bufsize=1)
    pgn, g = arg
    r = g["tags"]["Result"]
    losers = {"1-0": "b", "0-1": "w"}.get(r, "wb")   # lati che non hanno vinto
    fen, rows, done = g["tags"].get("FEN", START), [], set()
    side = lambda k: "wb"[(k + (fen.split()[1] == "b")) % 2]
    if not any(decisive(c) and side(k) in losers for k, c in enumerate(g["comments"])): return []   # niente motore
    for k, (mv, c) in enumerate(zip(g["moves"], g["comments"])):
        stm = fen.split()[1]
        if stm in losers and stm not in done and decisive(c) and sum(ch.isalpha() for ch in fen.split()[0]) >= 6:
            done.add(stm)
            rows.append(dict(fen=fen, pieces=sum(ch.isalpha() for ch in fen.split()[0]),
                             pawns=sum(ch in "pP" for ch in fen.split()[0]), eval=c.split("/")[0],
                             side_result="D" if r == "1/2-1/2" else "L", source=pgn, round=g["tags"].get("Round", ""),
                             white=g["tags"]["White"], black=g["tags"]["Black"], ply=k))
        if len(done) == len(losers): break
        E.stdin.write(f"position fen {fen} moves {mv}\nd\n"); E.stdin.flush()
        while not (l := E.stdout.readline()).startswith("Checkers"):
            if l.startswith("Fen:"): fen = l[5:].strip()
    return rows

if __name__ == "__main__":
    assert decisive("+3.00/20 1s") and decisive("+M12/30 1s") and not decisive("+2.99/20") and not decisive("-5.00/20")
    jobs = [(p, g) for p in pgns for g in games(p) if g["tags"]["Result"] != "*"]
    with Pool(4) as pool: rows = [r for rs in pool.imap(work, jobs, chunksize=8) for r in rs]
    seen, uniq = set(), []
    for r in rows:
        key = " ".join(r["fen"].split()[:4])
        if key not in seen: seen.add(key); uniq.append(r)
    with open(out_csv, "w", newline="") as f:
        w = csv.DictWriter(f, fieldnames=list(uniq[0])); w.writeheader(); w.writerows(uniq)
    for p in pgns: print(p, sum(r["source"] == p for r in uniq))
    print(len(uniq), "candidati")
