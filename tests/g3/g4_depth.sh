#!/bin/bash
# G4 a cadenza lunga (tests/g3/README-ltc.md): nps di `bench 16 1 <depth> default depth` (51 posizioni, un thread) per
# off e probeN (Prover=auto, ProverMaxNodes=0, ProverProbeDepth=N). Due modi:
#   iso  <core>  : una configurazione alla volta sul core indicato, ordine ruotato a ogni ripetizione;
#   conc         : le 4 configurazioni insieme sui core fisici 0-3 (L3 condivisa come 4 partite in parallelo),
#                  assegnazione ruotata a ogni ripetizione (quadrato latino con 4 ripetizioni).
# Uso: tests/g3/g4_depth.sh <stockfish> <depth> <ripetizioni> iso <core> | conc   >> out.csv
# Righe CSV: mode,rep,config,core,nodes,nps,ms
EXE=$(realpath "$1"); D=$2; R=$3; MODE=$4; CORE=$5
CFGS=(off probe6 probe8 probe10)
run() {  # $1 config, $2 core
  local opt="setoption name Prover value off\n"
  [ "$1" != off ] && opt="setoption name Prover value auto\nsetoption name ProverMaxNodes value 0\nsetoption name ProverProbeDepth value ${1#probe}\n"
  printf "${opt}bench 16 1 $D default depth\nquit\n" | taskset -c "$2" "$EXE" 2>&1 |
    awk -v m="$MODE" -v r="$rep" -v c="$1" -v k="$2" '/Total time/ {t=$5} /Nodes searched/ {s=$4} /Nodes\/second/ {n=$3} END {print m "," r "," c "," k "," s "," n "," t}'
}
[ "$MODE" = conc ] || [ "$MODE" = iso ] || { echo "modo: iso <core> | conc" >&2; exit 1; }
for ((rep = 0; rep < R; rep++)); do
  if [ "$MODE" = iso ]; then
    for ((i = 0; i < 4; i++)); do run "${CFGS[(i + rep) % 4]}" "$CORE"; done
  else
    for ((i = 0; i < 4; i++)); do run "${CFGS[i]}" $(((i + rep) % 4)) & done
    wait
  fi
done
