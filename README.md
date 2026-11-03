# WinTreeChess

Studio dell'integrazione dell'algoritmo WinTree (`2607.09715v2.pdf`) in Stockfish 19. Relazione completa: [docs/superpowers/specs/2026-10-15-wintree-stockfish-design.md](docs/superpowers/specs/2026-10-15-wintree-stockfish-design.md) (§13).

**Risultato (§13).** Il progetto è uno **strumento di certificazione** di matti e di riduzioni a tablebase, costruito sopra Stockfish 19 intatto, con prove verificate da un verificatore indipendente: **non è un motore più forte di Stockfish**, a nessuna cadenza misurata. Come attraversamento il depth-first di WinTree perde contro un df-pn; il nucleo di produzione è df-pn con la semantica di WinTree (cicli, `rev/req`, verificatore), che certifica il 96 % dei matti risolti da `go mate`. Nel gioco `Prover=off` resta il default: a 10+0.1 il prover alla radice non parte mai, a 60+0.6 interviene in 10 partite su 1500 e la differenza di Elo misurata (−5,79 ± 5,53) non è distinguibile dal rumore; le fortezze oltre la copertura delle tablebase non sono certificate (ipotesi H2 non sostenuta).

## Mappa delle cartelle

| Cartella | Contenuto |
|---|---|
| [`docs/superpowers/specs/`](docs/superpowers/specs/2026-10-15-wintree-stockfish-design.md) | specifica e relazione finale (§13, §13.1); `docs/agents/` = convenzioni per gli agenti |
| [`tests/wintree-java/`](tests/wintree-java/README.md) | M0: Java del paper con P1 e P2 corretti (`enginep1/`, versione pubblicata), harness e fuzzing; `engine/` = originale, baseline dei test |
| [`src/wintree/`](src/wintree/README.md) | M1/M2c: nuclei generici depth-first e df-pn, test sintetici e differenziali |
| [`src/wintree-chess/`](src/wintree-chess/README.md) | M2: adapter scacchi, verificatore, CLI `wintree-chess`, suite di matti, benchmark G0/G1 |
| [`src/stockfish-hybrid/`](src/stockfish-hybrid/README.md) | M3: copia di Stockfish 19 con gli hook del prover e le opzioni UCI `Prover*` |
| [`tests/g2/`](tests/g2/README.md) | G2: corpus di fortezze e script di valutazione |
| [`tests/g3/`](tests/g3/README.md) | G3: SPRT con fastchess; [tuning](tests/g3/README-tuning.md) e campione a 60+0.6 |
| `tests/suites/`, `tests/books/`, `tests/results/` | suite di matti (`mates2000.epd`), libri di apertura, CSV e log dei risultati |
| [`tools/`](tools/README.md) | bootstrap: rete NNUE, fastchess, Syzygy (binari e sorgenti ignorati da git) |
| `Stockfish-sf_19/` | Stockfish 19 **intatto**: riferimento e sorgenti linkati da `wintree-chess` |

## Comandi essenziali

Da questa cartella. Verificati il 2026-10-25 (g++ 16, OpenJDK 25).

```bash
make -C src/wintree-chess -j8     # build/wintree-chess (~15 s con -j8)
```

```bash
make -C src/wintree test          # ~12 s: nuclei C++ contro oracolo e Java
```

```bash
make -C src/wintree-chess test    # ~30 s: regressioni (TB 5-man) + G0 su 10^3 posizioni
```

```bash
(cd tests/wintree-java && ./run.sh)   # ~4 s: harness Java e fuzzing (non ha `make test`)
```

Certificare un matto (esito atteso: `PROVEN_WIN d=7 ... mossa=g4g7`, `verifica certificato: nodi 8 ...`):

```bash
src/wintree-chess/build/wintree-chess --fen "r2r1n2/pp2bk2/2p1p2p/3q4/3PN1QP/2P3R1/P4PP1/5RK1 w - - 0 1" --verify
```

Stockfish ibrido (la rete NNUE è `Stockfish-sf_19/src/nn-1a298aa575a0.nnue`, vedi sotto):

```bash
make -C src/stockfish-hybrid -j8 ARCH=x86-64-bmi2 all     # ~35 s
```

```bash
cd src/stockfish-hybrid && printf 'setoption name Prover value mate\nposition fen r2r1n2/pp2bk2/2p1p2p/3q4/3PN1QP/2P3R1/P4PP1/5RK1 w - - 0 1\ngo depth 5\nquit\n' | ./stockfish
# info string prover: proven mate in 4 move g4g7 ...   bestmove g4g7
```

