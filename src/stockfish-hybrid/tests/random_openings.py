#!/usr/bin/env python3
"""M3 (e): libro di aperture casuali in EPD per le partite di fumo con fastchess. Usa il motore stesso: `go perft 1`
elenca le mosse legali, `d` stampa la FEN. Uso: random_openings.py <stockfish> <n> <plies> [seme] > aperture.epd"""
import random, subprocess, sys

exe, n, plies = sys.argv[1], int(sys.argv[2]), int(sys.argv[3])
rng = random.Random(int(sys.argv[4]) if len(sys.argv) > 4 else 1)
p = subprocess.Popen([exe], stdin=subprocess.PIPE, stdout=subprocess.PIPE, text=True, bufsize=1)

def ask(cmd, until):
    p.stdin.write(cmd + "\n"); p.stdin.flush(); out = []
    while True:
        line = p.stdout.readline().rstrip("\n"); out.append(line)
        if line.startswith(until): return out

seen = set()
while len(seen) < n:
    moves = []
    for _ in range(plies):
        legal = [l.split(":")[0] for l in ask("position startpos moves " + " ".join(moves) + "\ngo perft 1", "Nodes searched") if ":" in l and len(l.split(":")[0]) in (4, 5)]
        if not legal: break
        moves.append(rng.choice(legal))
    fen = [l for l in ask("position startpos moves " + " ".join(moves) + "\nd", "Checkers") if l.startswith("Fen:")][0][5:]
    if len(moves) == plies and fen not in seen:   # scarta le linee terminate prima e i duplicati
        seen.add(fen); print(fen)
ask("quit", "")
