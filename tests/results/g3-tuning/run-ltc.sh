#!/bin/bash
# G3 tuning, attività (C) di §13: campione a 60+0.6, auto (ProverBudget=30, ProverLog=true) vs off, SPRT [-3, 1],
# 750 round = 1500 partite (stesso libro, adjudication e binario della NR); stima ~20 h con 4 partite in parallelo.
# NON avviato da questa sessione: lo avvia la sessione madre dopo la conferma del tempo-macchina.
# Uso: setsid nohup bash tests/results/g3-tuning/run-ltc.sh > tests/results/g3-tuning/chain-ltc.out 2>&1 < /dev/null &
set -u
cd "$(dirname "$0")/../../.."
OUT=tests/results/g3-tuning; export EXE=$PWD/$OUT/stockfish
XOPT="option.ProverBudget=30 option.ProverLog=true"
ADJ="-resign movecount=3 score=600 twosided=true -draw movenumber=34 movecount=8 score=20"
echo "$(date) ltc" >> $OUT/chain.log
tests/g3/sprt.sh $OUT/ltc tests/books/openings8-filtered.epd -3 1 750 60+0.6 "$XOPT" "$ADJ" > /dev/null 2>&1
python3 tests/g3/analyze.py $OUT/ltc/games.pgn > $OUT/ltc/analysis.md 2>&1
python3 tests/g3/prover_log.py $OUT/ltc/games.pgn $OUT/ltc/prover-starts.csv > $OUT/ltc/prover-log.md 2>&1
echo "$(date) ltc done" >> $OUT/chain.log; touch $OUT/ltc.done
