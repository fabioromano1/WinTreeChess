## (A) Statistiche per motore

| Motore | mosse | tempo/mossa medio (s) | mediana (s) | p99 (s) | profondità media | prof. media senza mosse provate | nodi medi | tl minimo (s) | perdite per il tempo |
|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| prover-auto | 740761 | 0.267 | 0.157 | 1.753 | 17.35 | 17.35 | 91486 | -0.160 | 24 |
| prover-off | 740759 | 0.267 | 0.156 | 1.758 | 17.27 | 17.27 | 91719 | -0.256 | 22 |

Terminazioni (tag): {'normal': 4007, 'adjudication': 9522, 'time forfeit': 46}

## (B) Prover

Coppie complete: 6787; partite con almeno una `info string prover` (di prover-auto): 10 su 13574; coppie con almeno una riga del prover: 10
Coppie con esito diverso dalla speculare (punti di auto nella coppia != 1): 2135; di queste con una riga del prover: 4
Pentanomiale (0..2 punti di auto) delle coppie con il prover: [0, 0, 6, 4, 0]; di tutte: [42, 1062, 4652, 1000, 31]
Righe del prover per tipo: {'proven mate': 12, 'proven win (tablebase leaves)': 4}

## (C) Classi (partite con righe del prover; matto lungo = mate in N >= 10)

| Classe | partite | V/P/S di auto | coppie: punti di auto 0/0.5/1/1.5/2 | partite con mossa provata != Prover=off (nodi mediani di off) | V/P/S di quelle |
|---|---:|---|---|---:|---|
| altro | 7 | 7/0/0 | [0, 0, 4, 3, 0] | 2 | 2/0/0 |
| finale con TB | 3 | 3/0/0 | [0, 0, 2, 1, 0] | 1 | 1/0/0 |
