# src/stockfish-hybrid — M3 (spec §9.1, §12.1 punto 6): integrazione sincrona del prover in Stockfish 19

Copia di `Stockfish-sf_19/src` (più `tests/`) con gli hook H1-H8 di §8.5 e il nucleo df-pn di M2c. Il commit
`c8c0e01` è la copia identica; il diff rispetto a quel commit sono gli hook. `Stockfish-sf_19/` resta intatto
(riferimento). Il nucleo (`../wintree/dfpn.h`), l'adapter scacchi (`../wintree-chess/chess_game.h`) e il
verificatore (`../wintree-chess/verifier.h`) sono inclusi dai percorsi esistenti, non copiati (`-iquote .` nel
Makefile perché quei file includono `"position.h"` ecc. dalla cartella del motore).

```bash
cd src/stockfish-hybrid && make -j8 ARCH=x86-64-bmi2 all      # ~35 s; `make build` richiede ../scripts (net.sh) che la copia non ha
./stockfish bench                                             # Prover=off di default: signature 2497913 come Stockfish 19
printf 'setoption name Prover value mate\nposition fen r2r1n2/pp2bk2/2p1p2p/3q4/3PN1QP/2P3R1/P4PP1/5RK1 w - - 0 1\ngo depth 5\nquit\n' | ./stockfish
#   info string prover: proven mate in 4 move g4g7 nodes 425 time 0 ms
#   info depth 1 ... score mate 4 ... pv g4g7 f7e8 g7e7 e8e7 g3g7 e7e8 e4f6
#   bestmove g4g7 ponder f7e8
python3 tests/instrumented.py --none ./stockfish              # suite di Stockfish + TestProver (c)
PROVER=auto tests/reprosearch.sh                              # riproducibilità con go nodes (d)
tests/prover_overhead.sh <stockfish-19-di-riferimento> 10 2   # B3/G4: overhead del probe sul bench (b)
tests/smoke_games.sh <cartella> 200 4                          # 200 partite fastchess Prover=auto vs off (e)
```

## Opzioni UCI (H1, `engine.cpp`)

| Opzione | Tipo | Default | Significato |
|---|---|---|---|
| `Prover` | string `off`/`auto`/`mate`/`draw` (`false`/`true` = `off`/`auto`) | `off` | `off`: nessun probe, ricerca identica a Stockfish 19. `auto`: probe attivo, prover alla radice su trigger. `mate`: prover alla radice sempre in modalità WIN(us). `draw`: prover alla radice sempre in modalità WIN(opp), cerca una patta certificata (PROVEN_NOWIN(opp)). Un valore non valido stampa `info string Prover: invalid value ...` e spegne il prover |
| `ProverHash` | spin 1-65536 MiB | 16 | tabella delle prove (16 byte per entry, bucket di 4), potenza di due per difetto |
| `ProverBudget` | spin 0-1000 ‰ | 100 | quota di `tm.optimum()` (con orologio) o di `movetime` per il prover alla radice; 0 = mai con un orologio |
| `ProverMaxNodes` | spin 0-10^9 | 1 000 000 | tetto in `do_move` del prover (il "nodo" di §9.4; ~35 per espansione df-pn); 0 = prover alla radice spento, probe attivo |
| `ProverProbeDepth` | spin 1-100 | 6 | probe della tabella solo con `depth >= ProverProbeDepth` (gating come `SyzygyProbeDepth`) |
| `ProverLog` | check | false | G3 tuning: a ogni avvio del prover alla radice `info string prover: start <win|nowin> budget [<ms> ms] <ProverMaxNodes> do_move` e, senza esito certificato, `no result after <do_move> do_move in <ms> ms (<budget|not certified with a clock|not verified>)` (in ogni modalità) |

