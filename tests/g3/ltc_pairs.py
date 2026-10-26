#!/usr/bin/env python3
"""G3 LTC: coppie a colori invertiti per classe del libro (CSV di build_hard_book.py: A, B, M) e coppie con esito
diverso dalla speculare, con la riga del libro e le righe del prover (`info string prover`) della coppia.
Uso: ltc_pairs.py <games.pgn> <libro.csv> [out.csv]"""
import csv, re, sys
from collections import defaultdict
from pgn_games import games

AUTO, POINTS = "prover-auto", {"1-0": (1, 0), "0-1": (0, 1), "1/2-1/2": (.5, .5)}
pgn, book = sys.argv[1], sys.argv[2]
key = lambda fen: " ".join(fen.split()[:2])   # scacchiera e tratto: fastchess può riscrivere arrocco ed en passant
cls = {key(r["fen"]): r for r in csv.DictReader(open(book))}
pairs = defaultdict(list)
for g in games(pgn):
    t = g["tags"]; pts = POINTS[t["Result"]][0 if t["White"] == AUTO else 1]
    msgs = [m for c in g["comments"] for m in re.findall(r'line="info string prover: ([^"]*)"', c)]
    pairs[t["Round"]].append((pts, cls[key(t["FEN"])], msgs))
full = [v for v in pairs.values() if len(v) == 2]
penta = lambda vs: [sum(1 for v in vs if v[0][0] + v[1][0] == k / 2) for k in range(5)]
print("| Classe | coppie | pentanomiale [0..2] di auto | punti di auto | coppie con il prover | non pari | non pari con il prover |")
print("|---|---:|---|---:|---:|---:|---:|")
for c in "ABM":
    vs = [v for v in full if v[0][1]["cls"] == c]
    if not vs: continue
    pr = lambda v: v[0][2] or v[1][2]; odd = [v for v in vs if v[0][0] + v[1][0] != 1]
    print(f"| {c} | {len(vs)} | {penta(vs)} | {sum(x[0] for v in vs for x in v):g} / {2 * len(vs)} | "
          f"{sum(1 for v in vs if pr(v))} | {len(odd)} | {sum(1 for v in odd if pr(v))} |")
print(f"| tutte | {len(full)} | {penta(full)} | {sum(x[0] for v in full for x in v):g} / {2 * len(full)} | | | |")
if len(sys.argv) > 3:
    with open(sys.argv[3], "w", newline="") as f:
        w = csv.writer(f); w.writerow(["round", "cls", "pieces", "pawns", "cp", "wdl6", "auto_points", "prover_lines", "fen"])
        for r, v in sorted(pairs.items(), key=lambda x: int(x[0])):
            if len(v) == 2 and v[0][0] + v[1][0] != 1:
                b = v[0][1]
                w.writerow([r, b["cls"], b["pieces"], b["pawns"], b["cp"], b["wdl6"], v[0][0] + v[1][0],
                            " | ".join(v[0][2] + v[1][2]), b["fen"]])
