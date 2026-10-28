#!/usr/bin/env python3
# Replay di controllo (README-ltc §2, §2.1): tutte le posizioni delle prime N partite di un PGN fastchess (notation=uci), una
# ricerca `go nodes` per ply, TT conservata fra le mosse (un processo per partita, niente ucinewgame). Stampa per ricerca
# partita, ply, profondità, nodi, bestmove. Uso: tests/g3/replay_nodes.py <exe> <pgn> <ngames> <nodes> [setoption ...]
import re, subprocess, sys

exe, pgn, ngames, nodes, opts = sys.argv[1], sys.argv[2], int(sys.argv[3]), int(sys.argv[4]), sys.argv[5:]
games = []
for block in open(pgn).read().split('[Event ')[1:ngames + 1]:
    fen = re.search(r'\[FEN "([^"]+)"\]', block).group(1)
    body = re.sub(r'\{[^}]*\}', '', block.split('\n\n', 1)[1])
    games.append((fen, re.findall(r'\b[a-h][1-8][a-h][1-8][qrbn]?\b', body)))

for gi, (fen, moves) in enumerate(games):
    p = subprocess.Popen([exe], stdin=subprocess.PIPE, stdout=subprocess.PIPE, text=True, bufsize=1)
    send = lambda s: p.stdin.write(s + '\n')
    for o in ['Hash value 16', 'Threads value 1', 'SyzygyPath value /home/dev/syzygy/5man'] + opts:
        send('setoption name ' + o)
    for ply in range(len(moves)):
        send(f'position fen {fen} moves {" ".join(moves[:ply])}')
        send(f'go nodes {nodes}')
        depth = n = None
        for line in p.stdout:
            if line.startswith('info depth') and ' nodes ' in line:
                depth, n = re.search(r'depth (\d+)', line).group(1), re.search(r' nodes (\d+)', line).group(1)
            if line.startswith('bestmove'):
                print(gi, ply, depth, n, line.split()[1], flush=True)
                break
    send('quit')
    p.wait()
