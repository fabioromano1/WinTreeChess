## (A) Statistiche per motore

| Motore | mosse | tempo/mossa medio (s) | mediana (s) | p99 (s) | profondità media | prof. media senza mosse provate | nodi medi | tl minimo (s) | perdite per il tempo |
|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| prover-auto | 15839 | 0.270 | 0.198 | 1.611 | 36.41 | 36.60 | 194731 | 0.132 | 0 |
| prover-off | 15839 | 0.270 | 0.197 | 1.746 | 35.85 | 35.85 | 187064 | 0.131 | 0 |

Terminazioni (tag): {'normal': 408}

## (B) Prover

Coppie complete: 204; partite con almeno una `info string prover` (di prover-auto): 24 su 408; coppie con almeno una riga del prover: 24
Coppie con esito diverso dalla speculare (punti di auto nella coppia != 1): 4; di queste con una riga del prover: 0
Pentanomiale (0..2 punti di auto) delle coppie con il prover: [0, 0, 24, 0, 0]; di tutte: [0, 4, 200, 0, 0]
Righe del prover per tipo: {'proven mate': 13, 'proven win (tablebase leaves)': 70}

## (C) Classi (partite con righe del prover; matto lungo = mate in N >= 10)

| Classe | partite | V/P/S di auto | coppie: punti di auto 0/0.5/1/1.5/2 | partite con mossa provata != Prover=off (nodi mediani di off) | V/P/S di quelle |
|---|---:|---|---|---:|---|
| altro | 3 | 3/0/0 | [0, 0, 3, 0, 0] | 2 | 2/0/0 |
| finale con TB | 20 | 20/0/0 | [0, 0, 20, 0, 0] | 12 | 12/0/0 |
| matto lungo (N >= 10) | 1 | 1/0/0 | [0, 0, 1, 0, 0] | 1 | 1/0/0 |
