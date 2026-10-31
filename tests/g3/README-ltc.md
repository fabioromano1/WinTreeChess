# G3 — misure a cadenza lunga (spec §13.1, punti 2-3 di "resta da fare"), 2026-10-25/26

Seguito di `README-tuning.md` (§7-8: a 60+0.6 `auto` −5,79 ± 5,53 Elo su 1500 partite con 16 avvii del prover; in
tutte le corse `auto` cerca meno nodi per mossa a parità di tempo). Lavoro: (1) G4 a 60+0.6, costo del probe H3 con il
prover alla radice spento; (2) set di finali sensibile `tests/books/endgames-hard.epd`; (3) campione LTC sul set,
2000 partite a 60+0.6 (avviato il 2026-10-26 sul via dell'utente). Risultati in `tests/results/g3-ltc/`.

Binario congelato `tests/results/g3-ltc/stockfish` (ignorato da git), sha256 `6f86e835…cff1a`
(`binary.sha256`), compilato con `make -j8 ARCH=x86-64-bmi2 all` da `git archive HEAD` (78b5dee) più i sorgenti
`syzygy/` e la rete `.nnue`, che git ignora; `bench` 2497913. Nota: il `.gitignore` della radice ignora `syzygy/`
anche in `src/stockfish-hybrid/`, quindi un clone pulito non compila l'ibrido (fuori da questo giro).

Script: `tests/g3/g4_depth.sh` (G4a), `hard_endgames.py` + `build_hard_book.py` (set), `ltc_pairs.py` (coppie per
classe del libro), `analyze.py` (ora anche nps aggregata e rapporto di nps per partita), `sprt.sh` (`elo0 = -`:
numero fisso di round, nessun SPRT); catena `tests/results/g3-ltc/run.sh`, tempi in `chain.log`.

## 1. G4a: nps su posizioni fisse a depth 30

`tests/g3/g4_depth.sh`: `bench 16 1 30 default depth` (51 posizioni, Hash 16, un thread, `taskset`), `off` contro
`probeN` = `Prover=auto`, `ProverMaxNodes=0`, `ProverProbeDepth=N`. Tutte le 28 corse cercano lo stesso albero
(742 212 930 nodi): il rapporto di nps è il rapporto dei tempi. CSV: `g4a-depth30.csv`.

- **Concorrente** (le 4 configurazioni insieme sui core fisici 0-3, L3 condivisa come 4 partite in parallelo; core
  ruotati a ogni ripetizione: quadrato latino 4×4, ~35 min per corsa). Stima con effetti di ripetizione, core e
  configurazione sul log degli nps (residuo σ = 0,33 %, 6 gradi di libertà; intervallo t al 95 %):

| Configurazione | nps medio | rispetto a `off` |
|---|---:|---:|
| off | 354 965 | — |
| probe6 | 353 545 | **−0,39 % ± 0,57 %** |
| probe8 | 353 987 | −0,27 % ± 0,57 % |
| probe10 | 353 902 | −0,30 % ± 0,57 % |

  Effetto del core: core 0 −0,93 % (interrupt), core 1-3 da +0,25 % a +0,39 %: è grande quanto l'effetto cercato, per
  questo la rotazione.
- **Isolata** (una configurazione alla volta sul core 2, ordine ruotato, 3 ripetizioni, ~22 min per corsa):
  inutilizzabile. Gli nps di `off` vanno da 494 657 a 582 178 (−15 %); i rapporti per ripetizione hanno σ 8-12 %. Con
  un solo core attivo il turbo e il carico del desktop (browser, editor) dominano. Le corse concorrenti condividono
  il rumore e lo cancellano nel confronto.

**Lettura.** Su posizioni fisse, anche a depth 30 e con 4 processi sulla L3, il probe costa ~0,3-0,4 %, entro la
soglia di §11 (≤ 1 %), e **`ProverProbeDepth` 6, 8 e 10 non si distinguono** (differenze ≤ 0,12 % contro ±0,57 %).

## 2. G4b: 400 partite a 60+0.6, solo probe contro `off`

`tests/results/g3-ltc/g4b/`: `auto` con `ProverMaxNodes=0` (`ProverProbeDepth=6`, default; `ProverLog=true`) contro
`off`, aperture 1001-1200 di `openings8-filtered.epd` (`openings-g4b.epd`), 200 coppie, adjudication della NR,
concorrenza 4; 4 h 48. Nessun avvio del prover (0 righe `info string prover`), nessun timeout, nessun crash.

| | `auto` (solo probe) | `off` | differenza |
|---|---:|---:|---:|
| mosse | 21 641 | 21 642 | |
| tempo per mossa medio / mediano (s) | 1,591 / 1,038 | 1,588 / 1,054 | +0,2 % |
| nodi per mossa medi | 495 939 | 503 299 | **−1,46 %** |
| nps aggregata (Σ nodi / Σ tempo) | 311 813 | 316 985 | **−1,63 %** |
| profondità media | 24,10 | 24,36 | −0,26 |
| nps per partita, `auto` / `off` − 1 (media, 95 %) | | | **−1,27 % ± 0,82 %** (mediana −1,15 %) |

Elo −6,08 ± 10,33, pentanomiale [0, 22, 163, 15, 0] (400 partite: non informativo).

**L'albero è lo stesso.** Diagnostica: le posizioni di 2 partite di G4b (121 ricerche, TT conservata fra le mosse come in
partita, `go nodes 300000`) danno con `off` e con il solo probe la stessa profondità, gli stessi nodi e la stessa
mossa in tutti i casi. La differenza in partita è quindi velocità, non ricerca diversa.

