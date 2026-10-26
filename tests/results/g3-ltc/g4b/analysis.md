## (A) Statistiche per motore

| Motore | mosse | tempo/mossa medio (s) | mediana (s) | p99 (s) | profondità media | prof. media senza mosse provate | nodi medi | tl minimo (s) | perdite per il tempo |
|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| prover-auto | 21641 | 1.591 | 1.038 | 11.227 | 24.10 | 24.10 | 495939 | 0.734 | 0 |
| prover-off | 21642 | 1.588 | 1.054 | 11.087 | 24.36 | 24.36 | 503299 | 0.744 | 0 |

nps aggregata prover-auto: 311813
nps aggregata prover-off: 316985
nps per partita, auto / off - 1: -1.27% ± 0.82% (95 %, 400 partite; mediana -1.15%)

Terminazioni (tag): {'normal': 126, 'adjudication': 274}

## (B) Prover

Coppie complete: 200; partite con almeno una `info string prover` (di prover-auto): 0 su 400; coppie con almeno una riga del prover: 0
Coppie con esito diverso dalla speculare (punti di auto nella coppia != 1): 37; di queste con una riga del prover: 0
Pentanomiale (0..2 punti di auto) delle coppie con il prover: [0, 0, 0, 0, 0]; di tutte: [0, 22, 163, 15, 0]
Righe del prover per tipo: {}

## (C) Classi (partite con righe del prover; matto lungo = mate in N >= 10)

| Classe | partite | V/P/S di auto | coppie: punti di auto 0/0.5/1/1.5/2 | partite con mossa provata != Prover=off (nodi mediani di off) | V/P/S di quelle |
|---|---:|---|---|---:|---|