Altre opzioni UCI (`Prover=off|auto|mate|draw`, `ProverHash`, `ProverBudget`, `ProverMaxNodes`, `ProverProbeDepth`, `ProverLog`): [src/stockfish-hybrid/README.md](src/stockfish-hybrid/README.md). Benchmark, SPRT e G2: README delle rispettive cartelle.

## Gate (spec §11, esiti di §13)

| Gate | Esito | Documentato in |
|---|---|---|
| G0 correttezza | **superato**: 0 esiti in contrasto con gli oracoli (10^4 grafi, 10^4 posizioni ≤ 5 pezzi) | [src/wintree](src/wintree/README.md), [src/wintree-chess](src/wintree-chess/README.md) |
| G1 matti | WinTree depth-first **non superato** (64,1 %); con il nucleo df-pn **superato** (96,1 %, 0,21× i nodi del df-pn da manuale) | [src/wintree-chess](src/wintree-chess/README.md) (M2b, M2c) |
| G2 fortezze | soglia formale superata (35,1 % → 37,5 %), ma solo riduzioni a foglie TB: **ipotesi H2 non sostenuta** | [tests/g2](tests/g2/README.md) |
| G3 forza | **non superato**: −2,15 ± 2,43 Elo a 10+0.1 (rumore: il prover non parte); tuning −1,39 ± 2,02; 60+0.6 −5,79 ± 5,53 | [tests/g3](tests/g3/README.md), [tuning](tests/g3/README-tuning.md) |
| G4 costo del probe | **superato**: −0,24 % di nps, `bench` identico con `Prover=off` (da ripetere a 60+0.6, §13.1) | [src/stockfish-hybrid](src/stockfish-hybrid/README.md) |

## Licenze (spec §12.2; analisi tecnica, non consulenza legale)

- **Tutto il repository**: GPL-3.0-or-later ([`LICENSE`](LICENSE)). © 2026 Fabio Romano per il codice Java (`tests/wintree-java/`), i nuclei C++ (`src/wintree`, `src/wintree-chess`), gli hook del prover, gli script e i dati generati nel progetto.
- **Stockfish 19**: GPL-3.0-or-later, © the Stockfish developers (`Stockfish-sf_19/Copying.txt`, `AUTHORS`). `Stockfish-sf_19/` è il riferimento intatto; `src/stockfish-hybrid/` è una copia modificata (ottobre-novembre 2026): le modifiche sono gli hook elencati in [src/stockfish-hybrid/README.md](src/stockfish-hybrid/README.md) (diff da `c8c0e01`).
- **Java**: si pubblica la versione con P1 e P2 corretti (`tests/wintree-java/enginep1/`); l'originale del paper resta come baseline dei test (`tests/wintree-java/engine/`, sha256 in `run.sh`) e nella storia (commit `1103f81`).
- **Paper**: CC BY-NC-SA 4.0 su arXiv; l'autore del paper è l'autore del codice, quindi la trascrizione della Sez. 3 nell'harness non pone vincoli.
- **Rete NNUE**: non inclusa e non distribuita: la scarica chi compila (`make net`) dai server di Stockfish; la licenza del file va verificata solo per distribuire binari con la rete.
- **Dati di terzi**: `tests/suites/mates2000.epd` da [vondele/matetrack](https://github.com/vondele/matetrack) (GPL-3.0; `fetch-mates2000.sh` lo rigenera con sha256 verificato); le fortezze di `tests/g2` sono posizioni storiche prese da en.wikipedia.

## Requisiti

- g++ 16 (C++17) e GNU make; OpenJDK 25 (`java` per `tests/wintree-java` e i confronti Java-C++); Python 3.
- **Rete NNUE** `nn-1a298aa575a0.nnue` (98,5 MB): `cd Stockfish-sf_19/src && make net` → `Stockfish-sf_19/src/` (ignorata da git).
- **Syzygy 3-4-5 e 6-man** (WDL e DTZ, 150 GB per le 6-man) da <https://tablebase.lichess.ovh/tables/standard/> in `~/syzygy/5man` e `~/syzygy/6man`; uso: `setoption name SyzygyPath value ~/syzygy/5man:~/syzygy/6man` (percorsi assoluti). Scaricamento e verifica md5: [tools/README.md](tools/README.md).
- **fastchess** alpha 1.8.2 (`tools/fastchess`, compilato con `-march=native`, non portabile): comandi in [tools/README.md](tools/README.md); serve solo per `tests/g3` e gli smoke test.
