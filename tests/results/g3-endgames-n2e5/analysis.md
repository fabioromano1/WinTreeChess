## (A) Statistiche per motore

| Motore | mosse | tempo/mossa medio (s) | mediana (s) | p99 (s) | profondità media | prof. media senza mosse provate | nodi medi | tl minimo (s) | perdite per il tempo |
|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| prover-auto | 15820 | 0.273 | 0.200 | 1.700 | 38.00 | 38.24 | 211939 | 0.131 | 0 |
| prover-off | 15814 | 0.271 | 0.198 | 1.641 | 37.55 | 37.55 | 200703 | 0.131 | 0 |

Terminazioni (tag): {'normal': 416}

## (B) Prover

Coppie complete: 208; partite con almeno una `info string prover` (di prover-auto): 25 su 416; coppie con almeno una riga del prover: 25
Coppie con esito diverso dalla speculare (punti di auto nella coppia != 1): 3; di queste con una riga del prover: 0
Pentanomiale (0..2 punti di auto) delle coppie con il prover: [0, 0, 25, 0, 0]; di tutte: [0, 2, 205, 1, 0]
Righe del prover per tipo: {'proven win (tablebase leaves)': 96, 'proven mate': 9}

## (C) Classi (partite con righe del prover; matto lungo = mate in N >= 10)

| Classe | partite | V/P/S di auto | coppie: punti di auto 0/0.5/1/1.5/2 | partite con mossa provata != Prover=off (nodi mediani di off) | V/P/S di quelle |
|---|---:|---|---|---:|---|
| altro | 2 | 2/0/0 | [0, 0, 2, 0, 0] | 1 | 1/0/0 |
| finale con TB | 23 | 23/0/0 | [0, 0, 23, 0, 0] | 14 | 14/0/0 |