**Stesso confronto sulle corse precedenti** (`analyze.py` aggiornato sui loro PGN; in tutte il prover alla radice è
partito al più 16 volte):

| Corsa | partite | nps aggregata `auto` vs `off` | nps per partita, media ± 95 % | mediana |
|---|---:|---:|---:|---:|
| G3 solo probe, 10+0.1 (`g3-probeonly`) | 3000 | −0,78 % | −0,27 % ± 0,37 % | −0,79 % |
| NR di tuning, 10+0.1, 30‰ (`g3-tuning/nr`) | 20 000 | −0,89 % | −0,37 % ± 0,14 % | −0,92 % |
| campione 60+0.6, 30‰ (`g3-tuning/ltc`) | 1500 | −1,26 % | −0,75 % ± 0,41 % | −1,11 % |
| **G4b, 60+0.6, solo probe** | 400 | **−1,63 %** | **−1,27 % ± 0,82 %** | −1,15 % |
| **G4b ripetuto, probe saltato a tabella senza PROVEN** (`g4b-skip`, sotto) | 400 | **+0,27 %** | **+0,51 % ± 0,78 %** | −0,14 % |

**Lettura (prudente).**
1. In partita il solo probe costa ~1 % di nps (mediana per partita da −0,8 % a −1,15 %, aggregata da −0,8 % a −1,6 %),
   2-4 volte il costo su posizioni fisse (−0,3/−0,4 %). La differenza fra le due misure non è spiegata: l'albero è
   lo stesso; in partita ci sono 8 processi vivi (4 partite × 2 motori, ciascuno con TT da 16 MiB e tabella delle prove
   da 16 MiB) contro 4 del bench, più `go wtime` e le Syzygy 5-man alla radice. L'ipotesi più semplice è la pressione su
   L3/TLB della tabella in più, non il numero di probe (che `ProverProbeDepth` non cambia in modo misurabile).
2. A 60+0.6 il punto stimato (−1,27 % per partita, −1,63 % aggregato) supera la soglia di §11 (≤ 1 %) ma l'intervallo
   [−2,1 %; −0,45 %] la contiene: **G4 a 60+0.6 non è superato con margine** con `ProverProbeDepth=6`.
3. Il costo cresce con la cadenza nelle medie (10+0.1: −0,3/−0,4 %; 60+0.6: −0,75/−1,3 %), meno nelle mediane (−0,8/−0,9 % contro −1,1/−1,15 %).

**Raccomandazione sui parametri.** `ProverProbeDepth`: nessun valore fra 6 e 10 riduce il costo su posizioni fisse,
quindi alzarlo non è il rimedio; tenere 6 (default) finché una misura in partita non dice il contrario. Il rimedio
indicato è non pagare nulla finché la tabella è vuota: (i) saltare il probe finché `prove_root` non ha scritto almeno
una entry PROVEN (con `ProverMaxNodes=0`, e a 10+0.1 in pratica sempre, `auto` diventa identico a `off`); (ii) allocare
la tabella solo al primo avvio del prover, o `ProverHash` piccolo. Da misurare con lo stesso G4b (400 partite, ~5 h)
prima di qualunque uso di `auto` in partita. Fatto il 2026-10-28 (sotto).

