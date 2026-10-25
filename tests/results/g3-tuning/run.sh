#!/bin/bash
# G3 tuning (tests/g3/README-tuning.md): catena in background. 1) overhead del probe (G4) sul core 2, a reprosearch
# finito; 2) 400 partite 10+0.1 auto (ProverBudget=30, ProverLog=true) vs off sulle ultime 200 aperture del libro;
# 3) se auto ha <= 5 perdite per il tempo, la NR SPRT [-3, 1] fino a 20000 partite (10000 round) sullo stesso libro di G3.
set -u
cd "$(dirname "$0")/../../.."
OUT=tests/results/g3-tuning; export EXE=$PWD/$OUT/stockfish
XOPT="option.ProverBudget=30 option.ProverLog=true"
ADJ="-resign movecount=3 score=600 twosided=true -draw movenumber=34 movecount=8 score=20"
until grep -q "reprosearch testing OK\|failed" /tmp/claude-1000/-home-dev-Scrivania-WinTreeChess/95e63653-1ff7-4407-9fb5-5f4817b483c9/scratchpad/reprosearch-auto.log 2>/dev/null; do sleep 5; done
echo "$(date) overhead" >> $OUT/chain.log
src/stockfish-hybrid/tests/prover_overhead.sh /tmp/claude-1000/-home-dev-Scrivania-WinTreeChess/95e63653-1ff7-4407-9fb5-5f4817b483c9/scratchpad/ref/stockfish 10 2 > $OUT/overhead.md 2>&1
echo "$(date) pre400" >> $OUT/chain.log
tests/g3/sprt.sh $OUT/pre400 $OUT/openings-pre400.epd -3 1 200 10+0.1 "$XOPT" "$ADJ" > /dev/null 2>&1
python3 tests/g3/analyze.py $OUT/pre400/games.pgn > $OUT/pre400/analysis.md 2>&1
python3 tests/g3/prover_log.py $OUT/pre400/games.pgn $OUT/pre400/prover-starts.csv > $OUT/pre400/prover-log.md 2>&1
AUTO_TL=$(grep "^| prover-auto" $OUT/pre400/analysis.md | head -1 | awk -F'|' '{gsub(/ /, "", $(NF-1)); print $(NF-1)}')
echo "$(date) pre400 done, auto time losses: $AUTO_TL" >> $OUT/chain.log
if [ "${AUTO_TL:-99}" -le 5 ]; then
  echo "$(date) nr" >> $OUT/chain.log
  tests/g3/sprt.sh $OUT/nr tests/books/openings8-filtered.epd -3 1 10000 10+0.1 "$XOPT" "$ADJ" > /dev/null 2>&1
  python3 tests/g3/analyze.py $OUT/nr/games.pgn > $OUT/nr/analysis.md 2>&1
  python3 tests/g3/prover_log.py $OUT/nr/games.pgn $OUT/nr/prover-starts.csv > $OUT/nr/prover-log.md 2>&1
fi
echo "$(date) done" >> $OUT/chain.log; touch $OUT/chain.done
