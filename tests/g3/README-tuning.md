# G3 — giro di tuning del prover alla radice (spec §13, attività a-b), 2026-10-20

Seguito di `README.md` (G3 non superato: NR a 10+0.1 −2,15 ± 2,43 Elo con `Prover=auto` e i default 100‰ / 10^6 /
depth 6; 24 timeout di `auto` contro 22 di `off`; esiti del prover in 10 partite su 13 574). Ipotesi residua di §13:
tempo speso alla radice senza esito. Lavoro: (1) trigger ristretto e memoria delle radici fallite, (2) verifica
della contabilità del tempo e dei timeout, (3) registro `ProverLog`, (4) verifiche di M3 ripetute, (5) nuova NR a
10+0.1 con `ProverBudget=30`. Risultati in `tests/results/g3-tuning/` (binario congelato `stockfish`, sha256
`9e73fac1…`, ignorato da git; `run.sh` è la catena eseguita; `chain.log` i tempi).

## 1. Causa dei timeout della NR di G3 (24 `auto` / 22 `off`)

Analisi dei 46 forfeit del PGN di `g3-nr` (`tests/results/g3-tuning/g3-nr-forfeits.csv`: per ogni perdita per il
tempo pezzi alla radice, punteggio precedente, tempo rimasto, tempo speso, `tm.maximum()` ricostruito come
`0,8097·tempo − 10 ms`, sforamento misurato da fastchess):

| | `auto` | `off` |
|---|---:|---:|
| forfeit | 24 | 22 |
| con punteggio precedente decisivo (trigger del prover) | 0 | 0 |
| a ≤ 7 pezzi (trigger "pochi pezzi" di M3) | 7 | 3 |
| di cui a ≤ 5 pezzi (radice in TB: il prover non parte) | 3 | 0 |
| mossa del forfeit registrata nel PGN | 16 | 12 |
| tempo speso oltre `tm.maximum()` in quelle mosse: mediana / max (ms) | 25 / 155 | 31 / 270 |
| sforamento dell'orologio (fastchess): mediana / max (ms) | 102 / 175 | 107 / 256 |

- **Nessun bug di contabilità.** Verificato nel codice: `limits.startTime` è fissato al `go` (`uci.cpp:195`) e
  copiato in `tm.startTime` (`timeman.cpp:55`); `elapsed()` è `now() − startTime` (`timeman.h:52`), quindi comprende il
  prover, che gira dopo `tm.init` e prima di `threads.start_searching()` (`search.cpp:215`); dopo il prover
  `callsCnt = 0` (`prover.cpp:237`) e il primo nodo della ricerca controlla l'orologio; con quota < 10 ms il prover
  non parte (`prover.cpp:212-213`). La scadenza del prover era calcolata da `Clock::now()` all'avvio del prover
  anziché da `limits.startTime`: differenza di microsecondi (generazione delle mosse alla radice), ora corretta
  comunque (`prover.cpp:210-211`: la quota è ridotta del tempo già passato dal `go`).
- **Riproduzione del caso più sospetto** (round 3335, `auto` da nero: mossa `e4e5` a profondità 1, 22 nodi, 553 ms
  con 616 ms a disposizione): posizione a 5 pezzi con punteggio 0,00; rigiocata con il binario di G3 (`position fen
  … moves …`, `go wtime 229 btime 616 winc 100 binc 100`) `auto` e `off` cercano gli stessi 129 174 nodi in 110-117 ms,
  il prover non parte (radice in TB). I 553 ms per 22 nodi sono uno stallo del processo, non del motore.
- **Causa: stalli dell'ambiente**, simmetrici fra i lati (24/22 è compatibile con il caso: p ≈ 0,88 al test
  binomiale), senza swap (0 B usati, 8,5 GB disponibili) e con il desktop attivo (VS Code, Firefox, altre sessioni)
  durante le 28 ore della corsa: Stockfish si ferma a `tm.maximum()` ≤ 81 % del tempo rimasto, e il forfeit
  richiede che il processo resti fermo per ≥ 19 % del tempo rimasto + 10 ms (con ~300 ms rimasti, ≥ 70 ms), come
  nei 28 casi registrati (sforamento di `tm.maximum()` fino a 270 ms per `off`). Nei 4 forfeit di `auto` a 6-7
  pezzi (dove il prover di M3 poteva partire) la quota era ≤ 10 % di un optimum di 20-40 ms, cioè ≤ 4 ms, contati
  in `elapsed()`: non bastano a spiegare sforamenti di 100-170 ms.