### 2.1 G4b ripetuto con il probe saltato a tabella senza PROVEN (2026-10-28)

Rimedio (i)+(ii) in `src/stockfish-hybrid` (README della copia, sezione "Probe H3 saltato a tabella senza PROVEN"):
il probe H3 parte solo se `Prover::provenEntries` (scritture PROVEN dall'ultimo `ucinewgame`) è > 0, e la tabella è
allocata al primo avvio del prover. Con `ProverMaxNodes=0` `auto` fa per nodo un solo test di un intero in più di `off`.

`tests/results/g3-ltc/g4b-skip/` (`run.sh`): stesso blocco di G4b (aperture 1001-1200, 200 coppie, 60+0.6,
adjudication della NR, concorrenza 4), binario sha256 `5d91e4d6…cad607`; 4 h 39. 0 righe `info string prover`, nessun
timeout, nessun crash, nessuna disconnessione.

| | `auto` (solo probe, saltato) | `off` | differenza |
|---|---:|---:|---:|
| mosse | 20 837 | 20 852 | |
| tempo per mossa medio / mediano (s) | 1,596 / 1,073 | 1,598 / 1,060 | −0,1 % |
| nodi per mossa medi | 590 598 | 589 847 | +0,13 % |
| nps aggregata (Σ nodi / Σ tempo) | 370 100 | 369 104 | +0,27 % |
| profondità media | 25,37 | 25,29 | +0,08 |
| nps per partita, `auto` / `off` − 1 (media, 95 %) | | | **+0,51 % ± 0,78 %** (mediana −0,14 %) |

Elo +5,21 ± 9,30, pentanomiale [0, 12, 170, 18, 0] (400 partite: non informativo).

**Prima delle partite** (stesso binario): 121 ricerche di controllo (`tests/g3/replay_nodes.py`, prime 2 partite di
G4b, `go nodes 300000`, TT conservata) con profondità, nodi e mosse identici fra binario nuovo e precedente × `off` e
`auto`/`ProverMaxNodes=0` (`g4b-skip/replay/`); `bench` con `tests/prover_overhead.sh` (`g4b-skip/overhead-bench-*.md`,
10 corse, `taskset -c 2`): probe1 da 96,93 % a 100,01 % del riferimento, probe6 da 98,82 % a 99,41 % (le differenze
residue fra 99,4 % e 100 % sono rumore fra corse: probe1, il caso con più probe, è a 100,01 %).

**Lettura.** Il costo misurato di G4b (−1,27 % ± 0,82 %) sparisce: la stima per partita (+0,51 % ± 0,78 %) è
compatibile con zero e la mediana è −0,14 %; gli intervalli prima e dopo non si sovrappongono ([−2,09 %; −0,45 %]
contro [−0,27 %; +1,29 %]). **G4 a
60+0.6 è superato** (§11, ≤ 1 %) per `auto` senza avvii del prover. L'nps assoluta è ~16 % più alta che nel G4b
precedente per entrambi i motori (anche per `off`, che il cambiamento non tocca): stato della macchina diverso fra i due
giorni, che non cambia il confronto interno alla corsa. Quando il prover scrive una entry PROVEN il probe torna attivo e
costa come prima (non misurato qui: richiede avvii del prover, come nel campione della sezione 4).

## 3. Set di finali sensibile: `tests/books/endgames-hard.epd` (481 posizioni)

Fonte: le 2000 partite `off` contro `off` a 2+0.02 di G3 (`tests/results/g3-books/eggen-games.pgn.xz`, senza
adjudication) e `tests/suites/mates2000.epd`. `hard_endgames.py` (~10 min con 4 processi):

- classi: **A** = 6-7 pezzi (re inclusi: fuori dalle 5-man); **B** = 8-10 pezzi con ≤ 3 pedoni;
- per partita, posizioni con il lato al tratto a +1..+8 nel commento della mossa giocata (prefiltro a 2+0.02), almeno
  8 semimosse fra loro, al massimo 4 per classe; poi `go depth 20` (`Prover=off`, Hash 16, Syzygy 5-man): tiene
  **150 ≤ cp ≤ 600 per il lato al tratto** (matti esclusi), al massimo 3 per partita; FEN duplicate tolte;
- WDL delle posizioni a 6 pezzi dalle 6-man (`~/syzygy/5man:~/syzygy/6man`, `go depth 1` con radice in TB).

`build_hard_book.py` aggiunge **M** = 100 matti lunghi da `mates2000` (N ≥ 10 con il lato al tratto che matta, ≥ 6 pezzi
perché a 5 l'adjudication a TB chiuderebbe la partita alla prima mossa; presi a passo costante nell'ordine di N) e
mescola con seme fisso. CSV con classe, pezzi, pedoni, eval, WDL ed esito d'origine: `tests/books/endgames-hard.csv`
(candidati: `tests/results/g3-ltc/endgames-candidates.{epd,csv}`; distribuzione: `book-distribution.md`).

Primo giro (2 per partita, passo 12, 3 per classe): 279 finali, sotto il minimo di 300; la catena si è fermata da
sola e il giro è stato ripetuto con i limiti attuali. Nel primo giro il WDL risultava "fuori TB" per un `~` non
espanso nel secondo percorso: corretto.

| Pezzi | posizioni | pedoni 0/1/2/3+ |
|---:|---:|---|
| 6 | 38 | 0/6/32/0 |
| 7 | 97 | 0/3/22/72 |
| 8 | 132 | 0/2/56/74 |
| 9 | 94 | 0/0/7/87 |
| 10 | 20 | 0/0/5/15 |

| Eval d20 (cp, lato al tratto) | A (135) | B (246) |
|---|---:|---:|
| 150-249 | 11 | 30 |
| 250-349 | 9 | 44 |
| 350-449 | 19 | 57 |
| 450-600 | 96 | 115 |

- WDL dalle 6-man (38 posizioni a 6 pezzi): **37 vinte, 1 patta, 0 perse** per il lato al tratto. Le 7-10 pezzi non hanno
  WDL; esito della partita d'origine (2+0.02) per il lato al tratto: A 125/9/1, B 218/24/4 (V/P/S).
- Matti (M): N da 10 a 22, mediana 12; da 6 a 26 pezzi.
- Limiti: nessun finale senza pedoni (il prefiltro e la regola dei 3 pedoni favoriscono torre/alfiere con pedoni);
  posizioni dalla stessa partita (fino a 3) non sono indipendenti; con 1000 coppie e 481 posizioni fastchess ricomincia
  il libro (`idx % size`): ogni posizione è giocata in ~2 coppie.

## 4. Campione LTC sul set sensibile: `auto` (30‰, log) contro `off`, 60+0.6, 1000 coppie

`tests/results/g3-ltc/ltc/` (`run-ltc.sh`, il blocco del campione di `run.sh`): `auto` (`ProverBudget=30`,
`ProverLog=true`, trigger ristretto) contro `off` a 60+0.6 su `endgames-hard.epd` in ordine (ogni posizione in ~2
coppie), 1000 coppie fisse a colori invertiti, nessun SPRT, concorrenza 4, adjudication solo a TB 5-man
(`-tb ~/syzygy/5man -tbpieces 5`), nessuna resign/draw adjudication. Sul via dell'utente (2026-10-25 19:56: non
avviarlo da solo; 2026-10-26: via) avviato alle 08:06, finito alle 23:41 (14 h 52: le partite dal finale sono più corte
della stima di 25 h). PGN `games.pgn.xz` (6,1 MB non compresso, ignorato da git); `analysis.md`, `prover-log.md`,
`prover-starts.csv` (un avvio per riga), `pairs.md` (classi del libro), `pairs-odd.csv` (coppie non pari).

