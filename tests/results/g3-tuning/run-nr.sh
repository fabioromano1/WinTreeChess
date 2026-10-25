#!/bin/bash
# G3 tuning: NR SPRT [-3, 1] a 10+0.1, auto (ProverBudget=30, ProverLog=true) vs off, fino a 20000 partite; poi analisi.
set -u
cd "$(dirname "$0")/../../.."
OUT=tests/results/g3-tuning; export EXE=$PWD/$OUT/stockfish
XOPT="option.ProverBudget=30 option.ProverLog=true"
ADJ="-resign movecount=3 score=600 twosided=true -draw movenumber=34 movecount=8 score=20"
echo "$(date) nr" >> $OUT/chain.log
tests/g3/sprt.sh $OUT/nr tests/books/openings8-filtered.epd -3 1 10000 10+0.1 "$XOPT" "$ADJ" > /dev/null 2>&1
python3 tests/g3/analyze.py $OUT/nr/games.pgn > $OUT/nr/analysis.md 2>&1
python3 tests/g3/prover_log.py $OUT/nr/games.pgn $OUT/nr/prover-starts.csv > $OUT/nr/prover-log.md 2>&1
echo "$(date) nr done" >> $OUT/chain.log; touch $OUT/nr.done
