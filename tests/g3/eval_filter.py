#!/usr/bin/env python3
"""G3: tiene le FEN (stdin, una per riga) la cui valutazione a `go depth D` ha |cp| <= MAXCP (scarta i matti).
Uso: eval_filter.py <stockfish> <depth> <maxcp> [minpieces=0] < in.epd > out.epd"""
import subprocess, sys

exe, depth, maxcp = sys.argv[1], int(sys.argv[2]), int(sys.argv[3])
minpieces = int(sys.argv[4]) if len(sys.argv) > 4 else 0
p = subprocess.Popen([exe], stdin=subprocess.PIPE, stdout=subprocess.PIPE, text=True, bufsize=1)
p.stdin.write("setoption name Hash value 16\nsetoption name Threads value 1\n")

def score(fen):
    p.stdin.write(f"ucinewgame\nposition fen {fen}\ngo depth {depth}\n"); p.stdin.flush(); last = None
    while True:
        t = p.stdout.readline().split()
        if t and t[0] == "bestmove": return None if t[1] == "(none)" else last
        if "score" in t:
            i = t.index("score"); last = None if t[i + 1] == "mate" else int(t[i + 2])

for line in sys.stdin:
    fen = " ".join(line.split()[:6])   # anche EPD con 4 campi + opcodes: fastchess accetta la FEN completa
    if not fen or sum(c.isalpha() for c in fen.split()[0]) < minpieces: continue
    s = score(fen)
    if s is not None and abs(s) <= maxcp: print(fen, flush=True)
p.stdin.write("quit\n"); p.stdin.flush()
