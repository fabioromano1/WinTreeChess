## (A) Statistiche per motore

| Motore | mosse | tempo/mossa medio (s) | mediana (s) | p99 (s) | profondità media | prof. media senza mosse provate | nodi medi | tl minimo (s) | perdite per il tempo |
|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| prover-auto | 20837 | 1.596 | 1.073 | 10.846 | 25.37 | 25.37 | 590598 | 0.747 | 0 |
| prover-off | 20852 | 1.598 | 1.060 | 11.181 | 25.29 | 25.29 | 589847 | 0.740 | 0 |

nps aggregata prover-auto: 370100
nps aggregata prover-off: 369104
nps per partita, auto / off - 1: +0.51% ± 0.78% (95 %, 400 partite; mediana -0.14%)

Terminazioni (tag): {'normal': 137, 'adjudication': 263}

## (B) Prover

Coppie complete: 200; partite con almeno una `info string prover` (di prover-auto): 0 su 400; coppie con almeno una riga del prover: 0
Coppie con esito diverso dalla speculare (punti di auto nella coppia != 1): 30; di queste con una riga del prover: 0
Pentanomiale (0..2 punti di auto) delle coppie con il prover: [0, 0, 0, 0, 0]; di tutte: [0, 12, 170, 18, 0]
Righe del prover per tipo: {}

## (C) Classi (partite con righe del prover; matto lungo = mate in N >= 10)

| Classe | partite | V/P/S di auto | coppie: punti di auto 0/0.5/1/1.5/2 | partite con mossa provata != Prover=off (nodi mediani di off) | V/P/S di quelle |
|---|---:|---|---|---:|---|
