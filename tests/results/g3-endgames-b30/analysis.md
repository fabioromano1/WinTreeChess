## (A) Statistiche per motore

| Motore | mosse | tempo/mossa medio (s) | mediana (s) | p99 (s) | profondità media | prof. media senza mosse provate | nodi medi | tl minimo (s) | perdite per il tempo |
|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| prover-auto | 22588 | 0.273 | 0.199 | 1.744 | 37.06 | 37.12 | 195277 | 0.129 | 0 |
| prover-off | 22578 | 0.273 | 0.200 | 1.751 | 36.74 | 36.74 | 194267 | 0.133 | 0 |

Terminazioni (tag): {'normal': 598}

## (B) Prover

Coppie complete: 299; partite con almeno una `info string prover` (di prover-auto): 11 su 598; coppie con almeno una riga del prover: 11
Coppie con esito diverso dalla speculare (punti di auto nella coppia != 1): 5; di queste con una riga del prover: 1
Pentanomiale (0..2 punti di auto) delle coppie con il prover: [0, 0, 10, 1, 0]; di tutte: [0, 2, 294, 3, 0]
Righe del prover per tipo: {'proven mate': 3, 'proven win (tablebase leaves)': 31}

## (C) Classi (partite con righe del prover; matto lungo = mate in N >= 10)

| Classe | partite | V/P/S di auto | coppie: punti di auto 0/0.5/1/1.5/2 | partite con mossa provata != Prover=off (nodi mediani di off) | V/P/S di quelle |
|---|---:|---|---|---:|---|
| altro | 1 | 1/0/0 | [0, 0, 0, 1, 0] | 0 | 0/0/0 |
| finale con TB | 10 | 10/0/0 | [0, 0, 10, 0, 0] | 6 | 6/0/0 |
