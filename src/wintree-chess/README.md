# src/wintree-chess — M2, prima metà (spec §9.1): adapter scacchi, verificatore, G0 scacchistico

Binario `wintree-chess` = nucleo WinTree (`../wintree/wintree.h`) + adapter scacchi (§8.2) + verificatore (§7.9) + CLI + regressioni + test G0, linkato con i sorgenti di **Stockfish 19 non modificati** (`Stockfish-sf_19/src`, tutti tranne `main.cpp`: `tt.cpp` trascina `ThreadPool` e `position.cpp` `UCIEngine`, quindi quasi tutto il motore, come previsto in §9.1). Stesse opzioni di compilazione del Makefile di Stockfish per `ARCH=native` (lette da `make config-sanity`), senza LTO e con `-DNNUE_EMBEDDING_OFF` (la rete da 98 MB non serve e non viene mai caricata). Build da pulito: ~1 min di CPU, ~15 s con `-j8`; binario 1,2 MB.

```bash
make -C src/wintree-chess -j8            # build/wintree-chess
make -C src/wintree-chess test           # regressioni (con TB 5-man) + G0 su 10^3 posizioni: ~3 min su 8 core
make -C src/wintree-chess g0full         # G0 su 10^4 posizioni (8 thread): ~30 min   [TB=... G0=... SEED=... NODES=...]
src/wintree-chess/build/wintree-chess --fen "r2r1n2/pp2bk2/2p1p2p/3q4/3PN1QP/2P3R1/P4PP1/5RK1 w - - 0 1" --verify
#   esito PROVEN_WIN d=7 rev=3 mossa=g4g7 / nodi 112365 iterazioni 4 profondità 7 tempo 0.10 s / verifica certificato: nodi 8 foglie 1 TB 0
src/wintree-chess/build/wintree-chess --fen "<fen>" [--moves m1 m2 ...] [--goal white|black] [--mode win|nowin] [--nodes N] [--depth D] [--tb PATH] [--tbmax K] [--hash MiB] [--verify]
```

| File | Contenuto |
|---|---|
| `chess_game.h` | `classify()` (terminali di §7.4 su una posizione reale, condivisa da prover e verificatore) e `ChessGame`: `Position` con uno `StateInfo` per ply agganciato alla storia della partita (FEN + mosse, come `thread.cpp:345`), chiave `st->key`, lista legale generata una volta (`terminal()`) e riusata (`gen_children()`), ordinamento freddo di §8.4, `reversible()`/`rule50()` per §7.5 |
| `verifier.h` | `WinTreeVerifier` (albero vincente, §7.9.1) e `verify_draw_strategy` (grafo di strategia di patta, §7.9.2) |
| `main.cpp` | CLI, `--regress`, `--g0` (campionamento, confronto con WDL, verifica di ogni PROVEN) |

## Semantica implementata

