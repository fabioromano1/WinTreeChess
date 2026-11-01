## (A) Statistiche per motore

| Motore | mosse | tempo/mossa medio (s) | mediana (s) | p99 (s) | profondità media | prof. media senza mosse provate | nodi medi | tl minimo (s) | perdite per il tempo |
|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| prover-auto | 56820 | 1.528 | 0.974 | 11.453 | 30.67 | 31.92 | 541104 | 0.709 | 0 |
| prover-off | 56853 | 1.528 | 0.990 | 11.211 | 31.19 | 31.19 | 540690 | 0.681 | 0 |

nps aggregata prover-auto: 354172
nps aggregata prover-off: 353945
nps per partita, auto / off - 1: +0.37% ± 0.51% (95 %, 1000 partite; mediana -0.26%)

Terminazioni (tag): {'adjudication': 673, 'normal': 327}
nps di ricerca, tratto 'saltato': 38060 mosse di auto usabili; auto / off - 1 = -0.25% ± 0.52% (95 %, 696 partite con >= 5 mosse per motore; mediana -0.40%)
nps di ricerca, tratto 'dopo un avvio': 17627 mosse di auto usabili; auto / off - 1 = +4.37% ± 1.50% (95 %, 389 partite con >= 5 mosse per motore; mediana +2.56%)
nps di ricerca, tratto 'dopo la prova': 0 mosse di auto usabili; meno di 2 partite con >= 5 mosse per motore

## (B) Prover

Coppie complete: 500; partite con almeno una `info string prover` (di prover-auto): 511 su 1000; coppie con almeno una riga del prover: 359
Coppie con esito diverso dalla speculare (punti di auto nella coppia != 1): 115; di queste con una riga del prover: 110
Pentanomiale (0..2 punti di auto) delle coppie con il prover: [0, 62, 249, 48, 0]; di tutte: [0, 67, 385, 48, 0]
Righe del prover per tipo: {'no result (budget)': 7477, 'proven win (tablebase leaves)': 474, 'no result (not certified with a clock)': 490, 'proven mate': 95}

## (C) Classi (partite con righe del prover; matto lungo = mate in N >= 10)

| Classe | partite | V/P/S di auto | coppie: punti di auto 0/0.5/1/1.5/2 | partite con mossa provata != Prover=off (nodi mediani di off) | V/P/S di quelle |
|---|---:|---|---|---:|---|
| altro | 364 | 8/201/155 | [0, 62, 243, 2, 0] | 208 | 8/88/112 |
| finale con TB | 143 | 143/0/0 | [0, 0, 99, 44, 0] | 143 | 143/0/0 |
| matto lungo (N >= 10) | 4 | 4/0/0 | [0, 0, 2, 2, 0] | 4 | 4/0/0 |