La spec prevedeva `Prover` check + `ProverMode` combo: qui una sola opzione string, perché il tipo combo di
Stockfish 19 non è utilizzabile (la validazione in `ucioption.cpp:160-169` rifiuta i token `var` ripetuti: `OptionsMap::add`
termina il processo sul duplicato).

## Hook (file:righe della copia modificata; il diff è `git diff c8c0e01 -- src/stockfish-hybrid`)

| Hook | Dove | Cosa |
|---|---|---|
| H1 opzioni | `engine.cpp:139-175` (dopo `EvalFile`) | le 5 opzioni sopra; i valori finiscono nei globali di `Prover::` (come `Tablebases::init`), letti dalla ricerca senza lookup della mappa |
| H2 radice | `search.cpp:215` → `Search::Worker::prove_root()` in `prover.cpp` | fra il controllo `rootMoves.empty()` e `threads.start_searching()`, sul main thread con gli helper fermi (mai in `start_thinking`). Trigger (solo `auto`): `go mate` o `bestPreviousScore` decisivo (G3 tuning: il trigger "pochi pezzi" di M3 è stato tolto, `../../tests/g3/README-tuning.md`); `mate`/`draw` forzano. Memoria dei fallimenti (solo `auto` senza `go mate`): una radice uscita senza esito (`UNKNOWN_BUDGET`) non riparte, per chiave, finché `ProverMaxNodes` e la quota di tempo non sono almeno il doppio di do_move e ms spesi; azzerata da `ucinewgame`. Direzione: WIN(us), oppure WIN(opp) (modalità NOWIN, tetto 4096) se `draw` o se il punteggio precedente è negativo. Budget `min(ProverBudget‰ · optimum, ProverMaxNodes)` con la quota misurata da `limits.startTime`, stop UCI cooperativo. Esito PROVEN_WIN/NOWIN/UNKNOWN_CYCLE → verificatore (§7.9); se certificato: `rootProof` (H4/H5), `tbRank` = 1 sulla mossa provata in tutti i worker + `stable_sort` come `tbprobe.cpp:1949`, `rootInTB` resta false |
| H3 probe | `search.cpp:949-973` (dopo lo Step 6, prima dello Step 7) | `!rootNode && !excludedMove && Prover::mode != OFF && depth >= ProverProbeDepth`; `Prover::probe()` legge l'entry base per ciascun goal, controlla `rev/req` sul contatore (§7.5) e mappa con la tabella di §7.8: PROVEN_WIN(g,d) → `mate_in(ply+d)`/LOWER per g al tratto, `mated_in`/UPPER per l'altro, banda TB (`VALUE_TB − (ply+d)`) se `tbLeaf`; PROVEN_NOWIN(g) → `VALUE_DRAW` UPPER/LOWER. Stesso schema di `search.cpp:955-970`: scrittura TT (`depth+6`) e ritorno se il bound chiude la finestra, altrimenti `bestValue/alpha` o `maxValue` ai nodi PV |
| H4 scelta | `search.cpp:249-262` (dopo la scelta di `bestThread`, prima di `bestPreviousScore`) | se `rootProof.proven`: `Utility::move_to_front`, `score = uciScore = mate_in(d)` (o `VALUE_TB − d` con foglie TB), `pv` = cammino principale dell'albero di prova, `unset_inexact()`, `uciPvSent = false`; con `searchmoves` che escludono la mossa provata non si applica |
| H5 stop | `search.cpp:571-579` (dopo il trigger `limits.mate`) | dopo un'iterazione completata, con `rootProof.proven` e (`!limits.mate` o `d ≤ 2·mate`): `threads.stop` (o `stopOnPonderhit` se si pondera); mai con `go infinite` |
| H6 contabilità | `prover.cpp` (`nodes += g.moves`) | le `do_move` del prover sommate a `Worker::nodes` del main (per `go nodes`/`nodestime`); `callsCnt = 0` come dopo una sonda TB |
| H7 clear | `engine.cpp:198` (`search_clear`) | `Prover::clear()` dopo `threads.clear()` (`ucinewgame`, `Clear Hash`) |
| H8 output | `prover.cpp` (`sync_cout`) | solo esiti certificati: `info string prover: proven mate in N move <uci> nodes <do_move> time <ms> ms`; `proven win (tablebase leaves) move ...`; `proven mated in N`; `proven loss (tablebase leaves)`; `proven draw-or-better` (NOWIN(opp) o strategia di patta da UNKNOWN_CYCLE certificata); `proven draw-or-worse` (NOWIN(us)). Mai "mate N" per prove con foglie TB (il valore resta nella banda TB e passa da `syzygy_extend_pv`) |
| Makefile | `Makefile:84,94,501-502` | `prover.cpp`/`prover.h` nelle liste; `-iquote .` in `CXXFLAGS` e `DEPENDFLAGS` |
| `search.h` | `search.h:39,315,362` | `#include "prover.h"`, `SearchManager::rootProof`, `Worker::prove_root()` |

