# src/wintree-chess — M2 (spec §9.1): adapter scacchi, verificatore, G0 scacchistico, ordinamento a caldo, baseline df-pn, gate G1; M2c: nucleo df-pn

Binario `wintree-chess` = nucleo df-pn di produzione (`../wintree/dfpn.h`, default da M2c; `--core wintree` per il depth-first di `../wintree/wintree.h`, `--core base` per la baseline di M2b) + adapter scacchi (§8.2) + verificatore (§7.9) + CLI + regressioni + test G0 + benchmark di §9.4, linkato con i sorgenti di **Stockfish 19 non modificati** (`Stockfish-sf_19/src`, tutti tranne `main.cpp`: `tt.cpp` trascina `ThreadPool` e `position.cpp` `UCIEngine`, quindi quasi tutto il motore, come previsto in §9.1). Stesse opzioni di compilazione del Makefile di Stockfish per `ARCH=native` (lette da `make config-sanity`), senza LTO e con `-DNNUE_EMBEDDING_OFF`: il prover non usa la rete; la ricerca di Stockfish in-process (ordinamento a caldo, `go mate`) la carica dal file `Stockfish-sf_19/src/nn-1a298aa575a0.nnue` (`NNUE=...` nel Makefile, `--nnue` nella CLI). Build da pulito: ~1 min di CPU, ~15 s con `-j8`; binario 1,3 MB.

```bash
make -C src/wintree-chess -j8            # build/wintree-chess
make -C src/wintree-chess test           # regressioni (con TB 5-man) + G0 su 10^3 posizioni con il nucleo df-pn: ~30 s su 8 core   [CORE=dfpn|wintree|base]
make -C src/wintree-chess g0full         # G0 su 10^4 posizioni (8 thread): ~5 min con df-pn, ~30 min con WinTree   [TB=... G0=... SEED=... NODES=... MOVES=...]
make -C src/wintree-chess bench-g1       # G1 rapido: B6 su 100 posizioni di mates2000 (una ogni 20): df-pn freddo/caldo, WinTree caldo, baseline calda, Stockfish; ~10 min   [TIME=10 THREADS=4]
make -C src/wintree-chess bench-g1-full  # G1 di M2c in background: B6 su 2000 posizioni (df-pn ± euristica, baseline, Stockfish) poi ε su 200; ~1-2 h; log in tests/results/bench-g1-m2c.log
src/wintree-chess/build/wintree-chess --fen "r2r1n2/pp2bk2/2p1p2p/3q4/3PN1QP/2P3R1/P4PP1/5RK1 w - - 0 1" --verify
#   esito PROVEN_WIN d=7 rev=3 mossa=g4g7 / nodi 19 do_move 245 iterazioni 1 profondità 245 tempo 0.0004 s / verifica certificato: nodi 8 foglie 1 TB 0 profondità 7   (do_move 677 prima di key_after, 2026-10-25)
src/wintree-chess/build/wintree-chess --fen "6k1/3b3r/1p1p4/p1n2p2/1PPNpP1q/P3Q1p1/1R1RB1P1/5K2 b - - 0 1" --min-d --verify                 # d minimo: prima prova d=21 (nodi 6568 do_move 74104), 4 ri-ricerche: d=9 (+nodi 12350 +do_move 98187)
src/wintree-chess/build/wintree-chess --fen "2rkr3/2p1p3/8/1B6/8/8/8/RK6 w A - 0 1" --chess960 --verify                                     # Chess960: PROVEN_WIN d=1 mossa=b1a1 (0-0-0, torre in d1)
src/wintree-chess/build/wintree-chess --fen "..." --noinit --verify                                                        # df-pn senza euristica di pn/dn: nodi 105 do_move 6297 (= baseline)
src/wintree-chess/build/wintree-chess --fen "..." --core wintree --verify                                                  # depth-first: nodi 112365 do_move 425921 iterazioni 4 profondità 7
src/wintree-chess/build/wintree-chess --fen "..." --order warm --nnue Stockfish-sf_19/src/nn-1a298aa575a0.nnue --verify   # 100 ms di Stockfish, poi il prover con ttMove e storie come spareggio
src/wintree-chess/build/wintree-chess --fen "<fen>" [--moves m1 m2 ...] [--goal white|black] [--mode win|nowin] [--nodes N] [--domove N] [--depth D] [--time S]
                                      [--core dfpn|wintree|base] [--eps E] [--noinit] [--order cold|warm|random --nnue PATH --warm-ms MS] [--tb PATH] [--tbmax K] [--hash MiB] [--verify]
src/wintree-chess/build/wintree-chess --g2sample 20000 --seed 1 --warm-ms 1000 --nnue PATH --tb ~/syzygy/5man:~/syzygy/6man   # G2 corpus (a): patte a 6 pezzi che Stockfish senza TB valuta > 150 cp (tests/g2/README.md)
src/wintree-chess/build/wintree-chess --g2eval FILE.epd --warm-ms 1000 --nnue PATH --tb ...                                      # G2: aggiunge c0 "goal= eval= wdl=" a ogni riga EPD
src/wintree-chess/build/wintree-chess --suite tests/suites/mates2000.epd [--step K] [--count N] [--solvers dfpn,dfpn-warm,dfpn-noinit,dfpn-warm-eps50,base,base-warm,cold,warm,sf] [--time S]
                                      [--warm-ms MS] [--threads T] --nnue PATH [--csv FILE] [--hash MiB] [--dfpn-rep loss|unknown]
src/wintree-chess/build/wintree-chess --b2 tests/suites/mates2000.epd [--count N] [--depth D] [--csv FILE]
```