- **Terminali** (`classify`, in quest'ordine): nessuna mossa legale → matto (vince chi non è al tratto) o stallo; `rule50 > 99` (il matto al 100° ply prevale, come `Position::is_draw`); `st->repetition < 0` (triplice, storia compresa, §7.3); posizione morta elementare K-K, KB-K, KN-K; TB solo con `rule50 == 0`, senza diritti d'arrocco, pezzi ≤ `--tbmax` (default `MaxCardinality`): WDL = +2 per il goal ⇒ vittoria valida solo con contatore 0 (`rev = 100`), WDL ≤ +1 ⇒ non vittoria valida per ogni contatore (§7.6, cursed win inclusa). `ProbeState::FAIL` ⇒ non terminale.
- **50 mosse** (§7.5, nel nucleo): `rev/req` per entry, controllo `c + rev <= 100` / `c >= req` a ogni lettura della tabella; esempio: KQ-K con contatore 98 dà `PROVEN_NOWIN req=98` (nessun matto in 2 ply), con contatore 0 `PROVEN_WIN d=15`.
- **Ordinamento freddo** (§8.4): scacchi, catture con `see_ge >= 0`, promozioni, quiete, catture perdenti; stabile entro la classe (ordine di `generate<LEGAL>`); uguale ai nodi OR e AND. ttMove e storie (ordinamento a caldo) in M2b.
- **Modalità** (`--mode`): `win` = iterative deepening sulla profondità (1, 3, 5, … fino a 245, §8.4) con budget di nodi complessivo e, nel nucleo, arresto al primo figlio NIL nei nodi AND; `nowin` = una ricerca al tetto (`--depth`, default 4096) che esplora tutti i figli (Sez. 3). Vedi `../wintree/README.md` ("Aggiunte di M2").
- **Verificatore** (`--verify`, e su ogni PROVEN dei test). *Albero vincente*: replay dalla radice con `do_move` sugli `StateInfo` reali (contatore e storia esatti, niente `rev`); ai nodi del goal la mossa memorizzata nell'entry, ai nodi avversari tutte le mosse di `MoveList<LEGAL>`; foglie ammesse: matti dati dal goal, TB WDLWin con contatore 0; memo sulle coppie (posizione, contatore) già verificate: un sottoalbero verificato a un dato contatore vale per ogni cammino (una ripetizione lungo un cammino fa solo crescere il contatore fino alla patta per 50 mosse, che il replay rileva; una triplice che dipende solo dalla storia non dipende dal cammino). *Grafo di strategia di patta* (per `PROVEN_NOWIN` e per una radice `UNKNOWN_CYCLE`, completa per I2): chiusura per visita in ampiezza su posizioni identificate dal FEN senza contatori; dai nodi del goal tutte le mosse, dai nodi avversari la mossa memorizzata (`PROVEN_NOWIN`) oppure un figlio `PROVEN_NOWIN` o, da un nodo `UNKNOWN_CYCLE`, un figlio `UNKNOWN_CYCLE` (Thm 4.8 con la precisazione di M0); sink ammessi: matto subito dal goal, stallo, posizione morta, TB non vinta dal goal; 50 mosse e ripetizioni non sono sink (se il gioco resta nell'insieme finisce in patta). L'esito è "certificato" solo se il controllo passa; un'entry mancante (rimpiazzata) o un sink dipendente dal contatore fanno fallire l'estrazione, non il prover.

## Risultati (2026-10-16, g++ 16.2, 8 core)

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

## Scostamenti dalla spec e decisioni

1. **Arresto al primo figlio NIL nei nodi AND in modalità WIN** (nucleo, non previsto dalla spec): senza, la ricerca di matto a profondità 5 su bench 2 supera 10^7 nodi (ogni nodo AND esplora tutti i figli anche quando uno è già irrisolto); con, 1,1·10^5. Solido per I1; costa i PROVEN_NOWIN intermedi, che la modalità NOWIN recupera.
2. **Sink dipendenti dal contatore in entrambe le modalità** (politica esatta di §7.5, non l'alternativa prudente): una prova NOWIN che usa sink per 50 mosse o triplice è esatta in tabella (`req`) ma non certificabile da §7.9.2 (il verificatore trova il nodo senza espansione: "nessuna mossa del difensore"), contata a parte nei test. L'alternativa (niente sink di patta in NOWIN) allungherebbe le linee fino al GRAY: da misurare in M2b.
3. **Foglie TB nel grafo di patta a qualunque contatore**: la spec le vuole "raggiunte con mossa irreversibile"; WDL ≤ 1 (assunto contatore 0) implica non vittoria a ogni contatore (meno mosse disponibili), quindi la condizione è superflua e il verificatore non la impone. Per le foglie TB *vinte* il contatore 0 resta obbligatorio.
4. **Memo del verificatore dell'albero su (posizione, contatore)** invece del replay puro dell'albero srotolato (esponenziale nei sottoalberi condivisi); argomento nel paragrafo "Verificatore".
5. **Tabella**: nessuna età/generazione; l'ultimo scrive, mai un PROVEN sovrascritto da un UNKNOWN (corretto un bug di M1 che testava la chiave invece dei dati). Un'entry rimpiazzata fa fallire l'estrazione ("entry mancante"), contata a parte.
6. **Budget**: in modalità win il budget è complessivo sulle iterazioni; `UNKNOWN_BUDGET resid=0` = nodi esauriti. Niente tempo/`stop` nella CLI (il nucleo li ha).
7. **Build**: LTO tolto (un link da ~1 min a ogni modifica) e `-DNNUE_EMBEDDING_OFF`; tutto il resto come `make config-sanity ARCH=native`, che viene eseguito a ogni `make` (richiede la rete `.nnue` presente, altrimenti `scripts/net.sh` tenterebbe il download).

Non fatto (M2b): ordinamento a caldo con TT/storie, suite di matti, baseline df-pn, G1; posizioni morte non elementari (KB-KB stesso colore ecc.: con le TB sono coperte); `UCI_Chess960`.