| Partite | Elo (logistic) | nElo | Pentanomiale [0..2] di `auto` | V/S/P | Timeout `auto`/`off` | Terminazioni |
|---:|---|---|---|---|---:|---|
| 2000 | **+0,17 ± 1,13** | +2,3 ± 15,2 | **[1, 1, 995, 2, 1]** | 988 / 987 / 25 | 1 / 2 | 1455 TB, 542 normali (matto, ripetizione), 3 tempo |

**Registro del prover** (`prover-log.md`; fastchess tiene solo l'ultima `match_line` per mossa, quindi gli avvii
contati sono le mosse con una riga del prover):

| | |
|---|---:|
| partite con almeno un avvio | 1870 su 2000 (sul libro di aperture: 10 su 1500) |
| avvii | 12 736 |
| esiti certificati | 5086 in 953 partite (787 con una vittoria a foglie TB, 95 con un matto in ≥ 10; molti sono riletture della prova già in tabella: `nodes 0 time 0 ms`) |
| senza esito: budget | 3829, 245,7 s in tutto, mediana 68 ms, max 152 ms |
| senza esito: non certificabile con l'orologio (NOWIN) | 3821, 12,9 s in tutto, mediana 0 ms (2076 con 0 do_move: la memoria dei fallimenti li ferma subito) |
| tempo senza esito per partita con avvio | 138 ms (contro ~52 s di riflessione di `auto` per partita: ~0,3 %) |
| avvii nella mossa di una perdita per il tempo | 0 |