- Partite perse per il tempo nella corsa di 400 partite con il nuovo binario: sezione 5.

## 2. Modifiche (diff rispetto all'albero di M3/G3; `git diff -- src/stockfish-hybrid`)

| Dove | Cosa |
|---|---|
| `prover.cpp:191-194` | **Trigger** di `Prover=auto`: solo `go mate` o `bestPreviousScore` decisivo (`is_decisive`: matto o TB). Tolto il trigger "pochi pezzi" (`≤ min(SyzygyProbeLimit, MaxCardinality) + 2`), che partiva a ogni mossa dei finali senza esito. `Prover=draw`/`mate` forzano come prima (nessun trigger). |
| `prover.cpp:55-57, 138, 216-223, 246-250` | **Memoria delle radici fallite** (solo `auto` senza `go mate`): mappa `chiave → (do_move, ms)` delle radici uscite con `UNKNOWN_BUDGET`; sulla stessa chiave si riparte solo se `ProverMaxNodes ≥ 2·do_move` e (con orologio) `quota ≥ 2·ms`; azzerata da `Prover::clear()` (`ucinewgame`, `Clear Hash`). Esiti PROVEN non certificati e UNKNOWN_CYCLE non entrano: una ri-ricerca sulla stessa radice li rilegge dalla tabella senza espansioni. |
| `prover.cpp:210-211` | **Quota misurata da `limits.startTime`**: `share = optimum·‰ − (now() − startTime)`; sotto 10 ms non parte. |
| `engine.cpp:178-182`, `prover.h:48`, `prover.cpp:49, 232-235, 238-243, 246-260` | **`ProverLog`** (check, default `false`): a ogni avvio `info string prover: start <win|nowin> budget [<ms> ms] <ProverMaxNodes> do_move`; senza esito certificato `info string prover: no result after <N> do_move in <T> ms (budget | not certified with a clock | not verified)`. Attivo in ogni modalità (non solo `auto`): più semplice e utile anche in analisi. fastchess le mette nel commento della mossa (`match_line`). |
| `tests/instrumented.py` (`TestProver`, 2 test) | `test_prover_auto_clock_within_limit`: bench 2 (matto in 4), `Prover=auto`, `go wtime 1000 btime 1000 movestogo 1` due volte: la prima senza prover (nessun punteggio precedente), la seconda con `start` + `proven mate in 4 move g4g7`, entrambe sotto 1000 ms di parete (`movestogo 1` perché con `wtime 1000` secco l'optimum è ~4 ms e la quota resta sotto 10 ms). `test_prover_auto_failed_root_not_retried`: `ProverMaxNodes=1` → `no result after 35 do_move`, la terza `go` sulla stessa radice non riparte, con `ProverMaxNodes=1000` riparte e prova. |
| `tests/g3/prover_log.py` | riepilogo delle righe del registro dal PGN (avvii, esiti, senza esito, tempo speso senza esito) + CSV per avvio; self-check `python3 tests/g3/prover_log.py`. fastchess 1.8.2 conserva nel commento della mossa solo l'**ultima** riga `match_line`: la riga `start` è sovrascritta da `no result`/`proven`, quindi un avvio = una riga finale (il budget in ms della riga `start` non arriva nel PGN). |
| `src/stockfish-hybrid/README.md` | tabella delle opzioni (`ProverLog`), riga H2 (trigger, memoria, quota), scostamento 5. |

Non fatto (fuori mandato): niente `k` adattivo, niente seconda direzione per mossa; il prover in `auto` con punteggio
precedente negativo cerca ancora NOWIN(opp) (con orologio mai certificato: solo bound per i probe).

## 3. Verifiche di M3 ripetute (binario `9e73fac1…`, g++ 16.2, `ARCH=x86-64-bmi2`, `make all` senza PGO)

| Verifica | Esito |
|---|---|
| (a) signature `bench` con `Prover=off` | **2497913** (identica) |
| (b) G4 overhead del probe (`tests/prover_overhead.sh <ref> 10 2`, riferimento Stockfish 19 ricompilato da una copia temporanea con la stessa riga; `tests/results/g3-tuning/overhead.md`) | **invariato**: `probe6` 100,45 % del riferimento (M3: 99,76 %), `probe8` 101,4 %, `probe4` 98,2 %, `probe1` 93,6 %; signature 2497913 in tutte le configurazioni. Il rumore del banco è ~±3 % (`off` 102,7 %, nps del riferimento 688 k contro 737 k di M3: macchina più carica), il probe a depth 6 resta senza costo misurabile; H3 non è stato toccato. |
| (c) `tests/instrumented.py --none` | **88/88** (TestProver 13/13: 11 di M3 + 2 nuovi) |
| (d) `PROVER=auto tests/reprosearch.sh` | **verde** (20 valori di `go nodes`, conteggi pari) |

## 4. Comandi

```bash
cd src/stockfish-hybrid && make -j8 ARCH=x86-64-bmi2 all && ./stockfish bench | grep Nodes   # 2497913
python3 tests/instrumented.py --none ./stockfish && PROVER=auto tests/reprosearch.sh
tests/prover_overhead.sh <stockfish-19-riferimento> 10 2
ADJ="-resign movecount=3 score=600 twosided=true -draw movenumber=34 movecount=8 score=20"
export EXE=tests/results/g3-tuning/stockfish; XOPT="option.ProverBudget=30 option.ProverLog=true"
tests/g3/sprt.sh tests/results/g3-tuning/pre400 tests/results/g3-tuning/openings-pre400.epd -3 1 200 10+0.1 "$XOPT" "$ADJ"   # 400 partite, ultime 200 aperture del libro
tests/g3/sprt.sh tests/results/g3-tuning/nr tests/books/openings8-filtered.epd -3 1 10000 10+0.1 "$XOPT" "$ADJ"            # NR, fino a 20000 partite
tests/g3/sprt.sh tests/results/g3-tuning/ltc tests/books/openings8-filtered.epd -3 1 750 60+0.6 "$XOPT" "$ADJ"              # (C) campione 60+0.6, 1500 partite (run-ltc.sh; ripresa dopo lo spegnimento: resume-ltc.sh, `fastchess -config file=config.json`)
python3 tests/g3/analyze.py <dir>/games.pgn > <dir>/analysis.md; python3 tests/g3/prover_log.py <dir>/games.pgn <dir>/prover-starts.csv
```

## 5. Corsa di 400 partite `auto` (30‰, log) vs `off` (prima della NR)

`tests/results/g3-tuning/pre400/` (ultime 200 aperture del libro, colori invertiti, stessa adjudication di G3; 50 min).

| Partite | Elo (logistic) | Pentanomiale | Timeout `auto` / `off` | Avvii del prover | Esiti certificati |
|---:|---|---|---:|---:|---:|
| 400 | +0,00 ± 15 (coppie [1, 43, 113, 43, 0]) | [1, 43, 113, 43, 0] | **7 / 5** | **0** | 0 |

- **Perdite per il tempo: 12 su 400 (3 %)**, dieci volte il tasso della NR di G3 (0,34 %), su entrambi i lati e con
  **0 avvii del prover**: non possono venire dal prover. Nelle 4 mosse registrate lo sforamento di `tm.maximum()` è
  di 18-58 ms (una addirittura −19 ms: il motore si è fermato in tempo e il processo è rimasto fermo dopo), negli
  altri 8 casi il motore non ha nemmeno prodotto la mossa (sforamento 101-189 ms). Durante la corsa il desktop era
  attivo (Firefox al 15 % di CPU, carico medio 3-4 su 8 thread logici): è l'ambiente del banco, come nella NR di G3.
- **Il prover non parte mai con `ProverBudget=30` a 10+0.1.** Le mosse di `auto` con punteggio precedente decisivo
  (il nuovo trigger) sono 17 su 21 625, tutte a fine partita con poco tempo sull'orologio: `tm.optimum()` ricostruito
  mediano 63 ms (p90 95 ms), quindi quota 30‰ = 2-3 ms e 100‰ = 6-10 ms, sotto la soglia di 10 ms di M3 (1 mossa su 17
  l'avrebbe superata a 100‰). Verifica diretta sul bench 2 con `go wtime 500 btime 500 winc 100 binc 100` e punteggio
  precedente di matto: nessun avvio a 30‰ e a 100‰, avvio (e matto provato) a 1000‰.
- Conseguenza: la NR con `ProverBudget=30` (sezione 6) misura il trigger ristretto più il probe, cioè un motore in cui il
  prover alla radice non spende nulla; un esito positivo dice "`auto` con questi parametri è innocuo a 10+0.1", non
  "il prover alla radice non costa Elo". Segnalato alla sessione madre prima della NR (vedi sezione 7).

## 6. Nuova NR a 10+0.1: `Prover=auto`, `ProverBudget=30`, `ProverLog=true` vs `off`, SPRT [−3, 1]

`tests/results/g3-tuning/nr/` (`run-nr.sh`): stesso libro, adjudication, concorrenza e modello (logistic) della NR di
G3; 10 000 round = fino a 20 000 partite; avviata il 2026-10-20 alle 09:05 dopo la corsa di 400. **Lettura concordata
con la sessione madre prima dell'avvio** (sezione 5): a 10+0.1 la quota di 30‰ resta sotto i 10 ms della soglia di
M3 su tutte le mosse con punteggio precedente decisivo, quindi il prover alla radice non viene mai avviato e la NR
misura esattamente la configurazione che rilegge la regressione di G3: `auto` con trigger ristretto e probe H3
(cioè senza alcun tempo speso alla radice) contro `off`. Se passa, la regressione di G3 non è del probe né della
contabilità del tempo e resta imputabile al solo tempo speso dal trigger "pochi pezzi" (tolto); se fallisce, la
regressione è nel probe o nel banco (e l'A/A di G3, +3,5 ± 5,3, non aveva la precisione per escluderlo).

| Corsa | Partite | Elo (logistic) | nElo | LOS | LLR (±2,94) | Pentanomiale [0..2] | Esito | Durata | Timeout `auto`/`off` | Partite con prover avviato / con esito | Tempo speso senza esito |
|---|---:|---|---|---:|---:|---|---|---:|---:|---:|---:|
| NR `auto` (30‰, log, trigger ristretto) vs `off` | 20000 | **−1,39 ± 2,02** | −3,31 ± 4,82 | 8,9 % | −1,46 | [57, 1583, 6796, 1511, 53] | **nessuno: tetto raggiunto** (né H0 né H1) | 41 h 03 | 22 / 33 | **0 / 0** | 0 |

Nessun crash né mossa illegale; patte 66,7 %; terminazioni: 13 938 adjudication, 6 007 normali, 55 forfeit (0,28 %).
Statistiche per motore (`nr/analysis.md`): tempo per mossa 0,267 s medio / 0,156 s mediano / 1,76 s p99 per
entrambi; profondità media 17,09 (`auto`) contro 17,15 (`off`); nodi medi per mossa 87 024 contro 87 772 (−0,85 %).
Registro (`nr/prover-log.md`, `nr/prover-starts.csv`): 0 avvii in 20 000 partite, come previsto dalla sezione 5.

**Lettura.**
1. **Lo SPRT non decide**: dopo 20 000 partite l'LLR è a −1,46 su ±2,94 (LOS 8,9 %). Il punto stimato −1,4 Elo con
   intervallo al 95 % [−3,4; +0,6] esclude una regressione ≥ 3,4 Elo e non esclude né zero né una perdita di 1-2 Elo.
   Per §11 (serve H1) la non regressione **non è superata**, ma nemmeno accettata H0 come in G3.
2. **Il prover alla radice non c'entra**: 0 avvii, 0 ms spesi alla radice, trigger "pochi pezzi" tolto. L'unica
   differenza fra i lati è il probe H3 su una tabella sempre vuota (più la chiamata vuota di `prove_root`). La NR di G3
   (−2,15 ± 2,43, prover avviato in ~10 partite su 13 574 e per pochi ms) e questa (−1,39 ± 2,02) sono compatibili fra
   loro e con un costo del probe dell'ordine di 1 Elo: i nodi per mossa di `auto` sono −0,85 % qui e −0,25 % in G3 a
   parità di tempo (il bench a un thread misura −0,24 %; con 4 processi che condividono la L3 un accesso casuale in
   16 MiB per nodo a depth ≥ 6 può costare di più). Il controllo "solo probe" di G3 (−0,58 ± 5,09 su 3 000 partite) non
   ha la precisione per confermarlo o escluderlo; l'A/A di G3 (+3,47 ± 5,32) nemmeno.
3. **Timeout**: 55, 22 di `auto` e 33 di `off` (0,28 %), tutti stalli dell'ambiente come in G3 (sezione 1): il lato con il
   prover ne ha di meno. La contabilità del tempo non è in discussione.
4. **Costo**: 41 h di macchina (il tetto di 20 000 partite supera il budget di 1-1,5 giorni di ~10 h).

## 7. Raccomandazione finale sui parametri

Decisioni della sessione madre (2026-10-20, 09:08): (A) la NR a 30‰ resta in corso; (B) la soglia dei 10 ms non si
abbassa; (C) campione a 60+0.6 (`run-ltc.sh`, 1500 partite, `ProverBudget=30`, `ProverLog=true`), avviato il
2026-10-24 su via dell'utente dopo la NR: sezione 8.

- `Prover=off` resta il default per giocare. `ProverProbeDepth=6`: tenere (overhead invariato, sezione 3).
- `Prover=auto` con il trigger ristretto: a 10+0.1 non spende nulla alla radice con qualunque `ProverBudget ≤ 100‰`
  (quota sotto i 10 ms sulle mosse decisive, che arrivano a fine partita con poco tempo); il suo costo è solo il
  probe, stimato dalla NR a −1,4 ± 2,0 Elo (sezione 6): **a 10+0.1 `auto` non serve a nulla e forse costa ~1 Elo**.
  A 60+0.6 (sezione 8) il prover gira in 10 partite su 1500 (16 avvii, 2 vittorie certificate, 0,3 s totali senza
  esito) e il campione dà −5,8 ± 5,5: ancora non il prover alla radice, ma di nuovo `auto` con meno nodi per mossa
  (−1,2 %; −0,85 % nella NR, −0,25 % in G3) a parità di tempo. **Il candidato è il costo del probe H3 in partita**
  (accesso casuale in una tabella di 16 MiB, vuota, a ogni nodo con depth ≥ 6, con 4 processi che condividono la L3;
  sul bench a un thread costa −0,24 %). Seguito proposto, fuori da questo giro: (i) saltare il probe finché la tabella
  non ha entry PROVEN (un flag alzato da `prove_root`: a 10+0.1 `auto` diventa identico a `off`), (ii) `ProverHash`
  piccolo o prefetch dell'entry del figlio (§8.6), (iii) `ProverProbeDepth=8`; da misurare con una NR "solo probe"
  (`ProverMaxNodes=0`) a ≥ 20 000 partite a 10+0.1, l'unica corsa che decide questa ipotesi.
- Per far lavorare il prover in partita servono cadenze più lunghe (60+0.6: campione (C)) oppure, in analisi,
  `Prover=mate`/`go mate` e `ProverBudget` alto: a 500 ms sull'orologio solo `ProverBudget=1000` fa partire il prover
  (quota 31 ms, matto in 4 del bench 2 provato in 1 ms).
- La memoria delle radici fallite e la quota da `startTime` sono corrette ma a 10+0.1 non entrano mai in gioco; le
  prove sono i due test di `instrumented.py`.
- Trigger "pochi pezzi": non reintrodurlo a cadenze brevi; a 60+0.6 (dove la quota basta) potrebbe tornare utile nei
  finali a 6-7 pezzi, ma solo con la memoria dei fallimenti e con un set sensibile (§13 (c)), fuori da questo giro.

## 8. Campione a 60+0.6 (attività (c)/(d) di §13): `auto` (30‰, log) vs `off`, SPRT [−3, 1], 750 round

`tests/results/g3-tuning/ltc/` (`run-ltc.sh`): stesso libro (aperture 1-750), adjudication, concorrenza, binario e
modello della NR; 1500 partite. Avviato il 2026-10-24 alle 08:26 su via dell'utente; **spegnimento improvviso della
macchina alle 22:37 dopo 1182 partite**; ripresa alle 22:43 con `fastchess -config file=config.json` (`resume-ltc.sh`),
che riparte dal round successivo alle statistiche autosalvate (1178 partite = round 589): le 4 partite dei round
590-591 sono state tolte dal PGN e rigiocate (PGN finale: 1500 partite, 2 per round). Durata 14 h 11 + 3 h 58.

| Corsa | Partite | Elo (logistic) | nElo | LOS | LLR (±2,94) | Pentanomiale [0..2] | Esito | Timeout `auto`/`off` | Partite con prover avviato / con esito | Avvii / esiti certificati / senza esito | Tempo speso senza esito |
|---|---:|---|---|---:|---:|---|---|---:|---:|---|---:|
| 60+0.6 `auto` (30‰, log, trigger ristretto) vs `off` | 1500 | **−5,79 ± 5,53** | −18,4 ± 17,6 | 2,0 % | −2,38 | [0, 86, 604, 59, 1] | **nessuno: tetto di 1500** (LLR all'81 % verso H0) | 1 / 4 | **10 / 1** | 16 / 2 / 14 (12 budget, 2 non certificati con l'orologio) | 0,3 s (mediana 16 ms, max 55 ms; 25 ms per partita con avvio) |

Nessun crash né mossa illegale; patte 80,5 %; terminazioni: 1044 adjudication, 451 normali, 5 forfeit (0,33 %, 1 di
`auto` e 4 di `off`; il contatore finale di fastchess, 1/3, copre solo la parte ripresa). Statistiche (`ltc/analysis.md`):
tempo per mossa 1,58 s medio / 1,04 s mediano / 11,2 s p99 per entrambi; profondità 24,57 (`auto`) contro 24,69; nodi
per mossa 544 731 contro 551 448 (−1,2 %). Registro (`ltc/prover-log.md`, `ltc/prover-starts.csv`): 16 avvii in 10
partite; do_move senza esito mediani 22 901 (max 176 134 in 55 ms); le 2 prove sono `proven win (tablebase leaves)`
nella stessa partita (vinta). Coppie con il prover: [0, 2, 7, 1, 0]; nelle 9 partite "altro" `auto` fa 3 V / 6 S: il
trigger scatta anche con punteggio decisivo negativo (ricerca NOWIN(opp), mai certificata con l'orologio), cioè in
partite già perse.

**Lettura (campione, non SPRT risolutivo).**
1. A 60+0.6 il prover alla radice gira davvero (quota 11-55 ms) ma di rado: 16 avvii in 1500 partite, 2 esiti, 0,3 s
   totali senza esito. Il suo effetto sul risultato è di 3 coppie non pari su 10 (−0,5 punti di coppia): nullo rispetto
   alle 146 coppie non pari totali (86 contro 59 a favore di `off`).
2. Il punto stimato −5,8 ± 5,5 (intervallo [−11,3; −0,3], LOS 2 %) è compatibile con la NR a 10+0.1 (−1,4 ± 2,0) e con la
   NR di G3 (−2,2 ± 2,4): tutte e tre vedono `auto` leggermente sotto `off` senza che il prover alla radice spenda
   tempo. Con 1500 partite e 80 % di patte il campione non decide (serve ~4× per un intervallo di ±2,7).
3. In tutte e tre le corse `auto` cerca meno nodi per mossa a parità di tempo (−0,25 %, −0,85 %, −1,2 %): l'ipotesi
   coerente è il costo del probe H3 in partita (sezione 7), non misurabile dal bench a un thread.
4. Il trigger "punteggio decisivo" fa partire il prover anche in posizioni perse (6 delle 9 partite "altro"): a budget
   invariato è tempo speso senza possibile esito con l'orologio (NOWIN(opp) non si certifica); limitare il trigger a
   `prev > 0` (o `go mate`) toglierebbe queste ricerche. Non fatto in questo giro.
