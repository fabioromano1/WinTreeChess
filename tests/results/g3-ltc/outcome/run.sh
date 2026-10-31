#!/bin/bash
# G3 LTC, set scelto per esito (tests/g3/README-ltc.md §6). Tre fasi:
#   filter: candidati (partite già giocate + matti lunghi di mates2000), filtro off contro off a 60+0.6 (2 partite a colori
#           invertiti per candidato, adjudication solo TB 5-man) in ../outcome-filter/, set tests/books/endgames-outcome.{epd,csv}
#   more:   il filtro dà 84 posizioni (< 150): 6000 nuove partite off contro off a 2+0.02 (come eggen-games: libro
#           openings8-filtered, order=random, seme 8, nessuna adjudication, concorrenza 8) in ../outcome-gen/, candidati
#           nuovi (FEN non già candidate) filtrati come sopra in ../outcome-filter2/, set ricostruito dai due filtri
#   sample: 500 coppie a 60+0.6 sul set, auto (ProverBudget=30, ProverLog) contro off, adjudication solo TB, qui.
# Uso: EXE=<binario congelato> setsid nohup bash tests/results/g3-ltc/outcome/run.sh <filter|more|sample> > tests/results/g3-ltc/outcome/run-<fase>.out 2>&1 < /dev/null &
set -eu
cd "$(dirname "$0")/../../../.."
OUT=tests/results/g3-ltc/outcome; F=tests/results/g3-ltc/outcome-filter; R=tests/results; G=tests/g3
EXE=$(realpath "${EXE:-src/stockfish-hybrid/stockfish}"); export EXE
TB="-tb $HOME/syzygy/5man -tbpieces 5"
echo "$(date) $1 start"
if [ "$1" = filter ]; then
  mkdir -p $F; sha256sum "$EXE" > $F/binary.sha256
  EGG=$(mktemp --suffix=.pgn); xz -dc $R/g3-books/eggen-games.pgn.xz > "$EGG"
  python3 $G/outcome_candidates.py "$EXE" $F/cand-games.csv $R/g3-tuning/ltc/games.pgn $R/g3-ltc/g4b/games.pgn \
    $R/g3-ltc/g4b-skip/games.pgn $R/g3-ltc/ltc/games.pgn $R/g3-nr/games.pgn $R/g3-tuning/nr/games.pgn \
    $R/g3-tuning/pre400/games.pgn $R/g3-aa/games.pgn $R/g3-probeonly/games.pgn $R/g3-endgames/games.pgn \
    $R/g3-endgames-b0/games.pgn $R/g3-endgames-b30/games.pgn $R/g3-endgames-n2e5/games.pgn "$EGG" > $F/candidates.out
  sed -i "s|$EGG|$R/g3-books/eggen-games.pgn.xz|" $F/cand-games.csv $F/candidates.out; rm "$EGG"
  python3 $G/mate_candidates.py "$EXE" tests/suites/mates2000.epd $F/cand-mates.csv >> $F/candidates.out
  (cd $G && python3 outcome_book.py candidates ../../$F/cand-games.csv ../../$F/cand-mates.csv ../../$F/candidates.epd \
    ../../$F/candidates.csv) >> $F/candidates.out
  N=$(wc -l < $F/candidates.epd)
  (cd $F && ../../../../tools/fastchess -engine cmd="$EXE" name=off-a -engine cmd="$EXE" name=off-b \
    -each tc=60+0.6 option.Prover=off option.Hash=16 option.Threads=1 option.SyzygyPath="$HOME/syzygy/5man" \
    -openings file=candidates.epd format=epd order=sequential -repeat -rounds "$N" -concurrency 4 \
    -pgnout file=games.pgn notation=uci timeleft=true nodes=true -log file=fastchess.log level=warn \
    -ratinginterval 50 -report penta=true $TB 2>&1 | tee fastchess.out > /dev/null)
  (cd $G && python3 outcome_book.py book ../../$F/candidates.csv ../../$F/games.pgn ../books/endgames-outcome.epd \
    ../books/endgames-outcome.csv) > $F/book.md
  xz -kf $F/games.pgn
elif [ "$1" = more ]; then
  GEN=tests/results/g3-ltc/outcome-gen; F2=tests/results/g3-ltc/outcome-filter2; mkdir -p $GEN $F2
  sha256sum "$EXE" > $GEN/binary.sha256; sha256sum "$EXE" > $F2/binary.sha256
  (cd $GEN && ../../../../tools/fastchess -engine cmd="$EXE" name=off-a -engine cmd="$EXE" name=off-b \
    -each tc=2+0.02 option.Prover=off option.Hash=16 option.Threads=1 option.SyzygyPath="$HOME/syzygy/5man" \
    -openings file=../../../books/openings8-filtered.epd format=epd order=random -srand 8 -repeat -rounds 3000 \
    -concurrency 8 -pgnout file=games.pgn notation=uci timeleft=true nodes=true -log file=fastchess.log level=warn \
    -ratinginterval 500 2>&1 | tail -20 > fastchess.tail)
  xz -kf $GEN/games.pgn
  python3 $G/outcome_candidates.py "$EXE" $F2/cand-games.csv $GEN/games.pgn > $F2/candidates.out
  (cd $G && python3 outcome_book.py candidates ../../$F2/cand-games.csv /dev/null ../../$F2/candidates.epd \
    ../../$F2/candidates.csv ../../$F/candidates.csv) >> $F2/candidates.out
  N=$(wc -l < $F2/candidates.epd)
  (cd $F2 && ../../../../tools/fastchess -engine cmd="$EXE" name=off-a -engine cmd="$EXE" name=off-b \
    -each tc=60+0.6 option.Prover=off option.Hash=16 option.Threads=1 option.SyzygyPath="$HOME/syzygy/5man" \
    -openings file=candidates.epd format=epd order=sequential -repeat -rounds "$N" -concurrency 4 \
    -pgnout file=games.pgn notation=uci timeleft=true nodes=true -log file=fastchess.log level=warn \
    -ratinginterval 50 -report penta=true $TB 2>&1 | tee fastchess.out > /dev/null)
  xz -kf $F2/games.pgn
  T=$(mktemp -d); cat $F/candidates.csv > $T/c.csv; tail -n +2 $F2/candidates.csv >> $T/c.csv; cat $F/games.pgn $F2/games.pgn > $T/g.pgn
  (cd $G && python3 outcome_book.py book $T/c.csv $T/g.pgn ../books/endgames-outcome.epd ../books/endgames-outcome.csv) > $F2/book.md
  rm -r $T
elif [ "$1" = sample ]; then
  $G/sprt.sh $OUT tests/books/endgames-outcome.epd - - 500 60+0.6 "option.ProverBudget=30 option.ProverLog=true" "$TB" > /dev/null 2>&1
  python3 $G/analyze.py $OUT/games.pgn "$EXE" > $OUT/analysis-raw.md 2>&1
  python3 $G/prover_log.py $OUT/games.pgn $OUT/prover-starts.csv > $OUT/prover-log.md 2>&1
  (cd $G && python3 ltc_pairs.py ../../$OUT/games.pgn ../books/endgames-outcome.csv ../../$OUT/pairs-odd.csv) > $OUT/pairs.md 2>&1
  (cd $G && python3 outcome_analysis.py ../../$OUT/games.pgn ../books/endgames-outcome.csv) > $OUT/conversion.md 2>&1
  xz -kf $OUT/games.pgn
fi
echo "$(date) $1 done"; touch $OUT/$1.done
