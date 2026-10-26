#!/bin/bash
# G3 LTC (tests/g3/README-ltc.md): campione LTC (blocco 3 di run.sh), avviato a mano sul via dell'utente (2026-10-26).
# Uso: setsid nohup bash tests/results/g3-ltc/run-ltc.sh > tests/results/g3-ltc/run-ltc.out 2>&1 < /dev/null &
set -u
cd "$(dirname "$0")/../../.."
OUT=tests/results/g3-ltc; export EXE=$PWD/$OUT/stockfish
log() { echo "$(date) $*" >> $OUT/chain.log; }
log ltc
tests/g3/sprt.sh $OUT/ltc tests/books/endgames-hard.epd - - 1000 60+0.6 "option.ProverBudget=30 option.ProverLog=true" \
  "-tb $HOME/syzygy/5man -tbpieces 5" > /dev/null 2>&1
python3 tests/g3/analyze.py $OUT/ltc/games.pgn > $OUT/ltc/analysis.md 2>&1
python3 tests/g3/prover_log.py $OUT/ltc/games.pgn $OUT/ltc/prover-starts.csv > $OUT/ltc/prover-log.md 2>&1
python3 tests/g3/ltc_pairs.py $OUT/ltc/games.pgn tests/books/endgames-hard.csv $OUT/ltc/pairs-odd.csv > $OUT/ltc/pairs.md 2>&1
log ltc done; touch $OUT/ltc.done
