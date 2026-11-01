#!/usr/bin/env python3
"""G3 LTC §7 (README-ltc.md): libro mirato per il trigger anticipato del prover (ProverAdvantage). Dal PGN del campione
outcome/ (§6.2) e dal CSV del set, le posizioni in cui il lato in vantaggio (lato al tratto della FEN) ha raggiunto almeno
<soglia> pedoni (valutazione della sua mossa, non decisiva) in almeno una partita: sono le sole in cui un trigger a quella
soglia puo' partire. Uso: adv_book.py <games.pgn> <libro.csv> <soglia> <out.epd>"""
import csv, re, sys
from pgn_games import games

INFO = re.compile(r"^([+-]?)(M?)(\d+(?:\.\d+)?)/")
pgn, book, thr, out = sys.argv[1], sys.argv[2], float(sys.argv[3]), sys.argv[4]
key = lambda fen: " ".join(fen.split()[:2])
hit = set()
for g in games(pgn):
    fen = g["tags"]["FEN"]
    for i, c in enumerate(g["comments"]):
        m = INFO.match(c)   # il lato al tratto della FEN muove alle semimosse pari; punteggio dal suo punto di vista
        if i % 2 == 0 and m and m[1] == "+" and not m[2] and thr <= float(m[3]) < 100:
            hit.add(key(fen)); break
rows = [r for r in csv.DictReader(open(book)) if key(r["fen"]) in hit]
with open(out, "w") as f:
    f.writelines(r["fen"] + "\n" for r in rows)
print(len(rows), "posizioni", {c: sum(r["cls"] == c for r in rows) for c in "FPM"})
