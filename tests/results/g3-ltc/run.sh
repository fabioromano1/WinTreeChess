#!/bin/bash
# G3 LTC (tests/g3/README-ltc.md): catena in background dopo G4a (g4a.done). 1) set di finali sensibile
# tests/books/endgames-hard.epd (~1 h); 2) G4b: 400 partite 60+0.6 solo probe (ProverMaxNodes=0) vs off sulle aperture
# 1001-1200 del libro di G3 (~5 h); 3) campione: auto (ProverBudget=30, ProverLog) vs off a 60+0.6 sul set, 1000 coppie
# fisse (nessun SPRT), adjudication solo a TB 5-man (esatta: entrambi hanno le 5-man alla radice) (~25 h). Fine = ltc.done.
# Uso: setsid nohup bash tests/results/g3-ltc/run.sh > tests/results/g3-ltc/run.out 2>&1 < /dev/null &
set -u
cd "$(dirname "$0")/../../.."
OUT=tests/results/g3-ltc; export EXE=$PWD/$OUT/stockfish
ADJ="-resign movecount=3 score=600 twosided=true -draw movenumber=34 movecount=8 score=20"
log() { echo "$(date) $*" >> $OUT/chain.log; }
until [ -e $OUT/g4a.done ]; do sleep 30; done
log set
xzcat tests/results/g3-books/eggen-games.pgn.xz > $OUT/eggen.pgn
python3 tests/g3/hard_endgames.py "$EXE" $OUT/eggen.pgn $OUT/endgames-candidates.epd $OUT/endgames-candidates.csv 20 > $OUT/set.out 2>&1
rm $OUT/eggen.pgn
python3 tests/g3/build_hard_book.py $OUT/endgames-candidates.csv tests/suites/mates2000.epd tests/books/endgames-hard.epd \
  tests/books/endgames-hard.csv > $OUT/book-distribution.md 2>&1
N=$(grep -vc ',M,' tests/books/endgames-hard.csv)   # righe dei finali + intestazione
[ "$N" -gt 300 ] || { log "set troppo piccolo: $((N - 1)) finali"; exit 1; }
log g4b
sed -n 1001,1200p tests/books/openings8-filtered.epd > $OUT/openings-g4b.epd
tests/g3/sprt.sh $OUT/g4b $OUT/openings-g4b.epd - - 200 60+0.6 "option.ProverMaxNodes=0 option.ProverLog=true" "$ADJ" > /dev/null 2>&1
python3 tests/g3/analyze.py $OUT/g4b/games.pgn > $OUT/g4b/analysis.md 2>&1
log ltc
tests/g3/sprt.sh $OUT/ltc tests/books/endgames-hard.epd - - 1000 60+0.6 "option.ProverBudget=30 option.ProverLog=true" \
  "-tb $HOME/syzygy/5man -tbpieces 5" > /dev/null 2>&1
python3 tests/g3/analyze.py $OUT/ltc/games.pgn > $OUT/ltc/analysis.md 2>&1
python3 tests/g3/prover_log.py $OUT/ltc/games.pgn $OUT/ltc/prover-starts.csv > $OUT/ltc/prover-log.md 2>&1
python3 tests/g3/ltc_pairs.py $OUT/ltc/games.pgn tests/books/endgames-hard.csv $OUT/ltc/pairs-odd.csv > $OUT/ltc/pairs.md 2>&1
log ltc done; touch $OUT/ltc.done
