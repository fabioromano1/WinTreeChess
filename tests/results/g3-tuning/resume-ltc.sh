#!/bin/bash
# G3 tuning: ripresa del campione 60+0.6 dopo lo spegnimento del 2026-10-24 22:37 (1182 partite giocate, config.json
# autosalvato a 1178 = round 589; le 4 partite dei round 590-591 sono state tolte dal PGN e vengono rigiocate).
set -u
cd "$(dirname "$0")/../../.."
OUT=tests/results/g3-tuning
echo "$(date) ltc resume" >> $OUT/chain.log
(cd $OUT/ltc && ../../../../tools/fastchess -config file=config.json 2>&1 | tee -a fastchess.out) > /dev/null
python3 tests/g3/analyze.py $OUT/ltc/games.pgn > $OUT/ltc/analysis.md 2>&1
python3 tests/g3/prover_log.py $OUT/ltc/games.pgn $OUT/ltc/prover-starts.csv > $OUT/ltc/prover-log.md 2>&1
echo "$(date) ltc done" >> $OUT/chain.log; touch $OUT/ltc.done
