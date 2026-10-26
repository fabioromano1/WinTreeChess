#!/bin/bash
# G3 (spec §9.5): SPRT fastchess con la stessa binaria, Prover=auto (+ opzioni extra) contro Prover=off; coppie a colori
# invertiti sullo stesso libro sequenziale, un thread, Hash 16, Syzygy 5-man per entrambi, 4 partite in parallelo.
# Uso: tests/g3/sprt.sh <out> <libro.epd> <elo0> <elo1> <round> [tc=10+0.1] ["opzioni extra di prover-auto"] ["flag fastchess extra"]
# Esempio: tests/g3/sprt.sh tests/results/g3-nr tests/books/openings8-filtered.epd -3 1 20000 10+0.1 "" "-resign movecount=3 score=600"
# elo0 = "-": nessun SPRT, numero fisso di round (G3 LTC).
# EXE=<binario> per usare una copia congelata (default src/stockfish-hybrid/stockfish).
set -e
OUT=$(realpath -m "$1"); BOOK=$(realpath "$2"); E0=$3; E1=$4; ROUNDS=$5; TC=${6:-10+0.1}; XOPT=$7; XFC=$8
HERE=$(cd "$(dirname "$0")" && pwd); EXE=$(realpath "${EXE:-$HERE/../../src/stockfish-hybrid/stockfish}"); FC=$HERE/../../tools/fastchess
SPRT="-sprt elo0=$E0 elo1=$E1 alpha=0.05 beta=0.05 model=logistic"; [ "$E0" = - ] && SPRT=""
mkdir -p "$OUT" && cd "$OUT"   # fastchess scrive config.json nella cartella corrente
sha256sum "$EXE" > binary.sha256
# shellcheck disable=SC2086   # XOPT, XFC e SPRT vanno spezzati in parole
"$FC" -engine cmd="$EXE" name=prover-auto option.Prover=auto $XOPT \
      -engine cmd="$EXE" name=prover-off option.Prover=off \
      -each tc="$TC" option.Hash=16 option.Threads=1 option.SyzygyPath="$HOME/syzygy/5man" \
      -openings file="$BOOK" format=epd order=sequential -repeat -rounds "$ROUNDS" -concurrency 4 \
      $SPRT \
      -pgnout file=games.pgn notation=uci timeleft=true nodes=true "match_line=info string prover.*" \
      -log file=fastchess.log level=warn -ratinginterval 100 -report penta=true $XFC 2>&1 | tee fastchess.out
