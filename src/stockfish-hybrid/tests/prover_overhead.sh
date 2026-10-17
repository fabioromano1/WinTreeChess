#!/bin/bash
# M3 (spec §9.4 B3, §11 G4): overhead del probe della tabella delle prove sul `bench` di Stockfish.
# Confronta, alternandoli, il riferimento (Stockfish 19 intatto), l'ibrido con Prover=off e l'ibrido con il probe
# attivo e il prover alla radice spento (Prover=auto, ProverMaxNodes=0) a varie ProverProbeDepth. Un core fisso.
# Uso: tests/prover_overhead.sh <stockfish-riferimento> [iterazioni=10] [core=2]
REF=${1:?binario di riferimento}; N=${2:-10}; CORE=${3:-2}
HYB=$(cd "$(dirname "$0")/.." && pwd)/stockfish
bench() {  # $1 binario, $2.. setoption (nome valore)
  local exe=$1; shift; local cmds=""
  while [ $# -gt 0 ]; do cmds="${cmds}setoption name $1 value $2\n"; shift 2; done
  printf "${cmds}bench\nquit\n" | taskset -c "$CORE" "$exe" 2>&1 | awk '/Nodes searched/ {s=$4} /Nodes\/second/ {n=$3} END {print s, n}'
}
declare -A sig nps
for ((i = 1; i <= N; i++)); do
  for cfg in ref off probe1 probe4 probe6 probe8; do
    case $cfg in
      ref)    out=$(bench "$REF") ;;
      off)    out=$(bench $HYB Prover off) ;;
      probe*) out=$(bench $HYB Prover auto ProverMaxNodes 0 ProverProbeDepth "${cfg#probe}") ;;
    esac
    sig[$cfg]="${sig[$cfg]} ${out% *}"; nps[$cfg]="${nps[$cfg]} ${out#* }"
  done
done
python3 - "$N" "${nps[ref]}" "${nps[off]}" "${nps[probe1]}" "${nps[probe4]}" "${nps[probe6]}" "${nps[probe8]}" "${sig[ref]}" "${sig[off]}" "${sig[probe1]}" "${sig[probe4]}" "${sig[probe6]}" "${sig[probe8]}" <<'PY'
import sys, statistics as st
n = int(sys.argv[1]); names = ["ref", "off", "probe1", "probe4", "probe6", "probe8"]
nps = {k: [int(x) for x in sys.argv[2 + i].split()] for i, k in enumerate(names)}
sig = {k: set(sys.argv[8 + i].split()) for i, k in enumerate(names)}
ref = st.median(nps["ref"])
print(f"| Configurazione | Signature | nps mediana ({n} corse) | nps max | mediana / riferimento |")
print("|---|---|---:|---:|---:|")
for k in names:
    print(f"| {k} | {'/'.join(sorted(sig[k]))} | {st.median(nps[k]):.0f} | {max(nps[k])} | {st.median(nps[k]) / ref:+.2%}".replace("+", "") + " |")
PY
