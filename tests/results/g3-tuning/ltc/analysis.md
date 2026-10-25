## (A) Statistiche per motore

| Motore | mosse | tempo/mossa medio (s) | mediana (s) | p99 (s) | profondità media | prof. media senza mosse provate | nodi medi | tl minimo (s) | perdite per il tempo |
|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| prover-auto | 82236 | 1.581 | 1.045 | 11.270 | 24.57 | 24.56 | 544731 | -18.499 | 1 |
| prover-off | 82239 | 1.580 | 1.042 | 11.184 | 24.69 | 24.69 | 551448 | -14.369 | 4 |

Terminazioni (tag): {'normal': 451, 'adjudication': 1044, 'time forfeit': 5}

## (B) Prover

Coppie complete: 750; partite con almeno una `info string prover` (di prover-auto): 10 su 1500; coppie con almeno una riga del prover: 10
Coppie con esito diverso dalla speculare (punti di auto nella coppia != 1): 146; di queste con una riga del prover: 3
Pentanomiale (0..2 punti di auto) delle coppie con il prover: [0, 2, 7, 1, 0]; di tutte: [0, 86, 604, 59, 1]
Righe del prover per tipo: {'no result after 42775 do_move in 12 ms (budget)': 1, 'no result after 8768 do_move in 11 ms (budget)': 1, 'no result after 14017 do_move in 8 ms (not certified with a clock)': 1, 'no result after 40207 do_move in 19 ms (budget)': 1, 'no result after 76812 do_move in 21 ms (budget)': 1, 'no result after 42674 do_move in 14 ms (budget)': 1, 'no result after 27281 do_move in 17 ms (budget)': 1, 'no result after 5791 do_move in 21 ms (budget)': 1, 'no result after 42959 do_move in 22 ms (budget)': 1, 'proven win (tablebase leaves)': 2, 'no result after 6749 do_move in 11 ms (budget)': 1, 'no result after 7108 do_move in 18 ms (budget)': 1, 'no result after 18521 do_move in 12 ms (not certified with a clock)': 1, 'no result after 10083 do_move in 12 ms (budget)': 1, 'no result after 176134 do_move in 55 ms (budget)': 1}

## (C) Classi (partite con righe del prover; matto lungo = mate in N >= 10)

| Classe | partite | V/P/S di auto | coppie: punti di auto 0/0.5/1/1.5/2 | partite con mossa provata != Prover=off (nodi mediani di off) | V/P/S di quelle |
|---|---:|---|---|---:|---|
| altro | 9 | 3/0/6 | [0, 2, 6, 1, 0] | n/d | n/d |
| finale con TB | 1 | 1/0/0 | [0, 0, 1, 0, 0] | n/d | n/d |