File nuovi: `prover.h` (stato, `RootProof`, `probe`), `prover.cpp` (tabella, mappatura §7.8, `prove_root`, cammino
principale), `tests/prover_overhead.sh`, `tests/random_openings.py`, `tests/smoke_games.sh`; modificati:
`tests/instrumented.py` (classe `TestProver`, percorso della rete), `tests/reprosearch.sh` (variabile `PROVER`). Nel
nucleo condiviso (`../wintree/wintree.h`, `../wintree/dfpn.h`): bit 55 `tbLeaf` nelle entry PROVEN (`Result::tbLeaf`,
`pack_proven(..., tbLeaf)`), propagato dalle foglie TB (`WIN_GOAL_C0`) lungo l'albero di prova, e `CHECK_EVERY` = 64.

## Scostamenti dalla spec e scelte di M3

1. **`Prover` string al posto di check + `ProverMode` combo** (sopra): il combo di Stockfish 19 è inutilizzabile.
2. **Tabella delle prove globale** (`Prover::table`, `prover.cpp`) come `Tablebases`, non membro di `Engine`/`SharedState`:
   un'istanza di `Engine` per processo (lo stesso limite di `Tablebases::init`, `engine.cpp:166`). Allocata con `new`
   (non `aligned_large_pages_alloc`), azzerata in `search_clear`. Le entry PROVEN non hanno età/generazione (§8.3):
   rimpiazzo di M2c (bucket di 4, protezione morbida dei PROVEN).
3. **`ProverMaxNodes` in `do_move`** (il nodo di §9.4, ~35 per espansione df-pn), non in espansioni: è la misura usata da
   G0/G1 e sommata a `Worker::nodes` (H6). Default 10^6: nella suite mates2000 il nucleo a freddo risolve 1506/2000
   posizioni entro 10^6 `do_move` (934 entro 10^5, 793 entro 6·10^4 ≈ il budget di 30 ms di una partita a 10+0.1).
4. **Budget alla radice**: `ProverBudget‰ · tm.optimum()` con orologio, la stessa quota di `movetime` senza orologio,
   nessuna scadenza con `go depth/nodes/infinite` (solo `ProverMaxNodes`: riproducibile); niente `k` adattivo (§8.4),
   niente seconda ricerca WIN(opp) dopo WIN(us) nella stessa mossa (§6.2: una direzione per mossa, scelta dal segno del
   punteggio precedente), niente WT di certificazione a fine ricerca (§6.2 punto 4, variante A). **Pressione di tempo**
   (emersa nella prima corsa di (e): una perdita per il tempo con 104 ms di sforamento, e mosse con ricerca a
   profondità 2 perché il prover aveva consumato il tempo): (i) con orologio il prover non parte se la quota è < 10 ms;
   (ii) con orologio si certifica (verificatore, output) solo PROVEN_WIN(us), l'esito che cambia la mossa: il replay
   del verificatore non ha budget e si ripeterebbe a ogni mossa dentro un albero di prova dell'avversario; gli altri
   esiti restano bound search-grade tramite i probe; (iii) tetto di profondità 4096 solo con `Prover=draw` (altrimenti
   245: niente buffer da 3,6 MB per mossa); (iv) nel nucleo `DfPn::CHECK_EVERY` passa da 1024 a 64 espansioni
   (scadenza e `stop` controllati ogni ~1 ms invece di ~20 ms; costo di `Clock::now()` trascurabile).
