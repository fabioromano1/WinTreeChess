# Riferimento: campione di §6 (trigger decisivo) ristretto alle 23 posizioni di endgames-outcome-adv100.epd (170 partite, 85 coppie)

Uscita di outcome_analysis.py e prover_log.py sul PGN di ../outcome/ filtrato per FEN (scratchpad della sessione, outcome-sub23.pgn).

| Sottoinsieme | posizioni | coppie | V/P/S del lato in vantaggio giocato da auto | giocato da off | punti auto - off per coppia [95 %] | vittorie auto - off [95 %] |
|---|---:|---:|---|---|---:|---:|
| tutte | 23 | 85 | 28/57/0 | 29/56/0 | -0.006 [-0.066; +0.057] | -1.2% [-13.3%; +11.4%] |
| classe F | 2 | 8 | 4/4/0 | 4/4/0 | +0.000 [+0.000; +0.000] | +0.0% [+0.0%; +0.0%] |
| classe P | 20 | 73 | 24/49/0 | 25/48/0 | -0.007 [-0.077; +0.061] | -1.4% [-15.5%; +12.2%] |
| classe M | 1 | 4 | 0/4/0 | 0/4/0 | +0.000 [+0.000; +0.000] | +0.0% [+0.0%; +0.0%] |
| patta in entrambe le partite del filtro | 17 | 63 | 15/48/0 | 13/50/0 | +0.016 [-0.047; +0.078] | +3.2% [-9.4%; +15.6%] |
| esiti diversi nel filtro | 6 | 22 | 13/9/0 | 16/6/0 | -0.068 [-0.190; +0.087] | -13.6% [-38.1%; +17.4%] |

Coppie non pari: 25; a favore di auto 12, a favore di off 13.
A favore di auto con prova certificata di auto col lato in vantaggio: 12; a favore di off con prova di auto col lato in vantaggio: 0.
Coppie pari, lato in vantaggio vince con auto e con off: 16; auto prova la vittoria in 16 di queste.

Partite in cui il motore raggiunge un punteggio decisivo a suo favore, di cui vinte: auto 28 su 28, off 29 su 29.

| Round | classe | filtro | lato in vantaggio: auto | off | mosse auto / off | prima prova di auto col lato in vantaggio: semimossa, eval della mossa precedente, esito |
|---:|---|---|---:|---:|---|---|
| 8 | P 12p +3.49 | 0.5/0.5 | 0.5 | 1 | 173 / 57 | nessuna |
| 56 | P 20p +3.14 | 1.0/0.5 | 1 | 0.5 | 161 / 193 | 160, +199.97, proven win (tablebase leaves) |
| 60 | P 22p +3.72 | 0.5/1.0 | 1 | 0.5 | 144 / 132 | 138, +199.90, proven win (tablebase leaves) |
| 62 | P 28p +3.26 | 0.5/0.5 | 0.5 | 1 | 246 / 97 | nessuna |
| 72 | P 24p +3.02 | 1.0/0.5 | 1 | 0.5 | 93 / 170 | 88, +199.93, proven win (tablebase leaves) |
| 103 | P 25p +4.57 | 1.0/0.5 | 0.5 | 1 | 85 / 122 | nessuna |
| 109 | P 13p +3.28 | 0.5/0.5 | 0.5 | 1 | 64 / 68 | nessuna |
| 124 | P 24p +3.30 | 0.5/0.5 | 0.5 | 1 | 103 / 89 | nessuna |
| 136 | P 24p +3.32 | 0.5/0.5 | 0.5 | 1 | 232 / 171 | nessuna |
| 140 | P 16p +3.35 | 0.5/0.5 | 1 | 0.5 | 98 / 166 | 92, +199.93, proven win (tablebase leaves) |
| 162 | P 21p +3.75 | 0.5/0.5 | 1 | 0.5 | 116 / 195 | 102, +199.93, proven win (tablebase leaves) |
| 189 | P 20p +3.14 | 1.0/0.5 | 1 | 0.5 | 105 / 99 | 100, +199.91, proven win (tablebase leaves) |
| 193 | P 22p +3.72 | 0.5/1.0 | 0.5 | 1 | 77 / 132 | nessuna |
| 195 | P 28p +3.26 | 0.5/0.5 | 1 | 0.5 | 244 / 114 | 236, +199.91, proven win (tablebase leaves) |
| 205 | P 24p +3.02 | 1.0/0.5 | 0.5 | 1 | 120 / 169 | nessuna |
| 242 | P 13p +3.28 | 0.5/0.5 | 1 | 0.5 | 81 / 97 | 64, +M17, proven mate in 9 |
| 247 | P 25p +4.77 | 0.5/1.0 | 0.5 | 1 | 80 / 121 | nessuna |
| 282 | P 26p +3.54 | 0.5/0.5 | 1 | 0.5 | 195 / 68 | 190, +199.93, proven win (tablebase leaves) |
| 295 | P 21p +3.75 | 0.5/0.5 | 1 | 0.5 | 170 / 104 | 162, +199.91, proven win (tablebase leaves) |
| 326 | P 22p +3.72 | 0.5/1.0 | 0.5 | 1 | 146 / 103 | nessuna |
| 338 | P 24p +3.02 | 1.0/0.5 | 0.5 | 1 | 199 / 117 | nessuna |
| 390 | P 24p +3.30 | 0.5/0.5 | 0.5 | 1 | 83 / 119 | nessuna |
| 406 | P 16p +3.35 | 0.5/0.5 | 1 | 0.5 | 129 / 81 | 126, +199.94, proven win (tablebase leaves) |
| 459 | P 22p +3.72 | 0.5/1.0 | 0.5 | 1 | 149 / 138 | nessuna |
| 461 | P 28p +3.26 | 0.5/0.5 | 1 | 0.5 | 127 / 163 | 124, +199.95, proven win (tablebase leaves) |

Partite: 170; avvii del prover: 296 in 56 partite; esiti certificati: 108 in 28 partite
Senza esito: 188 ({'budget': 103, 'not certified with a clock': 85}); tempo speso senza esito: totale 2.8 s, mediana 10 ms, max 87 ms; per partita con avvio 50 ms
do_move senza esito: mediana 2996, max 134819
Modalita' degli avvii: Counter({'?': 296}); avvii nella mossa di una perdita per il tempo: 0
