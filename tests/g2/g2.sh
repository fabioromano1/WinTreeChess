#!/bin/sh
# Esperimento G2 (spec §11, §12.2): corpus (a) a 6 pezzi, set (c) di letteratura, misure e CSV. Vedi README.md.
# Uso: tests/g2/g2.sh [sample|lit|measure|all]   Variabili: SEEDS="1 2 … 8" N=20000 JOBS=4 TIME=10 LONG=100
set -e
cd "$(dirname "$0")/../.."
BIN=src/wintree-chess/build/wintree-chess; NNUE=$PWD/Stockfish-sf_19/src/nn-1a298aa575a0.nnue
TB=$HOME/syzygy/5man:$HOME/syzygy/6man; G2=tests/g2; R=tests/results
: "${SEEDS:=1 2 3 4 5 6 7 8}" "${N:=20000}" "${JOBS:=4}" "${TIME:=10}" "${LONG:=100}"
make -C src/wintree-chess -j8 >/dev/null
step=${1:-all}

if [ "$step" = sample ] || [ "$step" = all ]; then       # corpus (a): un processo per seme, 1 s di Stockfish per patta candidata
    mkdir -p $G2/sample
    for s in $SEEDS; do $BIN --g2sample $N --seed $s --warm-ms 1000 --nnue $NNUE --tb $TB > $G2/sample/s$s.epd 2> $G2/sample/s$s.log & done; wait
    cat $G2/sample/s*.log
    { echo "# G2 set (a): posizioni casuali legali a 6 pezzi con pedoni, WDL 6-man |wdl| <= 1, |eval Stockfish 19 senza TB| > 150 cp a 1 s (prefiltro 100 ms). c0: goal = lato che Stockfish crede vincente, eval e wdl dal suo punto di vista."
      grep -hv '^info' $G2/sample/s*.epd | sort -u; } > $G2/fortress6.epd
    echo "fortress6.epd: $(grep -vc '^#' $G2/fortress6.epd) posizioni"
fi

if [ "$step" = lit ] || [ "$step" = all ]; then          # set (c): eval di Stockfish (goal) e oracolo Lichess a 7 pezzi
    $BIN --g2eval $G2/fortress-lit.epd --warm-ms 1000 --nnue $NNUE --tb $TB | grep -v '^info' > $G2/fortress-lit.tmp && mv $G2/fortress-lit.tmp $G2/fortress-lit.epd
    python3 $G2/lichess_wdl.py $G2/fortress-lit.epd $G2/lichess-cache.json
fi

if [ "$step" = measure ] || [ "$step" = all ]; then
    RUN="python3 $G2/run.py --jobs $JOBS"
    $RUN $G2/fortress6.epd   --tbmax 5 --time $TIME --csv $R/g2-a-tb5-${TIME}s.csv    --label "(a) tb5"
    $RUN $G2/fortress6.epd   --tbmax 0 --time $TIME --csv $R/g2-a-tb0-${TIME}s.csv    --label "(a) tb0"
    $RUN $G2/fortress6.epd   --tbmax 5 --time $LONG --csv $R/g2-a-tb5-${LONG}s.csv    --label "(a) tb5 lungo"
    $RUN $G2/fortress-lit.epd --tbmax 5 --time $TIME --csv $R/g2-c-tb5-${TIME}s.csv   --label "(c) tb5"
    $RUN $G2/fortress-lit.epd --tbmax 6 --time $TIME --csv $R/g2-c-tb6-${TIME}s.csv   --label "(c) tb6"
    $RUN $G2/fortress-lit.epd --tbmax 0 --time $TIME --csv $R/g2-c-tb0-${TIME}s.csv   --label "(c) tb0"
    $RUN $G2/fortress-lit.epd --tbmax 6 --time $LONG --csv $R/g2-c-tb6-${LONG}s.csv   --label "(c) tb6 lungo"
fi