5. **Trigger "pochi pezzi"** (M3: `pezzi ≤ min(SyzygyProbeLimit, MaxCardinality) + 2`): **tolto nel tuning di G3** (partiva a
   ogni mossa dei finali senza esito); in `auto` restano `go mate` e il punteggio precedente decisivo. Con `rootInTB` il prover non parte (la radice è già decisa dalle TB). I sink TB del prover richiedono
   `Syzygy50MoveRule = true` (§7.6: WDL +1 non è una vittoria); con la regola spenta il prover lavora senza sink TB.
6. **`d` è un limite superiore della DTM** (I6, nota 4 di M2c): sul bench 1 (matto in 5) il prover certifica un matto
   in 11 con la stessa mossa `h4f4`; l'output dice `score mate 11`. Nessuna ri-ricerca con tetto decrescente per
   minimizzare `d`; con `go mate N` e `d > 2N` la ricerca continua (H5 non ferma) finché Stockfish trova un matto ≤ N,
   e a fine ricerca H4 impone comunque la mossa provata.
7. **Probe H3 sulla sola entry base** (niente entry ombra `key ^ SHADOW` delle riprove a contatore diverso) e per
   entrambi i goal (due scansioni dello stesso bucket); nessun prefetch dedicato. Il valore di matto è nella banda
   mate solo se `ply + d ≤ 245`, altrimenti banda TB (clamp a `MAX_PLY`).
8. **Scelta della mossa con tutte le mosse perdenti** (§6.2 punto 5, `d` massimo) non implementata: con una
   PROVEN_WIN(opp) certificata si stampa `proven mated in N` e la ricerca sceglie come oggi (i probe danno i bound
   `mated_in` sui figli).
9. **Nucleo condiviso** (`../wintree/wintree.h`, `../wintree/dfpn.h`): bit 55 `tbLeaf` nelle entry PROVEN, propagato in
   `dfpn.h` (il depth-first di `wintree.h` non lo propaga: resta 0); `CHECK_EVERY` 64. `make -C src/wintree test` verde
   e `src/wintree-chess/main.cpp` compila (`-fsyntax-only`) dopo entrambe le modifiche.
10. **Build**: `make build` fallisce nella copia perché `net`/`ARCH=native` cercano `../scripts/`; si usa
    `make ARCH=x86-64-bmi2 all` (stessa riga di comando per il riferimento). Nessun PGO: riferimento e ibrido sono
    confrontati con la stessa build non profilata.
11. **Thread**: il prover gira solo sul main thread fra `rootMoves.empty()` e `threads.start_searching()`; la tabella è
    scritta solo lì e letta (load relaxed) da tutti i thread durante la ricerca. H9 (thread prover) è M5.

## Risultati (2026-10-17, g++ 16.2, `ARCH=x86-64-bmi2`, build `all` senza PGO, 4 core + HT)

### (a) Signature del `bench` con `Prover=off`

Riferimento: copia temporanea di `Stockfish-sf_19/src` compilata con la stessa riga di comando (non dentro
`Stockfish-sf_19/src`). `./stockfish bench` → `Nodes searched : 2497913` per il riferimento, per l'ibrido con
`Prover=off` (default) e anche per l'ibrido con il probe attivo e il prover alla radice spento (`Prover=auto`,
`ProverMaxNodes=0`: tabella vuota, nessun hit, stesso albero). Con `Prover=auto` e i default il bench non è più
identico per costruzione (il prover scatta sulle posizioni di matto e su quelle a ≤ 7 pezzi): non è un requisito.

