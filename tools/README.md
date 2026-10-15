# Strumenti di bootstrap (§12.1 punto 0)

Data: 2026-10-15. Tutto in questa cartella tranne questo file è ignorato da git.

## Stockfish: rete NNUE
- `Stockfish-sf_19/src/nn-1a298aa575a0.nnue` (98,5 MB), scaricata con `cd Stockfish-sf_19/src && make net`.
- sha256 verificato da `scripts/net.sh` (prefisso `1a298aa575a0` = nome file). Ignorata da git (`*.nnue`).

## fastchess
- Versione `alpha 1.8.2` (tag `v1.8.2-alpha`, commit `f618e34`), compilata con `-O3 -march=native` (binario non portabile ad altre CPU).
- Binario: `tools/fastchess`; sorgenti: `tools/fastchess-src/` (clone shallow).
- Comandi:
  ```
  git clone --depth 1 --branch v1.8.2-alpha https://github.com/Disservin/fastchess tools/fastchess-src
  make -C tools/fastchess-src -j8 && mv tools/fastchess-src/fastchess tools/fastchess
  ```

## Syzygy (sorgente: https://tablebase.lichess.ovh/tables/standard/)
| Cartella | Contenuto | Spazio | Stato |
|---|---|---|---|
| `~/syzygy/5man` | 3-4-5 WDL (145 `.rtbw`) + DTZ (145 `.rtbz`) | 939 MB | completo |
| `~/syzygy/6man` | 6-man WDL (365) + DTZ (365) | ~73 GB WDL + ~87 GB DTZ = ~160 GB attesi | **in corso** (vedi sotto) |

Comando (5-man; per 6-man sostituire `3-4-5-*` con `6-wdl`/`6-dtz`, WDL prima, poi DTZ, in background con `nohup`):
```
wget -q -r -np -nd -nH -c -e robots=off -A rtbw,rtbz https://tablebase.lichess.ovh/tables/standard/3-4-5-wdl/
```
- Velocità misurata ~7 MB/s ⇒ stima totale ~6,5 h (WDL ~3 h, poi DTZ ~3,5 h) dall'avvio (15 ott 2026 ~15:52). Disco: 372 GB liberi, bastano.
- Riprendere dopo un'interruzione: `-c` salta/continua i file; rilanciare lo stesso comando in `~/syzygy/6man`.
- Stato: `du -sh ~/syzygy/6man; cat ~/syzygy/6man/.end` (il file `.end`, timestamp epoch, compare a download concluso; `wget.log` contiene gli errori).
- Uso in Stockfish: `setoption name SyzygyPath value ~/syzygy/5man:~/syzygy/6man` (con percorsi assoluti).
- Non verificati i checksum dei file 6-man (sul sito: `sha256`/`md5` in `/tables/standard/`).