| File | Contenuto |
|---|---|
| `chess_game.h` | `classify()` (terminali di §7.4 su una posizione reale, condivisa da prover e verificatore) e `ChessGame`: `Position` con uno `StateInfo` per ply agganciato alla storia della partita (FEN + mosse, come `thread.cpp:345`), chiave `st->key`, lista legale generata una volta (`terminal()`) e riusata (`gen_children()`), ordinamento freddo / a caldo / casuale di §8.4 con la classe di ogni mossa, `init()` (pn/dn iniziali per df-pn, §8.1b), `reversible()`/`rule50()` per §7.5, contatore delle `do_move` |
| `verifier.h` | `WinTreeVerifier` (albero vincente, §7.9.1, con la profondità dell'albero verificato) e `verify_draw_strategy` (grafo di strategia di patta, §7.9.2); leggono base e entry ombra della tabella (`probe_for`) |
| `warm.h` | `SfEngine`: istanza di `Engine` (engine.h) senza output, ricerca breve per l'ordinamento a caldo (TT e storie del main worker in lettura) e `go mate N movetime T` per il confronto di B6 |
| `dfpn_baseline.h` | `DfPnBaseline<G>`: la baseline df-pn "da manuale" di M2b (Nagai 2002, 1+ε, tabella propria a 32 bit), conservata come controllo minimo di §11 (`--core base`, variante `base`) |
| `solve.h` | `Game` (posizione + storia), `Cfg` con `Core`, `solve_dfpn()` (nucleo di produzione), `solve()` (WinTree con iterative deepening), `solve_base()`, `solve_any()`, `verify()`, statistiche per nodo |
| `bench.h` | parser EPD, varianti di solver, runner della suite (B6/B4/B5), CSV, tabelle markdown, B2 |
| `main.cpp` | CLI, `--regress`, `--g0`, `--suite`, `--b2` |
| `../../tests/suites/` | `mates2000.epd` (vondele/matetrack) e `fetch-mates2000.sh` che lo rigenera (sha256 verificato) |
| `../../tests/results/` | CSV dei benchmark: M2b `g1-quick.csv`, `g1-full.csv`, `b4-order.csv`, `b5-ablation.csv`, `b2.csv`; M2c `g1-full-m2c.csv`, `g1-eps.csv` |

## Semantica implementata

- **Nucleo** (`--core`, M2c): `dfpn` (default) è `DfPn<ChessGame>` di `../wintree/dfpn.h` (MID di Nagai con 1+ε, tabella unica con (pn, dn), taint di ciclo, `rev/req`, `d`; descrizione e test sintetici in `../wintree/README.md`, "M2c"); `wintree` è il depth-first di M1-M2b; `base` la baseline di M2b (`dfpn_baseline.h`). Con df-pn la modalità fissa solo il tetto di profondità (245 `win`, 4096 `nowin`) e una ricerca sola prova o confuta; il budget è in espansioni (`--nodes`) **e** in `do_move` (`--domove`, il nodo di §9.4: il primo che si esaurisce).
- **Inizializzazione di pn/dn** (`ChessGame::init`, §8.1b; `--noinit` la spegne): per un figlio senza entry, con la posizione raggiunta, regola di Allis: nodo AND (difensore al tratto) pn = numero di risposte legali (ogni risposta va provata), dn = 1; nodo OR (attaccante) pn = 1, dn = numero di mosse legali (ogni mossa va confutata); se la mossa dell'attaccante che ha generato il nodo AND è uno scacco, una cattura con SEE ≥ 0 o la ttMove della ricerca a caldo, pn dimezzato. Dal 2026-10-25 `init` riceve la mobilità dal nucleo (`mobility()` sulla posizione del figlio, con cache nel nucleo): una `do_move` + `MoveList<LEGAL>` solo per un figlio senza entry la cui mobilità non è in cache; gli altri figli sono letti con `key_after` (chiave esatta senza `do_move`, verificata con `WT_ASSERT` a ogni `do_move` fatta). A parità di pn/dn vince l'ordine dei figli (classi del freddo, a caldo ttMove e storie entro la classe): l'ordinamento a caldo è lo spareggio previsto da §8.1b.
- **Terminali** (`classify`, in quest'ordine): nessuna mossa legale → matto (vince chi non è al tratto) o stallo; `rule50 > 99` (il matto al 100° ply prevale, come `Position::is_draw`); `st->repetition < 0` (triplice, storia compresa, §7.3); posizione morta (FIDE 5.2.2, solo i casi dimostrabili: KN-K, oppure soli re e alfieri tutti sullo stesso colore, cioè K-K, KB-K, KB-KB stesso colore, KBB-K stesso colore…; non KN-KN, KB-KN, KNN-K né alfieri di colore opposto, che ammettono un matto aiutato: argomento in `chess_game.h`, test in `--regress` contro le WDL); TB solo con `rule50 == 0`, senza diritti d'arrocco, pezzi ≤ `--tbmax` (default `MaxCardinality`): WDL = +2 per il goal ⇒ vittoria valida solo con contatore 0 (`rev = 100`), WDL ≤ +1 ⇒ non vittoria valida per ogni contatore (§7.6, cursed win inclusa). `ProbeState::FAIL` ⇒ non terminale.
- **50 mosse** (§7.5, nel nucleo): `rev/req` per entry, controllo `c + rev <= 100` / `c >= req` a ogni lettura della tabella; esempio: KQ-K con contatore 98 dà `PROVEN_NOWIN req=98` (nessun matto in 2 ply), con contatore 0 `PROVEN_WIN d=15`.
- **Ordinamento** (`--order`, §8.4). Classi, uguali nei nodi OR e AND: scacchi, catture con `see_ge >= 0`, promozioni, quiete, catture perdenti. *Freddo*: stabile entro la classe (ordine di `generate<LEGAL>`). *A caldo*: prima del prover una ricerca di Stockfish in-process sulla stessa posizione (`SfEngine`, un thread, Hash 16 MiB, `movetime` = `--warm-ms`, default 100 ms, tabella e storie azzerate prima); poi, **entro la classe**, prima la ttMove (`tt.probe(pos.key())`: chiave con il bucket delle 50 mosse, diversa da `st->key`; usata solo se legale), poi `captureHistory + 7·PieceValue` per le catture e `mainHistory` per le altre mosse (storie del main worker, `Worker::mainHistory`/`captureHistory`, pubbliche, lette a ricerca conclusa). La ttMove in testa a tutto e l'ordine AND letterale di §8.4 sono stati misurati peggiori del freddo: vedi "Ordinamento a caldo: scelta" sotto. *Casuale* (ablazione B4): hash di (chiave, mossa). L'ordine non cambia gli esiti PROVEN, cambia nodi e `d`.
- **`d` minimo** (`--min-d`, df-pn, 2026-10-25): dopo una PROVEN_WIN con profondità `d`, ri-ricerche sulla stessa tabella con tetto decrescente (`d − 2`, poi dicotomia fra l'ultimo tetto fallito e l'ultimo `d` trovato) entro il budget residuo; con `DfPn::capD` una PROVEN_WIN in tabella vale solo se la sua prova sta entro il tetto (`ply + d ≤ tetto`), altrimenti il nodo è ri-cercato nell'entry ombra e una prova più corta aggiorna la base; un tetto fallisce con `UNKNOWN_CYCLE` (tetto ⇒ confutazione tainted con residuo, `../wintree/README.md` M2c punto 5). La CLI stampa `d` e costo della prima prova e delle ri-ricerche; `bench.h` ha la variante `dfpn-mind` e la colonna `d0` nel CSV. Nell'ibrido solo con `go mate N` (`src/stockfish-hybrid/README.md`, scostamento 6).
- **Chess960** (`--chess960`, 2026-10-25): `Position::set(fen, true)` (arrocco con torri in posizione qualsiasi, lettere dei diritti nel FEN), `UCI_Chess960` nel motore in-process (ordinamento a caldo), notazione 960 delle mosse (`b1a1` = re prende la torre); `key_after` tratta l'arrocco con le case di destinazione di re e torre; il verificatore del grafo di patta ricostruisce le posizioni dal FEN con il flag (`verify_draw_strategy(…, chess960)`, parametro obbligatorio). Test in `--regress`: `key_after` sulle FEN 960 del bench di Stockfish e di `tests/perft.sh`, perft 960, matto in 1 per arrocco certificato.
- **Modalità** (`--mode`): `win` = iterative deepening sulla profondità (1, 3, 5, … fino a 245 da una radice OR; 2, 4, 6, … da una radice AND, dove il matto cade a ply pari, §8.4) con budget di nodi e tempo (`--time`) complessivi e, nel nucleo, arresto al primo figlio NIL nei nodi AND; `nowin` = una ricerca al tetto (`--depth`, default 4096) che esplora tutti i figli (Sez. 3). Vedi `../wintree/README.md` ("Aggiunte di M2").
- **Baseline df-pn** (`--core base`, `dfpn_baseline.h`; §11 la vuole "da manuale", non lo stato dell'arte): MID di Nagai (2002) con soglie proof/disproof, selezione del figlio a pn (OR) o dn (AND) minimo, soglia del figlio `min(th, max(second + 1, second·(1+ε)))` con ε = 0,25 (`--eps`), somme saturate a ∞ = 2^30; tabella propria di coppie (pn, dn) a 16 byte per entry (stessi MiB della ProofTable: `--hash`, default 64 MiB), rimpiazzo "l'ultimo scrive" (proteggere i risolti come nella ProofTable blocca la scrittura dei figli irrisolti che condividono lo slot e il ciclo MID del padre li risceglie senza progresso: misurato prima della correzione, 84·10^6 `do_move` senza esito sulla posizione 61 contro 1,6·10^5 dopo); stesso adapter, stessi terminali (WIN_GOAL ⇒ pn = 0, ogni altro terminale ⇒ dn = 0) e stesso ordinamento dei figli di WinTree; tetto di profondità 245 (oltre: confutazione, come il limite di profondità dei risolutori di tsume); un figlio che ripete una posizione del cammino = sconfitta dell'attaccante (pn = ∞, dn = 0, Nagai; vedi scostamento 8); nessun trattamento GHI. I nodi risolti e la mossa (di prova nei nodi OR, di confutazione nei nodi AND) finiscono anche nella `ProofTable`, così `--verify` e i benchmark controllano gli esiti PROVEN con lo stesso verificatore. Nodo = una chiamata di `mid()`; le `do_move` le conta l'adapter, comprese quelle della lettura dei figli a ogni giro del ciclo MID (in WinTree una volta per espansione, ciclo O1).
- **Stockfish in-process** (`warm.h`): `Engine` con tutti i callback a vuoto (`verify_network` stampa solo gli errori), rete caricata con l'opzione `EvalFile` (percorso assoluto), `Threads` 1; `tt` e `threads` sono privati e si leggono con un'istanziazione esplicita di template su puntatori a membro ([temp.explicit]/12), senza modificare `engine.h`; tra una ricerca e l'altra `tt.clear` + `threads.clear` (non `search_clear`, che reinizializza le Syzygy con il `SyzygyPath` del motore e spegnerebbe quelle del prover).
- **Verificatore** (`--verify`, e su ogni PROVEN dei test). *Albero vincente*: replay dalla radice con `do_move` sugli `StateInfo` reali (contatore e storia esatti, niente `rev`); ai nodi del goal la mossa memorizzata nell'entry, ai nodi avversari tutte le mosse di `MoveList<LEGAL>`; foglie ammesse: matti dati dal goal, TB WDLWin con contatore 0; memo sulle coppie (posizione, contatore) già verificate: un sottoalbero verificato a un dato contatore vale per ogni cammino (una ripetizione lungo un cammino fa solo crescere il contatore fino alla patta per 50 mosse, che il replay rileva; una triplice che dipende solo dalla storia non dipende dal cammino). *Grafo di strategia di patta* (per `PROVEN_NOWIN` e per una radice `UNKNOWN_CYCLE`, completa per I2): chiusura per visita in ampiezza su posizioni identificate dal FEN senza contatori; dai nodi del goal tutte le mosse, dai nodi avversari la mossa memorizzata (`PROVEN_NOWIN`) oppure un figlio `PROVEN_NOWIN` o, da un nodo `UNKNOWN_CYCLE`, un figlio `UNKNOWN_CYCLE` (Thm 4.8 con la precisazione di M0); sink ammessi: matto subito dal goal, stallo, posizione morta, TB non vinta dal goal; 50 mosse e ripetizioni non sono sink (se il gioco resta nell'insieme finisce in patta). L'esito è "certificato" solo se il controllo passa; un'entry mancante (rimpiazzata) o un sink dipendente dal contatore fanno fallire l'estrazione, non il prover. *Ri-ricerca locale* (G2, scostamento 16): un nodo del difensore con entry `UNKNOWN_CYCLE` e nessun figlio `PROVEN_NOWIN`/`UNKNOWN_CYCLE` viene ri-cercato con il nucleo df-pn nella stessa tabella (`DfPn::solve(b, retaint = true)`, 10^6 `do_move`, al più 64 nodi distinti per certificato, contati in `Certificate::researches` e stampati come "ri-ricerche N"); se dopo la ri-ricerca il nodo ha una mossa la chiusura riprende, altrimenti riparte dalla radice con le entry nuove (la scelta a monte era sbagliata, es. il nodo è risultato `PROVEN_WIN`). `verify` e `verify_draw_strategy` prendono quindi la tabella non costante.

## Protocollo dei benchmark (§9.4, §11)

- **B6** (`--suite`): per ogni riga EPD (`bm #N`: N > 0 il tratto matta in N mosse = 2N−1 ply, N < 0 il tratto è mattato in |N| mosse = 2|N| ply; goal = chi matta) e per ogni solver, una ricerca con tetto `--time` secondi (10) su un thread, senza TB, tabella azzerata. "Risolta entro 1 s" = risolta con tempo ≤ 1 s nella stessa ricerca: i solver sono deterministici a parità di ordinamento, la ricerca a 1 s è il prefisso di quella a 10 s. Le varianti a caldo (`warm`, `dfpn-warm`) condividono una sola ricerca di Stockfish di 100 ms per posizione, compresa nel loro tempo. `sf` = `go mate |N| movetime T` di Stockfish 19 con la rete, un thread, Hash 16 MiB, tabella e storie azzerate, eseguito per ultimo: risolta se il punteggio finale è un matto con ply ≤ dichiarati e segno coerente; il tempo è quello dell'ultima `info`. Ogni PROVEN_WIN dei prover passa dal verificatore (§7.9.1): "certificata" solo se il controllo passa; `d` è quello del prover (WinTree e nucleo df-pn: profondità dell'albero di prova trovato) o la profondità dell'albero verificato `dver` (baseline, che non calcola `d`). Varianti (M2c): `dfpn`, `dfpn-warm`, `-noinit` (senza euristica di pn/dn), `-epsN` (ε = N %), `base`, `base-warm` (baseline di M2b), `cold`, `warm`, `random` (WinTree), `sf`. `--threads 4` posizioni in parallelo (ogni ricerca resta a un thread): la macchina ha 4 core fisici con hyper-threading, quindi i nodi/s per thread sono un po' sotto quelli di una ricerca isolata; i confronti in nodi non ne dipendono.
- **Nodi**: "espansioni" = chiamate ricorsive (`rec` di WinTree, `mid` di df-pn); "do_move" = `ChessGame::do_move` (una per nodo visitato più una per figlio letto in tabella, §9.4: è la misura primaria del confronto WinTree/df-pn; esclude le `do_move` interne all'ordinamento a caldo, pagate nel tempo).
- **B4** (ablazione dell'ordinamento) e **B5** (ablazione di O6 e del dejavu): stesso runner su 200 posizioni (una ogni 10) con le varianti `random,cold,warm` e `cold,cold-noo6,cold-nodejavu,warm-noo6,warm-nodejavu`; nodi-per-prova mediana e coda sulle posizioni risolte da entrambe le varianti confrontate.
- **B2** (`--b2`): prover in modalità NOWIN con terminali solo matto/stallo, ordinamento freddo, un thread, sulle prime 20 posizioni della suite con |N| ≥ 10 (entro 4 ply nessun matto: il prover attraversa l'albero come perft, meno le trasposizioni tagliate dalla tabella), contro `perft 4` di Stockfish (`Benchmark::perft`, conteggio in blocco all'ultimo ply: le sue `do_move` sono perft(1)+perft(2)+perft(3)).
- CSV in `tests/results/` (una riga per posizione e solver: esito, tempo, cpu, espansioni, do_move, `d`, `d` verificato, certificato, nodi AND, riespansioni, ply medio e massimo, iterazioni); le tabelle sotto sono quelle stampate dal runner.

## Risultati M2a (2026-10-16, g++ 16.2, 8 core)

**Regressioni** (`--regress`, budget 10^6 nodi, con e senza TB): le 4 FEN di matto/stallo del bench di Stockfish (`benchmark.cpp:91-94`) più casi elementari, tutte PROVEN e certificate. Senza TB:

| Posizione | Goal | Esito | Nodi | Certificato |
|---|---|---|---:|---|
| bench 1 (`6k1/3b3r/… b`) | nero | `PROVEN_WIN d=9` (Stockfish: matto in 5 = 9 ply, Thm 3.6 esatto), mossa Qxf4 | 422 328 | albero 192 nodi, 73 foglie |
| bench 2 (`r2r1n2/… w`) | bianco | `PROVEN_WIN d=7` (matto in 4), mossa Qg7+ | 112 365 | 8 nodi, 1 foglia |
| bench 3 (stallo) | entrambi | `PROVEN_NOWIN` alla radice | 1 | 1 nodo |
| bench 4 (nero matto) | bianco / nero | `PROVEN_WIN d=0` / `PROVEN_NOWIN` | 1 | 1 nodo |
| Re8# | bianco | `PROVEN_WIN d=1` | 2 | 2 nodi |
| `k7/8/1K6/8/8/8/8/7R b` | bianco / nero | `PROVEN_WIN d=2` / `PROVEN_NOWIN` (ogni mossa nera subisce matto) | 5 | 3 nodi |
| KQ-K (`4K2Q`, Ke3) | bianco, win | `PROVEN_WIN d=15` (DTM 15) | 151 390 | 49 nodi, 7 foglie |
| KQ-K | nero, nowin | `PROVEN_NOWIN req=0` (il re solo non vince) | 13 910 | grafo 750 posizioni, 29 foglie |
| KN-K | bianco | `PROVEN_NOWIN` (posizione morta) | 1 | 1 nodo |

Con le TB attive KQ-K è un sink alla radice (`PROVEN_WIN d=0 rev=100`, 1 foglia TB) e la DTM non è più un limite inferiore (le foglie TB accorciano l'albero).

**G0 scacchistico** (`--g0`): posizioni casuali legali a 3-5 pezzi (tipi, colori e lato al tratto casuali, niente arrocco né ep, contatore 0 perché le WDL assumono contatore 0), goal casuale, prover **senza TB** con budget 2·10^5 nodi per ricerca (modalità win con ID, poi nowin se irrisolta), confronto di ogni PROVEN con `probe_wdl` dalle 5-man (WIN ⟺ WDL = 2 per il goal, NOWIN ⟺ WDL ≤ 1), verifica di ogni PROVEN.

| Campione (`--seed 1`, 8 thread) | Risolte | PROVEN_WIN | PROVEN_NOWIN | UNKNOWN_CYCLE | UNKNOWN_BUDGET | Disaccordi con WDL | Tempo |
|---|---:|---:|---:|---:|---:|---:|---:|
| 10^4 (`make g0full`) | 3965 (39,7 %) | 798 | 3167 (1367 dalla ricerca nowin, 1591 radici terminali) | 24 | 6011 | **0** (oracolo FAIL 0) | 1657 s |
| 10^3 (`make test`) | 407 (40,7 %) | 81 | 326 (152 / 154) | 0 | 593 | **0** | 163 s |

- **Nodi mediani** (10^4): 400 000 su tutte le posizioni (= il doppio budget delle irrisolte), 1288 sulle risolte.
- **Certificati** (10^4): alberi vincenti **789/798** (9 estrazioni fallite per entry rimpiazzata nella tabella da 64 MiB, 0 verifiche fallite); strategie di patta **2883/3167** (284 prove che usano sink per 50 mosse o ripetizione, non certificabili da §7.9.2; 0 con terminale vinto dal goal); radici UNKNOWN_CYCLE complete con strategia di patta certificata: 24/24 (contate come patte certificate nel confronto con le WDL: 0 disaccordi).
- Le irrisolte sono finali con pezzi mobili (KQ-KQ, KR-KB, …) in cui la prova senza TB richiede l'intero grafo delle posizioni: il budget di 2·10^5 nodi non basta (nessun disaccordo, come richiesto; la frazione risolta è la misura del costo, da confrontare con la baseline df-pn in M2b).

## Risultati M2c — G0 scacchistico con il nucleo df-pn (2026-10-17, `make g0full`, 8 thread)

Stesso protocollo di M2a (10^4 posizioni casuali a 3-5 pezzi, seme 1, prover senza TB, oracolo WDL 5-man, ogni PROVEN verificato), con una sola ricerca df-pn in modalità NOWIN (tetto 4096) e budget **10^6 `do_move`** per ricerca (M2a: 2·10^5 espansioni WinTree ≈ 9·10^5 `do_move`: lavoro comparabile; in espansioni df-pn ne fa ~28 000). Una radice UNKNOWN_CYCLE (confutazione tainted, ricerca completa) è un candidato di patta che vale solo se §7.9.2 chiude: contata come certificata solo in quel caso.

| Nucleo | Risolte | PROVEN_WIN | PROVEN_NOWIN | UNKNOWN_CYCLE (patta certificata) | UNKNOWN_BUDGET | Disaccordi con WDL | Alberi vincenti certificati | Strategie di patta certificate | Tempo |
|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| **df-pn (M2c)** | **4138 (41,4 %)** | 1772 | 2366 (1591 radici terminali) | 2916 (**2309**) | 2946 | **0** | 1770/1772 (2 entry sostituite) | 2366/2366 | 299 s |
| WinTree (M2a) | 3965 (39,7 %) | 798 | 3167 | 24 (24) | 6011 | 0 | 789/798 | 2883/3167 (284 con sink 50 mosse/ripetizione) | 1657 s |

- **Esiti certificati** (albero vincente, strategia di patta da PROVEN_NOWIN o da UNKNOWN_CYCLE): df-pn 1770 + 2366 + 2309 = **6445 (64,5 %)** contro 3696 (37,0 %) di WinTree a pari `do_move`; le vittorie dimostrate raddoppiano (1772 contro 798) e le irrisolte per budget scendono da 6011 a 2946.
- I PROVEN_NOWIN puliti sono meno (2366 contro 3167) perché ogni confutazione che passa da una ripetizione è tainted (UNKNOWN_CYCLE): 2309 di queste 2916 radici hanno comunque una strategia di patta chiusa e verificata (§7.9.2), le altre 607 restano candidati non certificabili (nessun certificato falso). Nessuna strategia di patta "senza mossa del difensore": df-pn confuta tramite ripetizioni (tainted ⇒ verificate come grafi) e non tramite sink per 50 mosse come faceva WinTree in 284 casi.
- Mediane: 1799 espansioni / 50 276 `do_move` su tutte le posizioni, 9 / 252 sulle risolte (1591 radici terminali). I 2 alberi vincenti non estratti: entry sostituita (bucket di soli PROVEN o riprova a contatore diverso con terzo regime, vedi `../wintree/README.md` M2c punto 3), nessuna verifica fallita.

## Risultati M2c — G1 ripetuto con il nucleo df-pn (2026-10-17, `make bench-g1-full`)

Protocollo di B6 come in M2b (2000 posizioni di `mates2000`, tetto 10 s per solver, un thread per ricerca, 4 posizioni in parallelo, tabella da 256 MiB azzerata, nessuna TB, ogni PROVEN_WIN verificato); 4958 s per le 7 varianti, poi 211 s per ε su 200 posizioni (una ogni 10). CSV: `tests/results/g1-full-m2c.csv`, `tests/results/g1-eps.csv`; log `tests/results/bench-g1-m2c.log`. Varianti: `dfpn`/`dfpn-warm` = nucleo di produzione (ε = 0,25, euristica di pn/dn), `-noinit` = senza euristica (pn = dn = 1), `base`/`base-warm` = baseline di M2b (stessa binaria, stessa corsa: 1852/2000 posizioni con le stesse espansioni della corsa di M2b a freddo e tempi 0,84×, quindi la macchina era un po' più libera; `sf` identico in 1787/2000). Tabelle stampate dal runner.

#### B6 — suite (2000 posizioni, tetto 10 s per solver, un thread per posizione; a caldo: 100 ms di Stockfish compresi)

| Solver | Risolte ≤ 1 s | Risolte ≤ 10 s | Certificate | d = dichiarato | d > dichiarato | Mediana espansioni | Mediana do_move | Mediana s | p90 do_move |
|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| dfpn | 1697 | 1917 | 1914 | 704 | 1213 | 3923 | 108150 | 0.042 | 2776180 |
| dfpn-warm | 1673 | 1921 | 1921 | 848 | 1073 | 3138 | 85745 | 0.138 | 2474065 |
| dfpn-noinit | 1458 | 1846 | 1845 | 691 | 1155 | 12606 | 432830 | 0.100 | 10933018 |
| dfpn-warm-noinit | 1442 | 1844 | 1844 | 827 | 1017 | 10648 | 353845 | 0.185 | 10250646 |
| base | 1405 | 1772 | 1766 | 690 | 1076 | 11711 | 395145 | 0.197 | 10678415 |
| base-warm | 1411 | 1789 | 1785 | 822 | 963 | 9755 | 326679 | 0.282 | 9787890 |
| sf | 1375 | 1788 | — | 1788 | 0 | 106036 | 0 | 0.196 | 0 |

#### Rapporti per posizione (sulle risolte da entrambi)

| A / B | Risolte da entrambi | Solo A | Solo B | Mediana do_move A/B | p90 | max | Mediana espansioni A/B | p90 | Σ do_move A / Σ do_move B |
|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| dfpn / base | 1762 | 155 | 10 | 0.21 | 0.44 | 3.05 | 0.26 | 0.49 | 0.20 |
| dfpn-warm / dfpn | 1902 | 19 | 15 | 0.84 | 1.35 | 211.17 | 0.85 | 1.36 | 0.89 |
| dfpn-warm / base-warm | 1782 | 139 | 7 | 0.21 | 0.57 | 3.51 | 0.26 | 0.64 | 0.19 |
| dfpn-noinit / dfpn | 1836 | 10 | 81 | 4.65 | 8.54 | 220.15 | 3.74 | 6.13 | 4.84 |
| dfpn-warm-noinit / dfpn | 1833 | 11 | 84 | 3.99 | 7.90 | 865.08 | 3.13 | 6.19 | 4.43 |
| dfpn-noinit / base (dal CSV) | 1763 | 83 | 9 | 1.00 | 1.03 | 4.05 | — | — | 1.00 |

#### Confronto con Stockfish `go mate N` (stesso tempo, un thread)

| Solver | Risolte dove Stockfish risolve ≤ 1 s | Stockfish ≤ 1 s | Risolte e certificate dove Stockfish risolve ≤ 10 s | Stockfish ≤ 10 s | Solo il solver | Solo Stockfish |
|---|---:|---:|---:|---:|---:|---:|
| dfpn | 1218 (88.6 %) | 1375 | 1717 (96.0 %) | 1788 | 197 | 71 |
| dfpn-warm | 1213 (88.2 %) | 1375 | 1719 (96.1 %) | 1788 | 202 | 69 |
| dfpn-noinit | 1095 (79.6 %) | 1375 | 1658 (92.7 %) | 1788 | 187 | 130 |
| dfpn-warm-noinit | 1086 (79.0 %) | 1375 | 1661 (92.9 %) | 1788 | 183 | 127 |
| base | 1060 (77.1 %) | 1375 | 1588 (88.8 %) | 1788 | 178 | 200 |
| base-warm | 1061 (77.2 %) | 1375 | 1604 (89.7 %) | 1788 | 181 | 184 |

#### Dove il nucleo spende i nodi (medie sulle posizioni della variante)

| Solver | Esito | Posizioni | Espansioni medie | Nodi AND (%) | Ply medio | Ply massimo medio |
|---|---|---:|---:|---:|---:|---:|
| dfpn | risolte | 1917 | 32829 | 51.7 | 17.3 | 38.6 |
| dfpn | irrisolte | 83 | 787904 | 51.6 | 16.5 | 55.2 |
| dfpn-warm | risolte | 1921 | 29694 | 51.7 | 15.6 | 37.1 |
| dfpn-warm | irrisolte | 79 | 786268 | 51.7 | 16.4 | 52.2 |
| dfpn-noinit | risolte | 1846 | 83373 | 66.4 | 15.4 | 38.5 |
| dfpn-noinit | irrisolte | 154 | 1024564 | 59.2 | 16.1 | 47.2 |
| base | risolte | 1772 | 77847 | — | — | 35.2 |
| base | irrisolte | 228 | 752839 | — | — | 59.8 |

#### ε (200 posizioni, una ogni 10, a caldo; `tests/results/g1-eps.csv`)

| Solver | Risolte ≤ 1 s | Risolte ≤ 10 s | Certificate | Mediana espansioni | Mediana do_move | p90 do_move | Mediana do_move / ε = 0,25 | p90 | Σ |
|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| dfpn-warm (ε = 0,25) | 169 | 192 | 192 | 2245 | 62342 | 2913747 | 1 | 1 | 1 |
| dfpn-warm-eps0 | 144 | 161 | 161 | 2054 | 48423 | 2336488 | 1.21 (160 comuni, 32 risolte solo da ε = 0,25) | 2.07 | 1.81 |
| dfpn-warm-eps50 | 171 | 194 | 194 | 2227 | 71622 | 3387900 | 1.00 (192 comuni, +2 solo ε = 0,5) | 1.50 | 0.98 |

### Verdetto G1 con il nucleo df-pn (§11)

| Criterio (§11) | Soglia | M2c (suite intera, 2000 posizioni, 10 s, un thread) | M2b (WinTree) | Esito |
|---|---|---|---|---|
| Posizioni risolte **con certificato** fra quelle che Stockfish risolve a 10 s, ordinamento a caldo | ≥ 90 % | **1719/1788 = 96,1 %** (a freddo 1717/1788 = 96,0 %; senza euristica 92,9 % / 92,7 %; baseline di M2b nella stessa corsa 89,7 % / 88,8 %) | 64,1 % | **superato** |
| Mediana nodi-per-prova nucleo / df-pn "da manuale" (`do_move`, stesso ordinamento, risolte da entrambi) | ≤ 3× | **0,21×** su 1782 posizioni (espansioni 0,26×; Σ 0,19×); a freddo 0,21× su 1762 | 7,1× | **superato** |
| Coda del rapporto | ≤ 10× | **p90 0,57×**, max 3,51× (a freddo 0,44× / 3,05×) | 332× | **superato** |

Letture:

1. **Da dove viene il guadagno**. Senza euristica di pn/dn il nucleo nuovo riproduce l'attraversamento della baseline (`dfpn-noinit / base`: mediana 1,00, p90 1,03 sulle 1763 comuni) ma risolve 74 posizioni in più a freddo (1846 contro 1772) e passa dall'88,8 % al 92,7 %: sono le radici che la baseline chiudeva come "ignote" per la saturazione a 2^30 (79 in M2b) e qualche tabella piena (4-way bucket: certificati mancanti da 6 a 1-3, a caldo 0). L'inizializzazione di Allis (mobilità, con pn dimezzato per scacco/cattura/ttMove) vale da sola 0,21× le `do_move` (Σ 0,19-0,21×) e +81/+85 risolte: è il contributo maggiore, come previsto da §8.1b. L'ordinamento a caldo aggiunge poco anche qui (0,84× mediana, p90 1,35×, +4 risolte; stessa frazione certificata 96,1 % contro 96,0 %), coerente con "spareggio a parità di pn/dn".
2. **ε**: 0,25 e 0,5 sono indistinguibili (mediana 1,00, p90 1,50, 192 contro 194 risolte su 200); ε = 0 (soglie +1 di Nagai) costa 1,21× le `do_move` mediane, 1,81× la somma e perde 31 posizioni: il trucco 1+ε è necessario, il valore resta 0,25.
3. **Posizioni ancora non certificate fra quelle che Stockfish risolve a 10 s, per |N|** (dfpn-warm; fra parentesi la baseline a caldo nella stessa corsa e WinTree a caldo in M2b): |N| ≤ 5: **0**/237 (9; 7), 6-7: **14**/821 (60; 152), 8-10: **21**/465 (56; 263), > 10: **34**/265 (59; 220). A freddo: 1, 12, 24, 34. Le mancate crescono ancora con |N|, ma la curva è piatta rispetto a WinTree (che oltre |N| = 8 mancava più della metà). Delle 69 mancate a caldo, 64 sono UNKNOWN_BUDGET a 10 s (8,5·10^5 espansioni medie: df-pn ha speso tutto il tempo senza chiudere) e 5 sono confutazioni tainted alla radice (UNKNOWN_CYCLE in 0,2-4,7 s con ply massimo 34-113, quindi non il tetto 245: una ripetizione del difensore, riusata da altri cammini, ha ucciso tutte le linee: è la perdita di completezza di §8.1b, 2 casi su 4142 anche sui grafi sintetici). A freddo si aggiungono 2 PROVEN_WIN non certificati (posizioni 618 e 1502, finali KQ-K e KNNN-K): entry dell'albero sostituita da una riprova a contatore diverso con un terzo regime (`../wintree/README.md`, M2c punto 3). Il nucleo risolve inoltre 202 posizioni che Stockfish non risolve in 10 s (69 il contrario).
4. **`d`**: df-pn trova un matto, non il più corto: `d` = dichiarato in 848/1921 casi a caldo (WinTree con l'iterative deepening 983/1286); il valore riportato è un limite superiore (I6) e il verificatore lo conferma (`dver`). Ri-ricerca con tetto decrescente: `--min-d`, misurata nella sezione "Attività (e)" (2026-10-25).
5. **Tempo**: mediana 0,042 s a freddo sulle risolte (baseline 0,197 s); a caldo 0,138 s di cui 0,100 s sono la ricerca di Stockfish. Espansioni/s ≈ 0,8·10^5 (≈ 35 `do_move` per espansione: a ogni giro del ciclo MID si rileggono tutti i figli con `do_move`, e `init` genera le mosse legali del figlio senza entry): la frontiera di ottimizzazione è lì (`key_after`, cache dei figli), fuori dal mandato di M2c; fatta il 2026-10-25 (sezione "Attività (e)").

## Attività (e) di §13 (2026-10-25): ciclo MID senza `do_move`, `d` minimo, posizioni morte, Chess960

Quattro modifiche al solver, ciascuna con misura prima/dopo sullo stesso binario di riferimento (albero a `e403db0`) e sul binario nuovo; regressioni: `make -C src/wintree test` (fuzz identico senza budget: 4140/7550/8310, 0 contrasti; con budget casuale 1408/8600/369 contro 1402/8611/374, per le confutazioni da tetto con residuo), `make -C src/wintree-chess test`, G0 10^4, `bench-g1` rapido. Protocollo B6 su 200 posizioni (`--step 10`, tetto 10 s, 4 thread, `--hash 256`, nucleo df-pn a freddo): CSV `tests/results/e-b6-200-prima.csv` e `e-b6-200.csv`.

1. **Ciclo MID senza `do_move`** (`ChessGame::key_after`, `mobility`, cache della mobilità nel nucleo: `../wintree/README.md` M2c). Prima ogni lettura di un figlio costava una `do_move` (≈ 35 per espansione); ora la chiave e il contatore del figlio si calcolano dalla posizione del padre e la `do_move` resta solo per la mobilità di un figlio senza entry non in cache. `key_after` è una replica esatta dell'aggiornamento di `st->key` in `Position::do_move` (arrocco anche 960, en passant solo se la presa è legale, promozioni, diritti persi da `from`/`to`), verificata con `WT_ASSERT` a ogni `do_move` fatta e, in `--regress`, su 266k mosse di 8 posizioni (kiwipete, ep con scoperta, promozioni, 5 FEN Chess960). Misura B6 su 200 posizioni:

   | dfpn a freddo | Risolte ≤ 10 s | Espansioni identiche | Mediana do_move | do_move per espansione (Σ/Σ; mediana per posizione) | Mediana s | Espansioni/s (mediana) | p90 do_move |
   |---|---:|---:|---:|---:|---:|---:|---:|
   | prima | 191 | — | 82 472 | 34,0; 29,3 | 0,031 | 87 283 | 2 595 976 |
   | dopo | 194 | 190/191 | 21 310 | 9,1; 7,4 | 0,020 | 178 979 | 993 199 |

   Rapporto dopo/prima per posizione sulle 191 risolte da entrambi: `do_move` mediana **0,26×** (p90 0,36×, max 0,50×; Σ 0,27×), tempo mediana 0,52× (Σ 0,54×), espansioni 1,00× (identiche su 190; la 1191 ha un sottoalbero KB-KB dello stesso colore che il punto 3 rende terminale: 2733 → 2751). `d` identico su tutte; 3 posizioni in più risolte entro 10 s (1321, 1891, 1971). Le ≈ 9 `do_move` residue per espansione sono la discesa (1) e le `init` dei figli senza entry la cui mobilità non è in cache (2^16 entry; la cache ne evita circa metà). `bench-g1` rapido (100 posizioni, 5 varianti): dfpn 97/97 risolte/certificate prima e dopo con mediana `do_move` 130 741 → 27 049 e tempo 0,055 → 0,027 s; dfpn-warm 99 → 98 (25 353 contro 112 763 `do_move`), Stockfish 89 in entrambe (le varianti WinTree e baseline oscillano di qualche posizione: tetto di tempo con carico concorrente). Sul matto in 5 del bench: 294 579 → 74 104 `do_move` (0,25×), 0,17 → 0,037 s, 6568 espansioni in entrambi; sul matto in 4: 677 → 245.

2. **`d` minimo** (`--min-d`; `DfPn::capD`; variante `dfpn-mind`): dopo una PROVEN_WIN con profondità `d`, ri-ricerche sulla stessa tabella con tetto `d − 2` e poi dicotomia fra l'ultimo tetto fallito e l'ultimo `d` trovato, entro il budget residuo. Una PROVEN_WIN in tabella vale per la ri-ricerca solo se `ply + d ≤ tetto`; altrimenti il nodo è ri-cercato nell'entry ombra (come la riprova a contatore diverso di §7.5) e una prova più corta aggiorna la base con il nuovo testimone, così il verificatore trova l'albero più corto. Due correzioni al nucleo sono state necessarie e valgono anche fuori da `--min-d`: (a) una confutazione da tetto memorizza la profondità residua e vale solo per letture con residuo minore o uguale (prima, come UNKNOWN_CYCLE senza residuo, bloccava le ri-ricerche con tetto maggiore e le trasposizioni a ply minore: 51 posizioni su 194 si fermavano sopra il dichiarato senza esaurire il budget); (b) in modalità `capD` lo stesso limite vale per le confutazioni da ciclo (un tetto basso ne produce molte: taglia anche le vie d'uscita dai cicli; esempio: posizione 91, `d` 38 → 12 con il riuso, → 8 = dichiarato senza). Senza `capD` le confutazioni da ciclo restano riusate come prima (incompletezza nota di §8.1b): il comportamento di default non cambia. Costo: la prova di minimalità è una ricerca che **fallisce** al tetto `d_min − 2`, cioè la confutazione esaustiva di ogni tentativo dell'attaccante entro quel tetto, e costa come tale anche da tabella vuota (matto in 5 del bench: la ri-ricerca a tetto 7 fa 183k espansioni contro 7k della prova a tetto 9). Misura B6 su 200 posizioni (`e-b6-200.csv`, varianti `dfpn` e `dfpn-mind`, stesso tetto di 10 s per tutto):

   | Variante | Risolte ≤ 10 s | d = dichiarato | d > dichiarato | Mediana espansioni | Mediana do_move | Mediana s | Risolte ≤ 1 s |
   |---|---:|---:|---:|---:|---:|---:|---:|
   | dfpn | 193 | 68 | 125 | 3107 | 19 756 | 0,017 | 178 |
   | dfpn-mind | 193 | **157** | 36 | 759 555 | 7 123 544 | 6,43 | 66 |

   Delle 36 posizioni che restano sopra il dichiarato 35 hanno esaurito i 10 s (la 36ª, 861, si è fermata a 7,4 s per una confutazione da ciclo riusata): entro il budget la minimizzazione raggiunge il dichiarato. Ri-ricerche: mediana 3, massimo 7; `d` migliora in 124 posizioni (mediana −6 ply, massimo −214: un matto in 3 trovato prima a 220 ply). Costo per posizione (dopo/prima): `do_move` mediana **77×** (p90 2797×, Σ 17,1×), tempo mediana 80×; il tempo mediano per arrivare al dichiarato è 2,2 s e 54 delle 157 hanno usato tutto il budget (il dichiarato è raggiunto ma la conferma al tetto `d − 2` non finisce). Tutte le 193 PROVEN_WIN sono certificate anche con `--min-d`. Senza `--min-d` nulla cambia (espansioni identiche su tutte le 193 risolte da entrambe). Suite intera (2000 posizioni, stesso protocollo, 2 varianti, 4 thread, 57 min; `tests/results/e-g1-mind.csv`):

   | Variante | Risolte ≤ 10 s | Certificate | d = dichiarato | d > dichiarato | Mediana espansioni | Mediana do_move | Mediana s | Risolte ≤ 1 s |
   |---|---:|---:|---:|---:|---:|---:|---:|---:|
   | dfpn | 1934 | 1932 | 704 | 1230 | 4041 | 24 587 | 0,024 | 1787 |
   | dfpn-mind | 1934 | **1934** | **1567** | 367 | 817 604 | 8 282 670 | 8,41 | 506 |

   A freddo df-pn risolve ora 1934 posizioni (M2c: 1846 a freddo, 1921 a caldo) e `d` = dichiarato resta 704 (M2c a freddo 704, a caldo 848); con `--min-d` 1567 (81 %): delle 367 sopra il dichiarato 366 hanno esaurito i 10 s e la 861 si è fermata a 17 ply su 13 per una confutazione da ciclo riusata (incompletezza di §8.1b). Per |N|: ≤ 4 tutte (119/119), 5: 123/126, 6: 380/411, 7: 396/478, 8: 175/239, 9: 114/178, 10: 74/109, ≥ 11: 186/274. Tempo mediano per arrivare al dichiarato 3,9 s (577 delle 1567 hanno usato tutto il budget per la conferma); `d` migliora in 1221 posizioni (mediana −8 ply, massimo −232); ri-ricerche mediana 3, massimo 8; costo `do_move` mediano **76×** (p90 3418×, Σ 18,7×). Le 2 PROVEN_WIN di `dfpn` non certificate sono le classi note di M2c (618: entry sostituita in un regime del contatore; 1603: albero più profondo del tetto con `d` saturo a 255); con `--min-d` le ri-ricerche le riscrivono e tutte le 1934 sono certificate.

3. **Posizioni morte** (`classify`, §7.4): oltre a KN-K, ogni posizione con soli re e alfieri tutti sullo stesso colore (K-K, KB-K, KB-KB stesso colore, KBB-K stesso colore, …): un re può essere messo in scacco solo da un alfiere, quindi sta su quel colore, le sue quattro case ortogonali sono dell'altro colore e non sono né attaccabili dagli alfieri né occupabili da essi, e il re avversario ne copre al più una (due di esse non hanno vicini comuni leciti): resta sempre una fuga, nessun matto è possibile (FIDE 5.2.2). KN-KN, KB-KN, KNN-K e gli alfieri di colore opposto non sono morte (matto aiutato) e restano al solver. Test in `--regress`: 11 FEN di confine e 20 000 posizioni casuali a 3-4 pezzi, di cui 4580 classificate morte, tutte con WDL = 0 per entrambi i lati.

4. **Chess960** (`--chess960`): `Position::set(fen, true)`, `UCI_Chess960` nel motore in-process (ordinamento a caldo), notazione 960 delle mosse (`b1a1` = re prende la propria torre); `verify_draw_strategy` riceve il flag (parametro obbligatorio, anche in `prover.cpp`) perché ricostruisce le posizioni dal FEN: con il flag spento `Position::fen` scrive `K`/`Q` e `set` cercherebbe la torre dall'angolo, assegnando il diritto alla torre sbagliata. Test in `--regress`: `key_after` sulle 2 FEN 960 del bench di Stockfish e su 2 di `tests/perft.sh` (arrocco con torre in e1), perft 960 di `tests/perft.sh` (3 posizioni, conteggi identici), matto in 1 per arrocco `2rkr3/2p1p3/8/1B6/8/8/8/RK6 w A -` (0-0-0: `PROVEN_WIN d=1 mossa=b1a1`, certificato; a caldo lo stesso).

**G0 10^4** (`make g0full`, stesso seme e budget di M2c, 2·10^5 espansioni e 10^6 `do_move` per ricerca): **0 disaccordi** con le WDL; risolte **4730**/10 000 (PROVEN_WIN 2256, PROVEN_NOWIN 2474) contro 4138 (1772, 2366) di M2c, strategie di patta certificate da radici UNKNOWN_CYCLE 3019 (2888), UNKNOWN_BUDGET 2125 (2946): a parità di budget in `do_move` il solver espande 3-4 volte di più, e le posizioni morte nuove chiudono subito KB-KB e simili (radici terminali 1687 contro 1591); alberi vincenti certificati 2250/2256 (6 entry rimpiazzate), strategie di patta 2474/2474; espansioni mediane 1558 (1799), `do_move` mediane 7842 (50 276); log `tests/results/e-g0-m2c.log`.

**Ibrido** (`src/stockfish-hybrid`, non ricompilato nel repository: lo usa un'altra sessione; verificato su una copia): `bench` 2497913 con `Prover=off`, `tests/instrumented.py` 88/88, `go mate 5` sul bench 1 → `proven mate in 5 move h4f4` e `score mate 5` (79 715 `do_move`, 45-105 ms: una ri-ricerca a tetto 10), `go mate 3` → la ri-ricerca a tetto 6 esaurisce `ProverMaxNodes` e resta `proven mate in 11` (certificato con il tetto originale: il verificatore non usa il tetto della ri-ricerca), `go movetime 1000` → `proven mate in 11` come in M3, `UCI_Chess960` + `go mate 1` sul matto per arrocco → `proven mate in 1 move b1a1`.

## Ordinamento a caldo: scelta (2026-10-16)

La prima versione seguiva §8.4 alla lettera (nodi OR: ttMove, scacchi per SEE, catture per storia, promozioni, quiete per storia; nodi AND: ttMove, catture, risposte per "meno scacchi lasciati al goal", storia). Sul campione rapido (100 posizioni, 10 s) risolveva **49** posizioni contro **59** del freddo, con mediana delle `do_move` 1,41× il freddo sulle risolte da entrambi. Ablazione su 8 posizioni fra le peggiori (do_move, 10 s): il componente dannoso è la **ttMove in testa** (es. posizione 1221: 1,1·10^6 a freddo, 6,0·10^6 con la sola ttMove; 441: 0,8 vs 1,8·10^6), anche limitata ai punteggi di matto in TT (1221: 4,9·10^6); storie e "meno scacchi" sono neutri. Spiegazione: con l'iterative deepening sulla profondità vince il primo testimone; la ttMove di una ricerca di 100 ms è spesso una mossa buona ma non la più forzante, e porta a un matto più lungo di una o due iterazioni, mentre le classi del freddo (scacchi prima, per entrambi i colori) trovano i matti più corti e, nei nodi AND, lo scacco del difensore è la confutazione più economica (il goal ha poche risposte). Confronto su 40 posizioni (una ogni 50, tetto 5 s, 4 thread), varianti contro il freddo (risolte 21 a freddo):

| Variante a caldo | Risolte | Mediana do_move caldo/freddo | p90 | max | Σ do_move caldo / Σ freddo |
|---|---:|---:|---:|---:|---:|
| classi del freddo + ttMove in testa alla sua classe | 22 | 0,95 | 1,53 | 2,14 | 0,68 |
| **classi del freddo + ttMove in testa alla classe + storie come spareggio (adottata)** | **22** | **0,83** | **1,50** | **3,65** | **0,63** |
| classi di §8.4 senza ttMove (OR per SEE/storie, AND "meno scacchi") | 20 | 0,89 | 1,44 | 2,07 | 0,98 |
| §8.4 alla lettera (ttMove in testa + classi di §8.4) | 19 | 0,97 | 2,83 | 37,3 | 0,69 |

Il guadagno dell'ordinamento a caldo su WinTree è quindi modesto (≈ 0,8× le `do_move`, coda invariata); per df-pn, che non ha iterative deepening, la ttMove in testa non è stata misurata separatamente: df-pn usa lo stesso ordinamento adottato.

## Risultati M2b — G1 rapido (2026-10-16, `make bench-g1`: 100 posizioni di mates2000, una ogni 20, tetto 10 s, 4 posizioni in parallelo; 387 s)

Posizioni per lunghezza del matto dichiarato |N| nel campione: ≤ 5: 12, 6-7: 46, 8-10: 25, > 10: 17. Tabelle stampate dal runner (CSV: `tests/results/g1-quick.csv`); per `sf` la colonna "espansioni" sono i nodi di Stockfish.

#### B6 — suite (100 posizioni, tetto 10 s per solver, un thread per posizione; a caldo: 100 ms di Stockfish compresi)

| Solver | Risolte ≤ 1 s | Risolte ≤ 10 s | Certificate | d = dichiarato | d > dichiarato | Mediana espansioni | Mediana do_move | Mediana s | p90 do_move |
|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| cold | 39 | 60 | 54 | 42 | 18 | 415293 | 1664242 | 0.559 | 20626464 |
| warm | 38 | 63 | 59 | 50 | 13 | 279016 | 1332697 | 0.513 | 17066392 |
| dfpn | 71 | 88 | 88 | 32 | 56 | 13253 | 487147 | 0.270 | 8965974 |
| dfpn-warm | 71 | 90 | 90 | 43 | 47 | 9984 | 335766 | 0.357 | 8519349 |
| sf | 72 | 89 | — | 89 | 0 | 79947 | 0 | 0.159 | 0 |

#### Rapporti per posizione (sulle risolte da entrambi)

| A / B | Risolte da entrambi | Solo A | Solo B | Mediana do_move A/B | p90 | max | Mediana espansioni A/B | p90 | Σ do_move A / Σ do_move B |
|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| cold / dfpn | 55 | 5 | 33 | 11.45 | 103.96 | 2121.33 | 86.02 | 1181.74 | 8.86 |
| warm / cold | 59 | 4 | 1 | 0.75 | 1.70 | 10.84 | 0.72 | 1.65 | 0.72 |
| warm / dfpn-warm | 59 | 4 | 31 | 5.34 | 170.16 | 9893.98 | 47.30 | 964.31 | 5.99 |

#### Confronto con Stockfish `go mate N` (stesso tempo, un thread)

| Solver | Risolte dove Stockfish risolve ≤ 1 s | Stockfish ≤ 1 s | Risolte e certificate dove Stockfish risolve ≤ 10 s | Stockfish ≤ 10 s | Solo il solver | Solo Stockfish |
|---|---:|---:|---:|---:|---:|---:|
| cold | 33 (45.8 %) | 72 | 51 (57.3 %) | 89 | 3 | 38 |
| warm | 33 (45.8 %) | 72 | 55 (61.8 %) | 89 | 4 | 34 |
| dfpn | 59 (81.9 %) | 72 | 78 (87.6 %) | 89 | 10 | 11 |
| dfpn-warm | 59 (81.9 %) | 72 | 80 (89.9 %) | 89 | 10 | 9 |

#### Dove WinTree spende i nodi (medie sulle posizioni della variante)

| Solver | Esito | Posizioni | Espansioni medie | Nodi AND (%) | Riespansioni (%) | Ply medio | Ply massimo medio | Iterazioni medie |
|---|---|---:|---:|---:|---:|---:|---:|---:|
| cold | risolte | 60 | 1495942 | 9.6 | 2.5 | 12.5 | 12.2 | 6.3 |
| cold | irrisolte | 40 | 6599889 | 10.6 | 3.1 | 12.6 | 14.1 | 7.4 |
| warm | risolte | 63 | 1156812 | 10.4 | 2.6 | 12.0 | 12.4 | 6.4 |
| warm | irrisolte | 37 | 6071863 | 11.4 | 3.5 | 13.0 | 14.6 | 7.6 |
| dfpn | risolte | 88 | 72158 | 0.0 | 0.0 | 0.0 | 31.0 | 1.0 |
| dfpn | irrisolte | 12 | 521465 | 0.0 | 0.0 | 0.0 | 36.8 | 1.0 |
| dfpn-warm | risolte | 90 | 67609 | 0.0 | 0.0 | 0.0 | 33.8 | 1.0 |
| dfpn-warm | irrisolte | 10 | 551171 | 0.0 | 0.0 | 0.0 | 47.9 | 1.0 |

Misure derivate dal CSV (mediane per posizione, tempo di CPU del thread): WinTree 0,57-0,74 M espansioni/s e 2,9-3,4 M `do_move`/s con 4,6 `do_move` per espansione; df-pn 0,04-0,05 M espansioni/s e 1,2-2,3 M `do_move`/s con ≈ 39 `do_move` per espansione (rilegge tutti i figli a ogni giro del ciclo MID). Posizioni risolte da Stockfish entro 10 s ma **non** certificate da WinTree a caldo, per |N|: ≤ 5: 1/12, 6-7: 7/43, 8-10: 15/22, > 10: 11/12; per df-pn a caldo: 0, 3, 4, 2.

#### B2 — nodi/s del prover (terminali solo matto/stallo, NOWIN, profondità 4) vs perft 4 (posizioni con |N| >= 10)

| # | Posizione | perft foglie | perft do_move | perft s | perft Mfoglie/s | perft Mdo_move/s | prover espansioni | prover do_move | prover s | prover Mespansioni/s | prover Mdo_move/s | do_move/s prover ÷ perft |
|---:|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| 1578 | `1B2BRnK/8/n2pp2k/2p3p1/6p1/q1r4p/4r1pR/8 w - -` | 708117 | 19553 | 0.005 | 137.9 | 3.8 | 415803 | 845702 | 0.203 | 2.05 | 4.18 | 1.10 |
| 1579 | `1K2b3/3k1p2/3B1p2/3R3P/3P1P2/2P2p2/p7/7r w - -` | 94418 | 5778 | 0.001 | 63.5 | 3.9 | 63954 | 131707 | 0.027 | 2.40 | 4.95 | 1.27 |
| 1580 | `1N6/3Bpp2/1p3p2/bN1k4/3P3n/3K3p/p5p1/6nq w - -` | 73527 | 4202 | 0.001 | 60.2 | 3.4 | 45923 | 95829 | 0.021 | 2.21 | 4.61 | 1.34 |
| 1581 | `1b4rq/6pp/2p1ppbP/k1PpP3/3P1Pp1/1R6/2P3R1/7K w - -` | 298029 | 14795 | 0.004 | 75.5 | 3.7 | 167487 | 346752 | 0.071 | 2.37 | 4.90 | 1.31 |
| 1582 | `1n6/4p3/8/p1p4p/rb1pBK1p/p1p5/6R1/7k w - -` | 37396 | 4244 | 0.001 | 43.0 | 4.9 | 28360 | 58800 | 0.013 | 2.20 | 4.57 | 0.94 |
| 1583 | `1q2B2q/2q1B1q1/3qkq2/qrr1rRRQ/3QKQ2/2Q3Q1/1Q2n2Q/Q3n3 w - -` | 81 | 39 | 0.000 | 4.8 | 2.3 | 109 | 222 | 0.000 | 1.36 | 2.76 | 1.19 |
| 1584 | `2B5/3NK3/r1k5/1p6/1p1pPN1p/3P4/1P5B/1q5n w - -` | 291252 | 13716 | 0.003 | 84.1 | 4.0 | 169265 | 347885 | 0.078 | 2.18 | 4.48 | 1.13 |
| 1585 | `2K5/2N2p2/1p6/pBkN1Rp1/2pp4/nb1pP1Bp/3nP2q/7r w - -` | 770825 | 31773 | 0.009 | 84.8 | 3.5 | 456628 | 931854 | 0.227 | 2.01 | 4.10 | 1.17 |
| 1586 | `2Nb3r/p7/3RPp2/1p3R1n/2k2p2/P1p2B2/P1P3pn/1K3b2 w - -` | 446731 | 22064 | 0.006 | 70.2 | 3.5 | 242272 | 505642 | 0.111 | 2.18 | 4.56 | 1.31 |
| 1587 | `2R5/p7/8/3bpp2/2p2p2/5p2/5K1p/7k w - -` | 19561 | 2037 | 0.000 | 57.6 | 6.0 | 9326 | 20563 | 0.004 | 2.41 | 5.31 | 0.89 |
| 1588 | `2RN2kb/5p2/4brpP/6PR/8/1p6/1K6/N7 w - -` | 120973 | 7109 | 0.002 | 79.2 | 4.7 | 76781 | 159587 | 0.038 | 2.02 | 4.19 | 0.90 |
| 1589 | `2k1BKN1/1pP5/1Pp5/2p1p3/1nrp3b/3p1n2/8/1b6 w - -` | 47401 | 2542 | 0.001 | 81.4 | 4.4 | 20060 | 41581 | 0.008 | 2.59 | 5.36 | 1.23 |
| 1590 | `2q5/K2bBRp1/1p1p4/3r1p2/3bkp2/1N1p4/8/5N2 w - -` | 392858 | 12899 | 0.004 | 105.8 | 3.5 | 39177 | 85906 | 0.019 | 2.07 | 4.55 | 1.31 |
| 1591 | `2r5/1b2pq2/5p1K/b1p1p3/5p2/n1r1p3/1n4pp/R4B1k w - -` | 183854 | 4761 | 0.001 | 142.8 | 3.7 | 5938 | 26645 | 0.005 | 1.17 | 5.26 | 1.42 |
| 1592 | `3B1K1k/7r/8/6p1/6P1/8/p7/8 w - -` | 14358 | 997 | 0.000 | 77.9 | 5.4 | 10715 | 22607 | 0.004 | 2.75 | 5.79 | 1.07 |
| 1593 | `3N1b1q/kBB1p1rb/6p1/rP4K1/P7/8/8/N6R w - -` | 162662 | 10992 | 0.003 | 60.2 | 4.1 | 89297 | 187421 | 0.050 | 1.78 | 3.73 | 0.92 |
| 1594 | `3R4/1p2BKbN/4pp2/1p2k3/6P1/1p1nPP2/brr5/8 w - -` | 371540 | 16936 | 0.005 | 81.3 | 3.7 | 187457 | 387116 | 0.091 | 2.06 | 4.26 | 1.15 |
| 1595 | `3R4/K7/1N2kppN/4p3/n1P5/pp2P3/3n4/4r3 w - -` | 355023 | 16505 | 0.004 | 86.0 | 4.0 | 193739 | 401304 | 0.079 | 2.45 | 5.07 | 1.27 |
| 1596 | `3r3k/p4Qp1/7p/1b1Bp3/4Pp2/7R/4nqPP/4R2K w - -` | 1812155 | 52619 | 0.014 | 130.2 | 3.8 | 152718 | 356559 | 0.076 | 2.00 | 4.67 | 1.23 |
| 1597 | `4KR2/8/b2p4/1pnpk3/rq1b1R2/3PP3/6p1/1N4r1 w - -` | 457061 | 15716 | 0.004 | 104.0 | 3.6 | 264329 | 539257 | 0.121 | 2.19 | 4.46 | 1.25 |
| **totale** | 20 posizioni | 6657822 | 259277 | 0.07 | 96.6 | 3.8 | 2639338 | 5492939 | 1.25 | 2.12 | 4.41 | 1.17 |

perft conta in blocco all'ultimo ply (96,6 M foglie/s) e fa una `do_move` solo nei nodi interni (3,8 M/s); il prover, con terminali solo matto/stallo e tabella attiva, fa 2,1 M espansioni/s e 4,4 M `do_move`/s: per espansione paga una generazione legale, l'ordinamento (`stable_sort`) e una lettura in tabella per figlio con `do_move`/`undo_move`, circa 0,5 µs in tutto, cioè ≈ 1,2 volte il costo di un nodo interno di perft per `do_move`. La tabella taglia le trasposizioni: 2,6·10^6 espansioni contro 6,7·10^6 foglie di perft.

### Verdetto G1 (§11)

| Criterio (§11) | Soglia | Suite intera (2000 posizioni) | Campione rapido (100) | Esito |
|---|---|---|---|---|
| Posizioni risolte **con certificato** fra quelle che Stockfish risolve a 10 s, ordinamento a caldo | ≥ 90 % | WinTree a caldo **1147/1789 = 64,1 %** (a freddo 1062/1789 = 59,4 %); df-pn a caldo 1583/1789 = 88,5 %, df-pn a freddo 1572/1789 = 87,9 % | 55/89 = 61,8 % (a freddo 57,3 %; df-pn 89,9 % e 87,6 %) | **non superato** |
| Mediana nodi-per-prova WinTree / df-pn (`do_move`, stesso ordinamento, risolte da entrambi) | ≤ 3× | a caldo **7,1×** su 1201 posizioni (espansioni 53×); a freddo 8,6× su 1126 (espansioni 63×) | 5,3× su 59 (espansioni 47×); a freddo 11,5× | **non superato** (fra 3× e 10×) |
| Coda del rapporto | ≤ 10× | p90 **332×** a caldo (max 8,8·10^4×); 290× a freddo | 170× (max 9,9·10^3×); a freddo 104× | **non superato** (> 10×) |

Lettura di §11: mediana fra 3× e 10× ⇒ "cambiare attraversamento mantenendo E"; coda oltre 10× ⇒ "fermarsi oppure adottare df-pn come nucleo mantenendo E e la semantica degli esiti". Il divario non è rumore: sulla suite intera df-pn a caldo risolve 585 posizioni che WinTree a caldo non risolve in 10 s (il contrario 85), ed è il controllo minimo di §11, non lo stato dell'arte. Il campione rapido (una posizione ogni 20) dava lo stesso verdetto; la suite intera sposta la mediana da 5,3× a 7,1× e la coda da 170× a 332×. Sulla somma delle `do_move` delle 1201 posizioni risolte da entrambi il rapporto è 3,0×: il divario è massimo dove df-pn chiude con pochi nodi.

### Dove WinTree spende i nodi

1. **Frontiera**: il ply medio delle espansioni è 0,87-0,89 del ply massimo dell'iterazione (mediana sulle risolte): quasi tutto il lavoro è negli ultimi due ply del limite di profondità corrente, dove ogni nodo paga una generazione legale (`terminal()`) e viene tagliato (`ply >= maxDepth`). È il profilo di una ricerca in profondità esaustiva con iterative deepening: 6 iterazioni mediane (profondità 1/2 → 11-13), l'ultima domina.
2. **Nodi OR**: i nodi AND sono il 10 % delle espansioni, quindi ogni nodo AND espande in media ≈ 9 figli OR. O6 rende i nodi AND economici (si fermano al primo figlio NIL), ma un nodo OR deve provare le mosse in ordine finché una vince e ogni tentativo fallito costa un intero sottoalbero limitato in profondità: WinTree non ha una stima (pn/dn) per abbandonare presto un tentativo poco promettente, lo lascia solo quando il NIL emerge alla frontiera. df-pn con soglie 1+ε abbandona un figlio appena il suo pn supera di poco il secondo: 10-13·10^3 espansioni mediane contro 280-415·10^3.
3. **Riespansioni** solo 2,5-3,5 % delle espansioni: le entry `UNKNOWN_BUDGET` con residuo evitano di riespandere i nodi delle iterazioni precedenti; il costo dell'iterative deepening non è rifare i nodi vecchi ma esplorare esaustivamente i nuovi alla frontiera.
4. **Matti lunghi**: le mancate certificazioni crescono con |N| (8-10: 15/22, > 10: 11/12), coerente con un costo ≈ b^(d/2) per tentativo OR fallito; df-pn ne manca 4/22 e 2/12.
5. **Certificati mancanti**: 6 PROVEN_WIN a freddo e 4 a caldo non certificati per "entry mancante" (tabella da 256 MiB = 2^24 entry; una ricerca di 10 s scrive ~10^7 entry e i PROVEN si sovrascrivono fra loro: §8.3 prevede "tra PROVEN il più vecchio", non implementato). Contati come non certificati nel verdetto.
6. **Ordinamento a caldo**: su WinTree 0,75× le `do_move` mediane e +3 risolte rispetto al freddo; su df-pn 0,7× e +2.
7. **`d` contro il matto dichiarato**: WinTree a caldo 50/63 uguali, 13 maggiori (riuso di entry provate in iterazioni a profondità diversa: `d` è un limite superiore, la profondità dell'albero verificato `dver` è nel CSV); df-pn 43/90 uguali e 47 maggiori, perché non minimizza la profondità. Stockfish risolve 89/100 (72 entro 1 s); nelle 11 mancate trova un matto più lungo del dichiarato o nessuno.

### Suite intera (bench-g1-full)

Eseguita il 2026-10-16 dalle 13:35 alle 16:24 in background (`make bench-g1-full`: 4 posizioni in parallelo, tetto 10 s, un thread per ricerca; log `tests/results/bench-g1-full.log`): B6 sulle 2000 posizioni in 7551 s (`g1-full.csv`), poi B4 in 902 s (`b4-order.csv`) e B5 in 1649 s (`b5-ablation.csv`) su 200 posizioni (una ogni 10). Tabelle stampate dal runner (per B4 e B5 la stessa tabella "suite" con le varianti dell'ablazione); il verdetto G1 è nella sezione sopra.

**B6** — risolte a 10 s: WinTree a freddo 1209 (1125 certificate), a caldo 1286 (1222), df-pn 1767 (1745), df-pn a caldo 1786 (1760), Stockfish `go mate N` 1789 (1381 entro 1 s). Il caldo vale 0,70× le `do_move` mediane del freddo (p90 1,83×) e +77 risolte; i certificati mancanti (entry rimpiazzate, punto 5 sopra) sono 84 a freddo, 64 a caldo, 22 e 26 per df-pn.

#### B6 — suite (2000 posizioni, tetto 10 s per solver, un thread per posizione; a caldo: 100 ms di Stockfish compresi)

| Solver | Risolte ≤ 1 s | Risolte ≤ 10 s | Certificate | d = dichiarato | d > dichiarato | Mediana espansioni | Mediana do_move | Mediana s | p90 do_move |
|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| cold | 684 | 1209 | 1125 | 860 | 349 | 584042 | 2307159 | 0.681 | 19332893 |
| warm | 730 | 1286 | 1222 | 983 | 303 | 465665 | 1929750 | 0.733 | 16469095 |
| dfpn | 1392 | 1767 | 1745 | 690 | 1055 | 11642 | 393901 | 0.294 | 10436233 |
| dfpn-warm | 1363 | 1786 | 1760 | 829 | 931 | 10068 | 330490 | 0.382 | 10531928 |
| sf | 1381 | 1789 | — | 1789 | 0 | 106036 | 0 | 0.190 | 0 |

#### Rapporti per posizione (sulle risolte da entrambi)

| A / B | Risolte da entrambi | Solo A | Solo B | Mediana do_move A/B | p90 | max | Mediana espansioni A/B | p90 | Σ do_move A / Σ do_move B |
|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| cold / dfpn | 1126 | 83 | 641 | 8.58 | 289.85 | 34270.03 | 63.49 | 2098.10 | 3.50 |
| warm / cold | 1164 | 122 | 45 | 0.70 | 1.83 | 205.95 | 0.69 | 1.93 | 0.75 |
| warm / dfpn-warm | 1201 | 85 | 585 | 7.07 | 332.00 | 87728.84 | 53.41 | 2342.62 | 3.02 |

#### Confronto con Stockfish `go mate N` (stesso tempo, un thread)

| Solver | Risolte dove Stockfish risolve ≤ 1 s | Stockfish ≤ 1 s | Risolte e certificate dove Stockfish risolve ≤ 10 s | Stockfish ≤ 10 s | Solo il solver | Solo Stockfish |
|---|---:|---:|---:|---:|---:|---:|
| cold | 566 (41.0 %) | 1381 | 1062 (59.4 %) | 1789 | 63 | 727 |
| warm | 617 (44.7 %) | 1381 | 1147 (64.1 %) | 1789 | 75 | 642 |
| dfpn | 1046 (75.7 %) | 1381 | 1572 (87.9 %) | 1789 | 173 | 217 |
| dfpn-warm | 1030 (74.6 %) | 1381 | 1583 (88.5 %) | 1789 | 177 | 206 |

#### Dove WinTree spende i nodi (medie sulle posizioni della variante)

| Solver | Esito | Posizioni | Espansioni medie | Nodi AND (%) | Riespansioni (%) | Ply medio | Ply massimo medio | Iterazioni medie |
|---|---|---:|---:|---:|---:|---:|---:|---:|
| cold | risolte | 1209 | 1499695 | 10.5 | 3.0 | 12.5 | 12.5 | 6.4 |
| cold | irrisolte | 791 | 7103995 | 10.2 | 2.9 | 12.9 | 14.3 | 7.4 |
| warm | risolte | 1286 | 1310704 | 10.6 | 3.2 | 12.4 | 12.6 | 6.5 |
| warm | irrisolte | 714 | 6660139 | 10.7 | 3.3 | 13.2 | 14.7 | 7.6 |
| dfpn | risolte | 1767 | 75693 | 0.0 | 0.0 | 0.0 | 35.1 | 1.0 |
| dfpn | irrisolte | 233 | 727855 | 0.0 | 0.0 | 0.0 | 59.5 | 1.0 |
| dfpn-warm | risolte | 1786 | 73535 | 0.0 | 0.0 | 0.0 | 33.2 | 1.0 |
| dfpn-warm | irrisolte | 214 | 689677 | 0.0 | 0.0 | 0.0 | 62.7 | 1.0 |

**B4 — ordinamento** (200 posizioni, 902 s): casuale 49 risolte, mediana `do_move` 1,96·10^6 e p90 16,7·10^6; freddo 120, 1,45·10^6 e 16,5·10^6; caldo 127, 0,95·10^6 e 14,1·10^6. Sulle 49 risolte anche dal casuale, freddo e caldo costano 0,11× le sue `do_move` (mediana; p90 1,0× e 1,2×, max 5,6× e 7,9×; Σ 0,23×): le classi del freddo valgono quasi un ordine di grandezza, il caldo aggiunge poco (0,70× il freddo su B6).

#### B4 — suite (200 posizioni, tetto 10 s per solver, un thread per posizione; a caldo: 100 ms di Stockfish compresi)

| Solver | Risolte ≤ 1 s | Risolte ≤ 10 s | Certificate | d = dichiarato | d > dichiarato | Mediana espansioni | Mediana do_move | Mediana s | p90 do_move |
|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| random | 29 | 49 | 46 | 35 | 14 | 685475 | 1958537 | 0.635 | 16715260 |
| cold | 82 | 120 | 110 | 85 | 35 | 310891 | 1453676 | 0.429 | 16476551 |
| warm | 80 | 127 | 120 | 102 | 25 | 258970 | 954066 | 0.474 | 14053703 |

#### Rapporti per posizione (sulle risolte da entrambi)

| A / B | Risolte da entrambi | Solo A | Solo B | Mediana do_move A/B | p90 | max | Mediana espansioni A/B | p90 | Σ do_move A / Σ do_move B |
|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| cold / random | 49 | 71 | 0 | 0.11 | 1.00 | 5.64 | 0.09 | 0.89 | 0.23 |
| warm / random | 49 | 78 | 0 | 0.11 | 1.18 | 7.87 | 0.09 | 1.08 | 0.23 |

#### Dove WinTree spende i nodi (medie sulle posizioni della variante)

| Solver | Esito | Posizioni | Espansioni medie | Nodi AND (%) | Riespansioni (%) | Ply medio | Ply massimo medio | Iterazioni medie |
|---|---|---:|---:|---:|---:|---:|---:|---:|
| random | risolte | 49 | 1572257 | 6.3 | 1.1 | 10.7 | 10.3 | 5.4 |
| random | irrisolte | 151 | 9132815 | 4.3 | 0.5 | 10.3 | 11.0 | 5.8 |
| cold | risolte | 120 | 1234181 | 10.2 | 2.8 | 12.3 | 12.4 | 6.4 |
| cold | irrisolte | 80 | 6894142 | 10.5 | 3.1 | 12.8 | 14.3 | 7.4 |
| warm | risolte | 127 | 1149756 | 10.8 | 3.3 | 12.7 | 12.6 | 6.5 |
| warm | irrisolte | 73 | 6255772 | 11.2 | 3.6 | 13.0 | 14.7 | 7.6 |

**B5 — ablazione** (200 posizioni, 1649 s): senza O6 (arresto al primo figlio NIL nei nodi AND, scostamento 1) le risolte scendono da 121 (120 in B4: una posizione al limite dei 10 s) a 28 a freddo e da 128 a 31 a caldo; sulle risolte da entrambe le varianti, senza O6 si pagano 32× le `do_move` del freddo con O6 (p90 500×, max 1,9·10^3×) a freddo e 21× (p90 518×) a caldo, e i nodi AND scendono dal 10 % all'1-2 % delle espansioni (ogni nodo AND espande tutti i figli). Senza dejavu non cambia nulla: a freddo stesse 121 risolte e rapporto 1,00 (max 1,02); a caldo 128 risolte e 0,74× il freddo, come il caldo completo.

#### B5 — suite (200 posizioni, tetto 10 s per solver, un thread per posizione; a caldo: 100 ms di Stockfish compresi)

| Solver | Risolte ≤ 1 s | Risolte ≤ 10 s | Certificate | d = dichiarato | d > dichiarato | Mediana espansioni | Mediana do_move | Mediana s | p90 do_move |
|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| cold | 82 | 121 | 110 | 85 | 36 | 310891 | 1453676 | 0.435 | 19471003 |
| cold-noo6 | 12 | 28 | 27 | 23 | 5 | 2526990 | 5408541 | 1.858 | 22974311 |
| cold-nodejavu | 82 | 121 | 110 | 85 | 36 | 311158 | 1453842 | 0.408 | 19472796 |
| warm-noo6 | 9 | 31 | 31 | 27 | 4 | 2191325 | 5008305 | 1.774 | 14422745 |
| warm-nodejavu | 80 | 128 | 125 | 101 | 27 | 289854 | 1103585 | 0.426 | 14562358 |

#### Rapporti per posizione (sulle risolte da entrambi)

| A / B | Risolte da entrambi | Solo A | Solo B | Mediana do_move A/B | p90 | max | Mediana espansioni A/B | p90 | Σ do_move A / Σ do_move B |
|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| cold-noo6 / cold | 28 | 0 | 93 | 31.86 | 499.92 | 1893.64 | 42.67 | 713.64 | 14.08 |
| cold-nodejavu / cold | 120 | 1 | 1 | 1.00 | 1.00 | 1.02 | 1.00 | 1.00 | 1.00 |
| warm-noo6 / cold | 31 | 0 | 90 | 20.89 | 518.04 | 1414.83 | 27.36 | 687.85 | 9.73 |
| warm-nodejavu / cold | 115 | 13 | 6 | 0.74 | 1.98 | 72.98 | 0.68 | 1.97 | 0.70 |

#### Dove WinTree spende i nodi (medie sulle posizioni della variante)

| Solver | Esito | Posizioni | Espansioni medie | Nodi AND (%) | Riespansioni (%) | Ply medio | Ply massimo medio | Iterazioni medie |
|---|---|---:|---:|---:|---:|---:|---:|---:|
| cold | risolte | 121 | 1240464 | 10.9 | 3.1 | 12.7 | 12.5 | 6.4 |
| cold | irrisolte | 79 | 6976715 | 10.3 | 3.0 | 12.7 | 14.2 | 7.4 |
| cold-noo6 | risolte | 28 | 3899665 | 1.9 | 0.6 | 10.6 | 10.5 | 5.4 |
| cold-noo6 | irrisolte | 172 | 13667061 | 0.8 | 0.2 | 7.7 | 8.2 | 4.4 |
| cold-nodejavu | risolte | 121 | 1278247 | 10.4 | 2.9 | 12.5 | 12.4 | 6.4 |
| cold-nodejavu | irrisolte | 79 | 6990823 | 10.5 | 3.1 | 12.8 | 14.3 | 7.5 |
| warm-noo6 | risolte | 31 | 2817431 | 2.1 | 0.7 | 10.1 | 10.3 | 5.3 |
| warm-noo6 | irrisolte | 169 | 13113229 | 0.7 | 0.1 | 7.6 | 8.2 | 4.3 |
| warm-nodejavu | risolte | 128 | 1176015 | 10.4 | 3.1 | 12.3 | 12.5 | 6.5 |
| warm-nodejavu | irrisolte | 72 | 6480624 | 11.2 | 3.7 | 13.2 | 14.9 | 7.7 |

## Scostamenti dalla spec e decisioni

1. **Arresto al primo figlio NIL nei nodi AND in modalità WIN** (nucleo, non previsto dalla spec): senza, la ricerca di matto a profondità 5 su bench 2 supera 10^7 nodi (ogni nodo AND esplora tutti i figli anche quando uno è già irrisolto); con, 1,1·10^5. Solido per I1; costa i PROVEN_NOWIN intermedi, che la modalità NOWIN recupera.
2. **Sink dipendenti dal contatore in entrambe le modalità** (politica esatta di §7.5, non l'alternativa prudente): una prova NOWIN che usa sink per 50 mosse o triplice è esatta in tabella (`req`) ma non certificabile da §7.9.2 (il verificatore trova il nodo senza espansione: "nessuna mossa del difensore"), contata a parte nei test. L'alternativa (niente sink di patta in NOWIN) allungherebbe le linee fino al GRAY: da misurare in M2b.
3. **Foglie TB nel grafo di patta a qualunque contatore**: la spec le vuole "raggiunte con mossa irreversibile"; WDL ≤ 1 (assunto contatore 0) implica non vittoria a ogni contatore (meno mosse disponibili), quindi la condizione è superflua e il verificatore non la impone. Per le foglie TB *vinte* il contatore 0 resta obbligatorio.
4. **Memo del verificatore dell'albero su (posizione, contatore)** invece del replay puro dell'albero srotolato (esponenziale nei sottoalberi condivisi); argomento nel paragrafo "Verificatore".
5. **Tabella**: nessuna età/generazione; l'ultimo scrive, mai un PROVEN sovrascritto da un UNKNOWN (corretto un bug di M1 che testava la chiave invece dei dati). Un'entry rimpiazzata fa fallire l'estrazione ("entry mancante"), contata a parte.
6. **Budget**: in modalità win il budget è complessivo sulle iterazioni; `UNKNOWN_BUDGET resid=0` = nodi esauriti. Niente tempo/`stop` nella CLI (il nucleo li ha).
7. **Build**: LTO tolto (un link da ~1 min a ogni modifica) e `-DNNUE_EMBEDDING_OFF`; tutto il resto come `make config-sanity ARCH=native`, che viene eseguito a ogni `make` (richiede la rete `.nnue` presente, altrimenti `scripts/net.sh` tenterebbe il download).
8. **df-pn, ripetizione sul cammino** (il mandato di M2b diceva "= ignoto"): implementata come sconfitta dell'attaccante (pn = ∞, dn = 0), il trattamento di Nagai 2002. "Ignoto" nel calcolo di pn/dn vuol dire (∞, ∞), ma in un nodo OR dn è una somma: un solo figlio ignoto dà dn = ∞ ≥ soglia e il ciclo MID del nodo si ferma subito, poi la stessa cosa risale fino alla radice; df-pn non prova più nulla oltre un ciclo. La variante è disponibile (`--dfpn-rep unknown`) e misurata sotto. Con la sconfitta, nessuna prova usa una ripetizione (solidità dei PROVEN_WIN, che comunque passano dal verificatore); le confutazioni possono dipendere dal cammino (GHI) e far perdere matti: è il "controllo minimo" previsto da §11, non lo stato dell'arte.
9. **Iterative deepening da 2 nelle radici AND**: con il difensore al tratto il matto cade a ply pari; la sequenza 1, 3, 5, … della spec (pensata per la radice OR) sprecherebbe un ply per iterazione.
10. **Conteggio dei nodi**: nel ciclo O1 di WinTree ogni figlio è letto in tabella con una `do_move`; nel ciclo MID di df-pn lo era fino al 2026-10-25 (`Position::prefetch_key` applica il bucket delle 50 mosse ed è approssimata), da allora `ChessGame::key_after` dà `st->key` esatta e la `do_move` resta solo per la mobilità dei figli senza entry non in cache. Le tabelle riportano entrambe le misure.
11. **Accesso a `Engine::tt` e `Engine::threads`** con l'istanziazione esplicita di un template su puntatori a membro privati, invece di una copia del costruttore di `Engine` (opzioni, NUMA, rete, thread) o di una modifica a `engine.h`.
12. **B2 su posizioni con |N| ≥ 10** invece delle prime 20 della suite: su un matto in 1 il prover chiude alla radice e non misura nulla.

13. **M2c, nucleo df-pn come default** (§8.1b): vedi `../wintree/README.md` ("M2c") per tabella a bucket, (pn, dn) a 30 bit, entry ombra, budget in `do_move`, `d`. Qui: la modalità (`--mode`) fissa solo il tetto di profondità; G0 usa una sola ricerca NOWIN (tetto 4096) invece delle due fasi WIN/NOWIN di WinTree (con il tetto 245 della modalità WIN le confutazioni tainted da tetto inquinerebbero la seconda fase).
14. **Inizializzazione di pn/dn** (§8.1b): regola di Allis (nodo AND pn = mobilità del difensore, dn = 1; nodo OR pn = 1, dn = mobilità dell'attaccante) con pn dimezzato per scacco, cattura SEE ≥ 0 e ttMove; la spec scrive "dn iniziale = mobilità nei nodi AND" (in un nodo AND dn è il minimo sui figli: una mobilità alta renderebbe il nodo *più* difficile da confutare, il contrario del senso della regola), letta come la regola classica.
15. **Baseline conservata** (`dfpn_baseline.h`, `--core base`): identica a M2b (tabella propria a 32 bit, saturazione a 2^30), come controllo minimo di §11; `dfpn-noinit` riproduce esattamente le sue espansioni (stesso attraversamento, con taint e rev/req in più).

16. **Ri-ricerca locale nel verificatore del grafo di patta** (G2, 2026-10-18). Diagnosi delle 16 radici `UNKNOWN_CYCLE` del set (a) a 100 s con "nessuna mossa del difensore (entry UNKNOWN_CYCLE)": al nodo bloccato i figli con entry sono tutti aperti (pn, dn) e nessuno è un antenato nel grafo del verificatore; la confutazione del nodo era la *ripetizione di un antenato del cammino di ricerca* (tainted), che il suo padre ha poi abbandonato con soglie superate: l'antenato resta aperto e il nodo, raggiunto dal verificatore per un'altra via, non ha un figlio chiuso. Non è una sovrascrittura: contate le scritture di un'entry aperta sopra un'`UNKNOWN_CYCLE` della stessa chiave in `DfPn::mid`, 0 su tutte e 16 le radici (il nucleo non rientra mai in un nodo tainted: per la selezione vale (∞, 0)); proteggere le entry tainted (approccio 2 del task) non avrebbe cambiato nulla, e l'eviction dai bucket resta possibile solo a tabella piena. Rimedio (approccio 1): ri-ricerca del nodo bloccato dalla stessa tabella con `retaint` (il cammino parte dal nodo, così l'antenato viene risolto come `UNKNOWN_CYCLE` o `PROVEN_NOWIN` e il ciclo si chiude per il Thm 4.8), 10^6 `do_move` per ri-ricerca, al più 64 nodi distinti per certificato, riavvio dalla radice se il nodo resta senza mossa. Misure: le 16 radici a 100 s passano da 0 a **10/16** certificate (3, 13, 29, 80, 113, 128, 131, 149, 153, 157; ri-ricerche 1-47, costo ≤ 1,5 s); con 10^5 `do_move` e 16 ri-ricerche 8/16. Le 6 restanti: 121 il nodo bloccato è `PROVEN_WIN` (la scelta a monte è sbagliata e il riavvio non basta: servirebbe ri-cercare il padre OR, non fatto), 34 entry evicted dalla tabella (3·10^6 espansioni in 256 MiB), 29-bis/49/54/133/144 nodi che non si risolvono entro il budget (49 è `UNKNOWN_BUDGET` anche da solo a 10 s). G0 10^3 (`make test`, nessuna TB): radici `UNKNOWN_CYCLE` certificate da 224 a 278 su 283 (10^5/16: 266), 0 disaccordi con le WDL, tempo da 52 a 54 s. Solidità invariata: il certificato è sempre il controllo di chiusura sulle posizioni; le entry scritte dalla ri-ricerca sono quelle di una ricerca normale (nessuna scrittura al taglio).

Non fatto: età nella ProofTable; alternativa prudente senza sink di patta in NOWIN (scostamento 2); doppio conteggio delle trasposizioni in df-pn (df-pn+); dejavu a prefisso per le confutazioni tainted; ri-ricerca del padre quando il nodo bloccato del verificatore risulta `PROVEN_WIN` (scostamento 16).
