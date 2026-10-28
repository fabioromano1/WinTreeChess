#!/bin/bash
# G4b ripetuto con il salto del probe a tabella senza PROVEN (src/stockfish-hybrid/README.md, "H3 saltato"):
# stesso blocco di ../run.sh (400 partite 60+0.6, auto con ProverMaxNodes=0 e ProverLog contro off, aperture 1001-1200).
# Uso: EXE=<binario congelato> setsid nohup bash tests/results/g3-ltc/g4b-skip/run.sh > tests/results/g3-ltc/g4b-skip/run.out 2>&1 < /dev/null &
set -u
cd "$(dirname "$0")/../../../.."
OUT=tests/results/g3-ltc/g4b-skip
ADJ="-resign movecount=3 score=600 twosided=true -draw movenumber=34 movecount=8 score=20"
echo "$(date) start"
tests/g3/sprt.sh $OUT tests/results/g3-ltc/openings-g4b.epd - - 200 60+0.6 "option.ProverMaxNodes=0 option.ProverLog=true" "$ADJ" > /dev/null 2>&1
python3 tests/g3/analyze.py $OUT/games.pgn > $OUT/analysis.md 2>&1
xz -k $OUT/games.pgn
echo "$(date) done"; touch $OUT/done