### (b) B3/G4: overhead del probe sul `bench` (`tests/prover_overhead.sh`, 10 corse alternate per configurazione, `taskset -c 2`)

`probeN` = `Prover=auto`, `ProverMaxNodes=0` (nessun prover alla radice: solo il probe H3 con `ProverProbeDepth=N` su una
tabella vuota, cioè il costo puro del lookup in memoria e del branch).

| Configurazione | Signature | nps mediana (10 corse) | nps max | mediana / riferimento |
|---|---|---:|---:|---:|
| ref (Stockfish 19) | 2497913 | 737500 | 753745 | 100.00% |
| off (ibrido, `Prover=off`) | 2497913 | 738266 | 749448 | 100.10% |
| probe1 | 2497913 | 719030 | 725294 | 97.50% |
| probe4 | 2497913 | 727830 | 737717 | 98.69% |
| probe6 | 2497913 | 735762 | 741660 | 99.76% |
| probe8 | 2497913 | 735438 | 742321 | 99.72% |

G4 (overhead ≤ 1 % di nps): **superato con `ProverProbeDepth ≥ 6`** (−0,24 %; il default era 4 nella prima stesura e
costava 1,3 %, oltre la soglia: il rimedio previsto da §11, "ridurre il gating", è applicato: default 6). A depth 1
il probe costa 2,5 %: un accesso casuale in memoria per nodo di `search()`, come atteso; un prefetch dell'entry del
figlio in `Worker::do_move` (§8.6) è la via per abbassare il gating, non misurata.

### (c) `tests/instrumented.py` (`python3 tests/instrumented.py --none ./stockfish`, ~15 s)

Suite di Stockfish 19 (CLI, interattiva, Syzygy 3-4 pezzi, en passant, FEN non valide, opzioni non valide, file di
bench) più la classe `TestProver` (11 test): le 4 posizioni di matto/stallo del bench (`benchmark.cpp:91-94`) e 14
posizioni di `mates2000` (id 1, 7, 19, 51, 120, 249, 660, 1144, 1391, 1579, 1691, 1773, 1920, 1983: una per ogni N da 1
a 12, poi 15 e 20, scelte fra quelle che il nucleo a freddo risolve entro 3·10^5 `do_move` nel CSV di M2c) con
`Prover=mate` e `go depth 5`: deve comparire `info string prover: proven mate in N move M`, la `bestmove` deve essere
`M`, l'ultimo `score mate` deve valere `N` e `N` deve essere ≥ al matto dichiarato (I6). Inoltre: stallo e matto
subito (`bestmove (none)`, nessun prover), `go mate 4` che si ferma alla prova (depth 1), `Prover=off` senza output,
`Prover=auto` che non scatta sulla posizione iniziale e scatta con `go mate`, `Prover=draw` su una posizione morta
(`proven draw-or-better`), valore non valido dell'opzione. **Esito: 86/86 test verdi, 8 suite** (TestProver 11/11;
il campione mates2000 in 0,43 s). `tests/syzygy/` contiene link simbolici alle 3-4 pezzi di `~/syzygy/5man` (la suite
originale le scarica da rete); `test_network_equals_base` cerca la rete in `tests/../src/`: nella copia il percorso
è `tests/..`, aggiunto come seconda opzione.

### (d) `tests/reprosearch.sh` (`PROVER=auto tests/reprosearch.sh`, ~2 min)

Verde con `Prover=auto` e con il default (`off`): 20 valori di `go nodes` da 150 a 332525, ogni conteggio di nodi
compare un numero pari di volte. Con `auto` sulla posizione iniziale nessun trigger scatta: il test copre il probe
H3 (tabella vuota) e la stabilità del ciclo con le opzioni attive; con un trigger attivo la riproducibilità vale solo
senza orologio (budget in `do_move`, nessuna scadenza), come per `go nodes`.

