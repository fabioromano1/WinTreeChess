#!/usr/bin/env python3
"""G3: per ogni partita di un PGN fastchess (notation=uci) stampa la FEN SKIP semimosse dopo la prima posizione con
<= MAXP pezzi (re inclusi; SKIP lascia chiudere gli scambi), se ha ancora >= MINP pezzi. FEN calcolate dal motore (`d`).
Uso: extract_endgames.py <stockfish> <pgn> [maxp=10] [minp=6] [skip=4] > out.epd"""
import subprocess, sys
from pgn_games import games

exe, pgn = sys.argv[1], sys.argv[2]
arg = lambda i, d: int(sys.argv[i]) if len(sys.argv) > i else d
maxp, minp, skip = arg(3, 10), arg(4, 6), arg(5, 4)
p = subprocess.Popen([exe], stdin=subprocess.PIPE, stdout=subprocess.PIPE, text=True, bufsize=1)
START = "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1"

def fen(start, moves):
    p.stdin.write(f"position fen {start} moves {' '.join(moves)}\nd\n"); p.stdin.flush()
    while True:
        l = p.stdout.readline()
        if l.startswith("Fen:"): f = l[5:].strip()
        if l.startswith("Checkers"): return f

pieces = lambda f: sum(c.isalpha() for c in f.split()[0])
for g in games(pgn):
    start, mv = g["tags"].get("FEN", START), g["moves"]
    if pieces(fen(start, mv)) > maxp: continue
    lo, hi = 0, len(mv)   # prima semimossa k con pieces(dopo mv[:k]) <= maxp (i pezzi non aumentano mai)
    while lo < hi:
        mid = (lo + hi) // 2
        if pieces(fen(start, mv[:mid])) <= maxp: hi = mid
        else: lo = mid + 1
    k = lo + skip
    if k < len(mv):   # la partita deve proseguire oltre: niente posizioni finali (matto/stallo)
        f = fen(start, mv[:k])
        if pieces(f) >= minp: print(f, flush=True)
p.stdin.write("quit\n"); p.stdin.flush()