**Coppie per classe del libro** (`pairs.md`):

| Classe | coppie | pentanomiale [0..2] di `auto` | punti di `auto` | coppie con il prover | non pari | non pari con il prover |
|---|---:|---|---:|---:|---:|---:|
| A (6-7 pezzi) | 279 | [1, 0, 278, 0, 0] | 278 / 558 | 277 | 1 | 1 |
| B (8-10 pezzi, ≤ 3 pedoni) | 515 | [0, 0, 514, 0, 1] | 516 / 1030 | 513 | 1 | 0 |
| M (matti lunghi) | 206 | [0, 1, 203, 2, 0] | 206,5 / 412 | 196 | 3 | 3 |
| tutte | 1000 | [1, 1, 995, 2, 1] | 1000,5 / 2000 | | | |

**Le 5 coppie non pari** (`pairs-odd.csv`):

| Round | Classe | Punti di `auto` | Causa |
|---:|---|---:|---|
| 65 | B, 8 pezzi, +355 | 2 | `off` perde per il tempo entrambe le partite: col lato perdente (23,3 s di sforamento: stallo dell'ambiente) e col lato vincente (1,6 s di sforamento dopo 36 s di riflessione) |
| 66 | A, 6 pezzi, TB W | 0 | `auto` perde per il tempo col lato vincente (2,2 s di sforamento con 18 s sull'orologio; la mossa non è nel PGN, ma un avvio del prover dura al più ~150 ms in questo campione): stallo dell'ambiente |
| 315 | M, #10, 22 pezzi | 1,5 | `auto` matta con `proven mate in 5` certificato; `off` col lato che matta patta per ripetizione |
| 469 | M, #11, 13 pezzi | 0,5 | `auto` col lato che matta patta per ripetizione (prover: budget e NOWIN senza esito); `off` matta |
| 950 | M, #11 (stessa posizione del 469) | 1,5 | `auto` matta con `proven mate in 9` certificato; `off` patta per adjudication TB |

Statistiche per motore (`analysis.md`): tempo per mossa 2,41 s (`auto`) contro 2,53 s, nodi per mossa 1,14 M contro
1,23 M, profondità 30,5 contro 39,9. Qui **non misurano il costo del probe**: le mosse con una prova escono subito
(H5, spesso `n=1`, `0.000s`) e il tempo del prover alla radice entra nel tempo della mossa ma non nei nodi; per questo
il rapporto di nps per partita (−4,42 % ± 0,88 %, mediana −2,10 %) non è confrontabile con G4b.

**Lettura (campione, prudente).**
1. Il set è sensibile per il prover, non per il risultato: il prover parte nel 94 % delle partite e certifica un
   esito nel 48 %, ma a 60+0.6 Stockfish converte quasi tutto anche senza (995 coppie pari su 1000; 25 patte su 2000).
   150-600 cp a depth 20 con 6-10 pezzi non sono posizioni "difficili" per Stockfish a questa cadenza.
2. Effetto sul risultato: nullo entro la risoluzione. Le coppie non pari sono 5: 2 decise da perdite per il tempo
   dell'ambiente (una coppia a favore di ciascun motore), 3 sui matti lunghi (2 a favore di `auto` con
   matto certificato, 1 a favore di `off` sulla stessa posizione di una delle due). Elo +0,17 ± 1,13; nElo +2,3 ± 15,2.
3. Costo alla radice: 138 ms senza esito per partita con avvio, ~0,3 % del tempo di riflessione; le ricerche NOWIN
   (trigger con punteggio decisivo negativo) sono 3821 ma costano 12,9 s in tutto grazie alla memoria dei fallimenti
   e al tetto di 4096. Non producono mai un esito con l'orologio: toglierle (trigger `prev > 0`) resta un'opzione
   gratuita, non una necessità.
4. Il campione non dice nulla sul costo del probe (G4b, sezione 2), che resta l'unico effetto negativo misurato di
   `auto` in partita.

## 5. Raccomandazione aggiornata sui parametri

- **`Prover=off` resta il default per giocare.** Anche sulle posizioni in cui il prover lavora davvero (set sensibile,
  60+0.6, 1870 partite su 2000 con avvii) il risultato non cambia: Stockfish converte da solo. Il valore del sistema
  resta la certificazione in analisi (`Prover=mate`, `wintree-chess`), come in §13 della spec.
- **`ProverProbeDepth=6`**: tenere. Su posizioni fisse a depth 30, 6/8/10 costano uguale (−0,3/−0,4 % ± 0,6 %); in
  partita il solo probe costa ~1,3 % ± 0,8 % a 60+0.6 (G4: non superato con margine). Il rimedio non è il gating ma
  non pagare nulla a tabella vuota (probe saltato finché non c'è una entry PROVEN; tabella allocata al primo avvio):
  fatto e verificato con G4b ripetuto (sezione 2.1: +0,51 % ± 0,78 % di nps per partita, prima −1,27 % ± 0,82 %).
- **`ProverBudget=30`, trigger ristretto, memoria dei fallimenti**: corretti e a basso costo anche dove il prover
  parte spesso (0,3 % del tempo); il ramo NOWIN può essere tolto senza perdite.
- **Un set più sensibile per il risultato** dovrebbe essere scelto per esito, non per valutazione: posizioni che `off`
  non converte a 60+0.6 in una corsa preliminare (o finali a 7 pezzi con DTZ lunga), più matti lunghi a ≥ 15 mosse,
  dove già qui stanno le 3 coppie non pari. Fatto il 2026-10-31 (sezione 6): nessun effetto sull'esito.

## 6. Set scelto per esito e campione a 60+0.6 (2026-10-30/31)

Domanda (§5, ultimo punto): dove `off` non converte, il prover cambia l'esito? **No, entro l'intervallo**: punti di `auto`
meno punti di `off` col lato in vantaggio +0,002 per coppia [−0,010; +0,014], 40 vittorie contro 40.

Risultati in `tests/results/g3-ltc/outcome/` (`run.sh` con le fasi `filter`, `more`, `sample`; `analysis.md`,
`conversion.md`, `pairs.md`, `pairs-odd.csv`, `prover-log.md`, `prover-starts.csv`, `games.pgn.xz`), filtri in
`outcome-filter/` e `outcome-filter2/`, partite nuove in `outcome-gen/`. Binario sha256 `5d91e4d6…cad607` (lo stesso di
`g4b-skip`), `bench` 2497913.

### 6.1 Costruzione del set: `tests/books/endgames-outcome.epd` (133 posizioni)

Il lato in vantaggio è sempre il lato al tratto della FEN. `tests/books/endgames-outcome.csv` ha una riga per posizione,
nello stesso ordine: classe, pezzi, pedoni, valutazione o matto, WDL 6-man, provenienza (PGN, round, ply, oppure riga di
`mates2000`), esito delle 2 partite del filtro.

1. **Candidati da partita** (`outcome_candidates.py`): per partita e per lato che non l'ha vinta, la prima posizione con quel
   lato al tratto a ≥ +3,00 o a matto nel commento della sua mossa, con ≥ 6 pezzi. **Le partite a 60+0.6 non bastano**: dalle
   2300 da libro di aperture zero candidati (nelle patte l'eval non arriva a +3: massimo 2,x), dalle 2000 sul set
   sensibile 2. Fonti aggiunte rispetto al mandato: le corse a 10+0.1 e le 2000 partite a 2+0.02 di `g3-books`
   (205 candidati in tutto, 90 da queste ultime).
2. **Candidati da matti** (`mate_candidates.py`): dei 243 matti di `mates2000` con N ≥ 10 e ≥ 6 pezzi, i 66 che `off` non vede
   con `go movetime 1500`.
3. **Primo filtro**: 267 candidati (tolte 4 posizioni a 6 pezzi non vinte nelle 6-man), 2 partite `off` contro `off` a colori
   invertiti, 60+0.6, adjudication solo TB 5-man, concorrenza 4: 534 partite, 4 h 24. Restavano 84 posizioni, sotto l'obiettivo
   di 150.
4. **Secondo giro** (fase `more`, fuori mandato: partite nuove, non già giocate): 6000 partite `off` contro `off` a 2+0.02 dal
   libro di aperture (seme 8, nessuna adjudication, concorrenza 8; 1 h 25), 262 candidati, 260 dopo le 6-man (2 patte), stesso
   filtro: 520 partite, 5 h 22.
5. **Regola di selezione**: resta una posizione con almeno una partita del filtro non vinta dal lato in vantaggio. Le prime 170
   posizioni così ottenute comprendevano 39 posizioni perse in entrambe le partite dal lato "in vantaggio": 37 da partita, dove il
   +3 di una partita veloce era sbagliato e `off` converte per l'altro lato. **Tolte su decisione dell'utente** (2026-10-31);
   restano i 2 matti persi due volte, dove il matto è forzato. Il set scende a 133, **sotto l'obiettivo di 150-300**.

| Classe | candidati | nel set |
|---|---:|---:|
| F: da partita, 6-10 pezzi | 57 | 19 |
| P: da partita, 11-32 pezzi | 404 | 101 |
| M: matti di `mates2000`, N ≥ 10 | 66 | 13 |

Esito delle 2 partite del filtro per il lato in vantaggio, sulle 133: due patte 120, una patta e una vittoria 9, una patta e una
sconfitta 2, due sconfitte 2 (matti). Il nome `endgames-outcome` è improprio: il 76 % del set ha 11-32 pezzi. Il primo filtro ha
girato per 24 minuti con due processi `ssh-add` esterni a ~98 % di CPU (terminati dall'utente): pesa sui tempi, non sulla regola.

### 6.2 Campione: `auto` contro `off`, 60+0.6, 500 coppie

`auto` con `ProverBudget=30`, `ProverLog=true`, trigger ristretto e memoria dei fallimenti contro `off`, libro in ordine (ogni
posizione in ~3,8 coppie), Hash 16, un thread, Syzygy 5-man, adjudication solo TB, concorrenza 4. Stima scritta prima del via:
~8,7 h; sul via dell'utente, 2026-10-31 dalle 08:07 alle 16:07 (8 h 00). Nessun timeout, nessun crash.

| Partite | Elo (logistic) | nElo | Pentanomiale [0..2] di `auto` | V/S/P | Terminazioni |
|---:|---|---|---|---|---|
| 1000 | **+0,69 ± 3,85** | +3,9 ± 21,5 | **[0, 15, 468, 17, 0]** | 56 / 54 / 890 | 583 TB, 417 normali (matto, ripetizione, 50 mosse) |

**Conversione del lato in vantaggio** (`outcome_analysis.py`; intervallo bootstrap al 95 % sulle posizioni, perché le coppie
sulla stessa posizione non sono indipendenti):

| Sottoinsieme | posizioni | coppie | V/P/S con `auto` | V/P/S con `off` | punti `auto` − `off` per coppia | vittorie `auto` − `off` |
|---|---:|---:|---|---|---:|---:|
| tutte | 133 | 500 | 40/446/14 | 40/444/16 | **+0,002 [−0,010; +0,014]** | **0,0 % [−2,2 %; +2,4 %]** |
| F | 19 | 70 | 4/66/0 | 4/66/0 | 0 (nessuna coppia non pari) | 0 |
| P | 101 | 382 | 24/353/5 | 25/350/7 | +0,001 [−0,013; +0,016] | −0,3 % [−2,9 %; +2,4 %] |
| M | 13 | 48 | 12/27/9 | 11/28/9 | +0,010 [−0,041; +0,067] | +2,1 % [−8,2 %; +13,3 %] |
| patta due volte nel filtro | 120 | 452 | 20/430/2 | 16/434/2 | +0,004 [−0,006; +0,015] | +0,9 % [−1,1 %; +3,0 %] |
| esiti diversi nel filtro | 11 | 41 | 20/16/5 | 24/10/7 | −0,024 [−0,115; +0,071] | −9,8 % [−25,0 %; +5,1 %] |

**Registro del prover** (`prover-log.md`, `analysis-raw.md`):

| | |
|---:|---:|
| partite con almeno un avvio | 111 su 1000 (coppie con il prover: 73 su 500; F 4/70, P 44/382, M 25/48) |
| avvii | 771 |
| esiti certificati | 298 in 56 partite (165 matti, 133 vittorie a foglie TB) |
| senza esito | 473 (261 budget, 212 non certificabili con l'orologio), 12,2 s in tutto, mediana 12 ms, max 87 ms |
| tempo senza esito per partita con avvio | 110 ms (~0,2 % dei ~57 s di riflessione di `auto`) |

**Conversione dopo il primo punteggio decisivo** (banda matto/TB): `auto` 56 vinte su 56, `off` 54 su 54.

**Coppie non pari**: 32 su 18 posizioni, 17 a favore di `auto`, 15 a favore di `off` (`conversion.md`, una riga per coppia con la
semimossa della prima prova e il punteggio della mossa precedente).

**Velocità.** nps per partita −0,06 % ± 0,63 % (mediana +0,08 %), aggregata 422 279 contro 422 514. Per tratto della partita
(tempo di mossa meno i ms del prover, `analyze.py`):

| Tratto | mosse di `auto` | partite | nps di ricerca `auto` / `off` − 1 |
|---|---:|---:|---:|
| prima del primo avvio (probe saltato) | 32 108 | 945 | +0,44 % ± 0,53 % |
| dopo un avvio, prima di una prova (probe attivo se l'avvio ha scritto entry PROVEN: non visibile nel PGN) | 422 | 34 | −0,23 % ± 7,47 % |
| dopo la prima prova certificata (probe attivo) | 0 | 0 | non misurabile |

### 6.3 Lettura (prudente)

1. **Il prover non cambia l'esito dove `off` non converte.** L'estremo alto dell'intervallo, +0,014 punti per coppia, equivale
   a una patta in più trasformata in vittoria in meno di 3 coppie su 100. Sui soli casi più netti (due patte nel filtro) 20 vittorie
   contro 16, intervallo che comprende zero.
2. **Il motivo è strutturale, non di campione.** Il prover alla radice parte solo se il punteggio della mossa precedente è già
   decisivo (`is_decisive(bestPreviousScore)`), e ogni partita che ha raggiunto un punteggio decisivo è stata vinta, con entrambi i
   motori: 110 casi su 110. Il primo punteggio decisivo arriva da Stockfish prima del prover, e dopo non c'è nulla da salvare.
   Le posizioni che `off` non converte sono quelle in cui la ricerca non arriva mai a quel punteggio, e lì il prover non parte:
   427 coppie su 500 senza una riga del prover.
3. **"Con una riga del prover" non vuol dire "per merito del prover".** Tutte le 32 coppie non pari hanno una riga del prover,
   ma è un effetto del trigger: una partita decisa fa partire il prover in chi vince o perde. Nelle 15 coppie a favore di `auto` con una
   prova, la mossa precedente era già a +199,xx o a matto. Il bilancio 17 contro 15 è compatibile con il rumore: nel filtro, `off`
   contro `off` sulle stesse posizioni, le coppie non pari erano 11 su 133 (8,3 %, gonfiato dalla selezione) contro il 6,4 % qui.
4. **Matti lunghi**: 3 coppie a favore di `auto` e 2 a favore di `off`. Nelle 3 a favore di `auto` la ricerca mostrava già il matto
   (+M7, +M9, +M13) alla mossa prima della prova: lo scarto nasce prima che il prover parta.
5. **La colonna "mossa provata != `Prover=off`" di `analysis-raw.md` non è informativa qui**: la ricerca di controllo riparte con la
   tabella vuota e un altro numero di nodi, e la differenza compare anche nelle partite perse (35 su 42 nella classe "altro").
6. **Costo del probe con entry PROVEN in partita: non risolto.** Dopo la prima prova ogni mossa di `auto` rilegge la prova ed esce
   subito (H5): zero mosse usabili, qui come nel campione di §4. Il tratto intermedio ha solo 422 mosse (1,3 %) e un intervallo di
   ±7,5 %, e non si sa quante abbiano la tabella con PROVEN. Sull'intera partita il costo non si vede (−0,06 % ± 0,63 %) perché il
   probe attivo copre pochissime mosse. Una misura vera richiederebbe posizioni fisse con la tabella riempita prima (non fatto).
7. **Limiti.** 133 posizioni sotto l'obiettivo, in maggioranza non finali; ogni posizione è filtrata da 2 sole partite, quindi una
   parte delle 133 è convertibile e ci è finita per rumore; il ciclo del libro dà ~3,8 coppie per posizione; nella classe F il prover
   parte in 4 coppie su 70, quindi lo zero di F dice poco sul prover. Il campione misura il trigger attuale e non dice nulla su un
   trigger che parta prima del punteggio decisivo, l'unico modo di lasciare spazio al prover (`README-tuning.md` §7: il trigger "pochi
   pezzi" a 60+0.6, non misurato). `Prover=off` resta il default e §5 non cambia.