### (e) Partite di fumo (`tests/smoke_games.sh`, fastchess 1.8.2, 10+0.1, Hash 16, 1 thread, Syzygy 5-man, 4 partite in parallelo, 100 aperture casuali a 8 ply da `random_openings.py` seme 1, colori invertiti)

Solo per escludere crash, timeout e mosse illegali: non è G3 e il risultato non dice nulla sulla forza.

**Prima corsa (binario prima delle correzioni di tempo, interrotta a 116 partite)**: nessun crash, nessuna mossa
illegale, nessuna disconnessione; **1 perdita per il tempo** di `prover-auto` (nero, partita 20, sforamento di 104 ms
all'ultima mossa, 377 ms spesi con ~270 ms sull'orologio) e, nelle stesse partite, mosse con ricerca ferma a
profondità 2-8 in 25-50 ms: il prover alla radice consumava più della quota (scadenza controllata ogni 1024 espansioni
≈ 20 ms, verifica dell'albero di prova dell'avversario ripetuta a ogni mossa senza budget, buffer da 3,6 MB in
modalità NOWIN). Esiti: 51 vittorie del bianco, 15 del nero, 49 patte (26 ripetizione, 13 materiale, 7 50 mosse,
3 stallo), 1 tempo. Correzioni: punto 4 degli scostamenti.

**Seconda corsa (binario con le correzioni, 200 partite, 55 min)**: **nessun crash, nessuna mossa illegale, nessuna
disconnessione, nessuna perdita per il tempo di `prover-auto`**; 199 terminazioni normali e 1 perdita per il tempo di
`prover-off` (bianco, partita 62, sforamento di 9 ms: Stockfish 19 invariato con `Move Overhead` 10 ms e 4 partite su 4
core fisici, cioè rumore dell'ambiente, non il prover). Esiti: 89 vittorie del bianco, 31 del nero, 79 patte (62
ripetizione, 12 materiale, 3 stallo, 2 50 mosse), 1 tempo; `prover-auto` − `prover-off`: +62 −59 =79, Elo +5,2 ± 16,3,
pentanomiale [0, 9, 80, 10, 1] (nessun significato statistico, come previsto per 200 partite). Uso del tempo per
motore (dal PGN con `timeleft`): tempo massimo per mossa 2,337 s contro 2,339 s, residuo minimo 0,119 s contro 0,127 s;
mosse con profondità ≤ 4: 220 contro 179, di cui 153 contro 129 sono matti trovati a profondità 1 (per `prover-auto`
anche gli arresti H5 su matti certificati). Riepilogo in `../../tests/results/m3-smoke.out`.

## Riepilogo dei gate di M3

| Verifica | Esito |
|---|---|
| (a) signature `bench` con `Prover=off` | **identica**: 2497913 (anche con il probe attivo e tabella vuota) |
| (b) G4 overhead del probe | **−0,24 %** con `ProverProbeDepth=6` (default); −1,3 % a 4, −2,5 % a 1 |
| (c) `tests/instrumented.py` | **86/86**, `TestProver` 11/11 |
| (d) `tests/reprosearch.sh` con `Prover=auto` | **verde** (anche con `off`) |
| (e) 200 partite 10+0.1 `auto` vs `off` | **nessun crash/timeout/illegale di `auto`**; 1 forfeit di 9 ms di `off` (ambiente) |

Non fatto in M3 (fuori mandato o rinviato): G3 (SPRT), G2 (corpus di fortezze), misura di precisione/richiamo dei trigger
(§12.2), prefetch dell'entry della tabella, ri-ricerca per minimizzare `d`, scelta "resistenza massima" con tutte le
mosse perdenti, `k` adattivo, PGO per riferimento e ibrido, H9.


