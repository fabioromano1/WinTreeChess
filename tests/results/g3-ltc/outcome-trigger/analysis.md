# Trigger anticipato `ProverAdvantage=100` su 23 posizioni: `auto` contro `off`, 60+0.6, 500 coppie (2026-11-01)

Costruzione del libro e lettura: `tests/g3/README-ltc.md` §7. Corsa: `run.sh` 09:10-11:36 (spegnimento della macchina a 201 partite;
fastchess aveva salvato 99 coppie) e `run.sh resume` 11:45-21:31 dalla partita 199 (scartate le 3 partite dei round 100-101), 12 h 12
di gioco in tutto, analisi e compressione fino alle 22:57 (stima prima del via 8-10 h, rivista a ~12 h al controllo delle 100 partite: 169 s per partita su queste posizioni,
come in §6 sulle stesse). Binario sha256 `d4507e96…6346`, libro `tests/books/endgames-outcome-adv100.epd` (23 posizioni di §6: F 2,
P 20, M 1, quelle in cui in §6 il lato in vantaggio ha toccato +1.00 non decisivo), `auto` con `ProverBudget=30`, `ProverLog=true`,
`ProverAdvantage=100`. Dalle 14:18 due processi `ssh-add` bloccati al 92 % di CPU (2 core) fino alla fine, più Firefox: i due motori di
una partita condividono la macchina, le misure di nps sono confronti dentro la partita.

**Un trigger che parte prima del punteggio decisivo dà spazio al prover dove `off` non converte? No.** Punti di `auto` − punti di `off`
col lato in vantaggio: **−0,019 per coppia [−0,044; +0,006]** (bootstrap al 95 % sulle posizioni); vittorie del lato in vantaggio 155
contro 173, differenza −3,6 % [−8,8 %; +1,4 %]. Il prover è partito in 352 delle 500 partite di `auto` col lato in vantaggio (mediana:
alla sua seconda semimossa), 8536 avvii in tutto contro i ~1700 attesi col trigger decisivo (§6 sulle stesse posizioni: 1,7 per partita);
dei 6923 avvii dovuti alla nuova soglia (valutazione della mossa non decisiva) 6916 finiscono senza esito per budget e 7 certificano una
vittoria (valutazione propria precedente in [+5, +10) in 2, in [+10, +100) in 5); le altre 562 prove arrivano con la valutazione
precedente già decisiva, come in §6. Tutte le partite che raggiungono un punteggio decisivo sono vinte: `auto` 155 su 155, `off` 174 su
174 (una col lato in svantaggio: round 135, `auto` col lato in vantaggio matto subito da +1.16 dopo 19 avvii senza esito, 1,9 s).
Al controllo delle 100 partite (50 coppie) la differenza era +0,060 [+0,000; +0,125], 8 coppie contro 2: rumore.

| Misura | Valore |
|---|---|
| Elo (logistic) | −6,60 ± 7,28 (nElo −19,53 ± 21,53) |
| Pentanomiale [0..2] di `auto` | [0, 67, 385, 48, 0] |
| V/S/P di `auto` | 155 / 174 / 671 |
| Timeout, crash | 0, 0 |
| Avvii del prover | 8536 in 511 partite su 1000 (359 coppie su 500); 352 delle 500 partite di `auto` col lato in vantaggio |
| Avvii dovuti alla nuova soglia, per fascia della valutazione della mossa | 6923: [+1, +2) 2757, [+2, +3) 967, [+3, +5) 1251, [+5, +10) 1337, [+10, +100) 159, < +1 (punteggio precedente ≥ +1) 445; senza esito 6916, prove 7 |
| Avvii del trigger di §6 (valutazione decisiva) | 1613: 562 prove, 378 senza esito a favore, 673 col punteggio decisivo contro (patta certificata, mai con l'orologio) |
| Esiti certificati | 569 in 150 partite (95 matti, 474 vittorie a foglie TB); 562 con valutazione precedente già decisiva, 7 prima |
| Senza esito | 7967 (7477 budget, 490 non certificabili con l'orologio), 409 s in tutto, mediana 54 ms, 801 ms per partita con avvio (~0,9 % degli 87 s di riflessione di `auto` per partita; §6: 110 ms, 0,2 %) |
| nps per partita, `auto` / `off` − 1 | +0,37 % ± 0,51 % (mediana −0,26 %) |
| nps, probe saltato (prima del primo avvio) | −0,25 % ± 0,52 % (696 partite) |
| nps, dopo un avvio e prima di una prova | +4,37 % ± 1,50 % (389 partite, 17627 mosse): lati opposti della stessa partita (attaccante contro difensore), non un costo del probe |
| nps, dopo la prima prova | non misurabile: 0 mosse usabili, ogni mossa rilegge la prova ed esce (H5) |

Conversione del lato in vantaggio per classe e confronto con §6 (`conversion.md`, `tests/results/g3-ltc/outcome-trigger/baseline-sub23.md`):

| Campione | posizioni | coppie | `auto` V/P/S | `off` V/P/S | punti `auto` − `off` per coppia [95 %] | vittorie [95 %] |
|---|---:|---:|---|---|---:|---:|
| questo, tutte | 23 | 500 | 155/344/1 | 173/327/0 | −0,019 [−0,044; +0,006] | −3,6 % [−8,8 %; +1,4 %] |
| classe F | 2 | 44 | 19/25/0 | 17/27/0 | +0,023 (2 posizioni: intervallo degenere) | +4,5 % |
| classe P | 20 | 434 | 136/297/1 | 156/278/0 | −0,024 [−0,053; +0,005] | −4,6 % [−10,6 %; +1,2 %] |
| classe M | 1 | 22 | 0/22/0 | 0/22/0 | 0 | 0 |
| §6 sulle stesse 23 posizioni (trigger decisivo) | 23 | 85 | 28/57/0 | 29/56/0 | −0,006 [−0,066; +0,057] | −1,2 % [−13,3 %; +11,4 %] |
| §6, set intero | 133 | 500 | 40 vittorie | 40 vittorie | +0,002 [−0,010; +0,014] | 0,0 % [−2,2 %; +2,4 %] |

Coppie non pari 115: 48 per `auto`, 67 per `off`. Delle 48 per `auto`, 46 hanno una prova certificata di `auto` col lato in vantaggio;
delle 67 per `off`, nessuna; le 7 prove anticipate (valutazione precedente da +8.67 a +14.39, non il +1..+3 della soglia) stanno in
3 coppie a favore di `auto` (round 10, 342, 425) e 4 pari. Nelle 107 coppie in cui entrambi convertono `auto` certifica la vittoria in 104: la prova segue la vittoria,
non la produce. La conversione di `off` è la stessa di §6 sulle stesse posizioni (34,6 % contro 34,1 %); quella di `auto` scende da
32,9 % a 31,0 %.

Dettagli: `conversion.md` (conversione per classe, 115 coppie non pari una per riga), `pairs.md` e `pairs-odd.csv` (coppie per classe),
`prover-log.md` e `prover-starts.csv` (registro del prover), `analysis-raw.md` (uscita di `analyze.py` col binario; in (C) le partite con almeno una mossa
con avvio diversa da quella di `Prover=off` ai nodi mediani: 355 su 511, quasi tutte avvii senza esito, quindi per lo più la
non-determinismo del numero di nodi, non la prova), `baseline-sub23.md` (§6 filtrato alle 23 posizioni), `book.out`, `fastchess.out`, `games.pgn.xz`.
