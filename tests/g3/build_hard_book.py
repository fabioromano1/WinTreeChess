#!/usr/bin/env python3
"""G3 LTC: libro `endgames-hard.epd` = finali di hard_endgames.py (CSV) + NM matti lunghi (N >= 10, lato al tratto
che matta, >= 6 pezzi: a 5 l'adjudication a TB chiuderebbe la partita subito) da mates2000, presi a passo costante
nell'ordine di N; mescolati con seme fisso. Scrive anche il CSV con la classe (A, B, M) e stampa la distribuzione.
Uso: build_hard_book.py <finali.csv> <mates2000.epd> <out.epd> <out.csv> [NM=100]"""
import csv, random, re, sys
from collections import Counter

src, mates, out_epd, out_csv = sys.argv[1:5]
NM = int(sys.argv[5]) if len(sys.argv) > 5 else 100
rows = list(csv.DictReader(open(src)))
long = sorted((int(m[2]), m[1]) for m in (re.match(r"(.*?) bm #(-?\d+);", l.strip()) for l in open(mates))
            if m and int(m[2]) >= 10 and sum(c.isalpha() for c in m[1].split()[0]) >= 6)
step = len(long) / NM
for n, fen4 in (long[int(i * step)] for i in range(NM)):
    b = fen4.split()[0]
    rows.append(dict(fen=fen4 + " 0 1", cls="M", pieces=sum(c.isalpha() for c in b), pawns=sum(c in "pP" for c in b),
                     cp=f"#{n}", wdl6="", game_result="", round="", ply=""))
random.Random(0).shuffle(rows)
assert len({" ".join(r["fen"].split()[:4]) for r in rows}) == len(rows)
open(out_epd, "w").writelines(r["fen"] + "\n" for r in rows)
with open(out_csv, "w", newline="") as f:
    w = csv.DictWriter(f, fieldnames=list(rows[0])); w.writeheader(); w.writerows(rows)

eg = [r for r in rows if r["cls"] != "M"]
print(f"Posizioni: {len(rows)} (A 6-7 pezzi: {sum(r['cls'] == 'A' for r in rows)}, B 8-10 pezzi con <= 3 pedoni: "
      f"{sum(r['cls'] == 'B' for r in rows)}, M matti lunghi: {NM})\n")
print("| Pezzi | posizioni | pedoni 0/1/2/3+ |\n|---:|---:|---|")
for n in sorted({int(r["pieces"]) for r in eg}):
    rs = [r for r in eg if int(r["pieces"]) == n]; pc = Counter(min(int(r["pawns"]), 3) for r in rs)
    print(f"| {n} | {len(rs)} | {'/'.join(str(pc[k]) for k in range(4))} |")
bins = [(150, 250), (250, 350), (350, 450), (450, 601)]
print("\n| Eval d20 (cp, lato al tratto) | A | B |\n|---|---:|---:|")
for lo, hi in bins:
    print(f"| {lo}-{hi - 1} | " + " | ".join(str(sum(r["cls"] == c and lo <= int(r["cp"]) < hi for r in eg)) for c in "AB") + " |")
w6 = Counter(r["wdl6"] for r in eg if int(r["pieces"]) == 6)
print(f"\nWDL dalle 6-man (lato al tratto, posizioni a 6 pezzi): W {w6['W']}, D {w6['D']}, L {w6['L']}, fuori TB {w6['']}")
gr = Counter((r["cls"], r["game_result"]) for r in eg)
print("Esito della partita d'origine (2+0.02, lato al tratto) V/P/S: " +
      ", ".join(f"{c} {gr[c, 'W']}/{gr[c, 'D']}/{gr[c, 'L']}" for c in "AB"))
mn = Counter(int(r["cp"][1:]) for r in rows if r["cls"] == "M")
print(f"Matti: N da {min(mn)} a {max(mn)}, mediana {sorted(int(r['cp'][1:]) for r in rows if r['cls'] == 'M')[NM // 2]}; "
      f"pezzi da {min(int(r['pieces']) for r in rows if r['cls'] == 'M')} a {max(int(r['pieces']) for r in rows if r['cls'] == 'M')}")
