#!/bin/bash
# M3 (e): partite di fumo con fastchess, stessa binaria con Prover=auto contro Prover=off, solo per escludere crash,
# timeout e mosse illegali (non e' G3: nessuna conclusione sulla forza). Aperture casuali da random_openings.py.
# Uso: tests/smoke_games.sh <cartella-output> [partite=200] [concorrenza=4] [tc=10+0.1] [SyzygyPath=~/syzygy/5man]
set -e
OUT=${1:?cartella di output}; GAMES=${2:-200}; CONC=${3:-4}; TC=${4:-10+0.1}; TB=${5:-$HOME/syzygy/5man}
HERE=$(cd "$(dirname "$0")" && pwd); EXE=$HERE/../stockfish; FC=$HERE/../../../tools/fastchess
mkdir -p "$OUT" && cd "$OUT"   # fastchess scrive config.json nella cartella corrente
[ -f "$OUT/openings.epd" ] || python3 "$HERE/random_openings.py" "$EXE" $((GAMES / 2)) 8 1 > "$OUT/openings.epd"
"$FC" -engine cmd="$EXE" name=prover-auto option.Prover=auto \
      -engine cmd="$EXE" name=prover-off option.Prover=off \
      -each tc="$TC" option.Hash=16 option.Threads=1 option.SyzygyPath="$TB" \
      -openings file="$OUT/openings.epd" format=epd order=sequential -repeat -rounds $((GAMES / 2)) -games 2 \
      -concurrency "$CONC" -pgnout file="$OUT/games.pgn" notation=uci timeleft=true -log file="$OUT/fastchess.log" level=warn \
      -ratinginterval 20 -report penta=true 2>&1 | tee "$OUT/fastchess.out"
echo "--- esiti anomali nel PGN (Termination diversa da normal/adjudication):"
grep -h '^\[Termination' "$OUT/games.pgn" | sort | uniq -c
