## (A) Statistiche per motore

| Motore | mosse | tempo/mossa medio (s) | mediana (s) | p99 (s) | profondità media | prof. media senza mosse provate | nodi medi | tl minimo (s) | perdite per il tempo |
|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| prover-auto | 32960 | 1.736 | 1.368 | 11.052 | 45.85 | 45.79 | 733102 | 0.729 | 0 |
| prover-off | 32940 | 1.742 | 1.372 | 10.954 | 46.52 | 46.52 | 736209 | 0.735 | 0 |

nps aggregata prover-auto: 422279
nps aggregata prover-off: 422514
nps per partita, auto / off - 1: -0.06% ± 0.63% (95 %, 1000 partite; mediana +0.08%)

Terminazioni (tag): {'normal': 417, 'adjudication': 583}
nps di ricerca, tratto 'saltato': 32108 mosse di auto usabili; auto / off - 1 = +0.44% ± 0.53% (95 %, 945 partite con >= 5 mosse per motore; mediana +0.20%)
nps di ricerca, tratto 'dopo un avvio': 422 mosse di auto usabili; auto / off - 1 = -0.23% ± 7.47% (95 %, 34 partite con >= 5 mosse per motore; mediana -1.26%)
nps di ricerca, tratto 'dopo la prova': 0 mosse di auto usabili; meno di 2 partite con >= 5 mosse per motore

## (B) Prover

Coppie complete: 500; partite con almeno una `info string prover` (di prover-auto): 111 su 1000; coppie con almeno una riga del prover: 73
Coppie con esito diverso dalla speculare (punti di auto nella coppia != 1): 32; di queste con una riga del prover: 32
Pentanomiale (0..2 punti di auto) delle coppie con il prover: [0, 15, 41, 17, 0]; di tutte: [0, 15, 468, 17, 0]
Righe del prover per tipo: {'no result (budget)': 261, 'no result (not certified with a clock)': 212, 'proven mate': 165, 'proven win (tablebase leaves)': 133}

## (C) Classi (partite con righe del prover; matto lungo = mate in N >= 10)

| Classe | partite | V/P/S di auto | coppie: punti di auto 0/0.5/1/1.5/2 | partite con mossa provata != Prover=off (nodi mediani di off) | V/P/S di quelle |
|---|---:|---|---|---:|---|
| altro | 66 | 11/3/52 | [0, 15, 39, 4, 0] | 42 | 7/0/35 |
| finale con TB | 38 | 38/0/0 | [0, 0, 25, 13, 0] | 32 | 32/0/0 |
| matto lungo (N >= 10) | 7 | 7/0/0 | [0, 0, 7, 0, 0] | 7 | 7/0/0 |
