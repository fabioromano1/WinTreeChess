#!/bin/sh
# M0: verifica le copie dei sorgenti, esegue l'harness dell'Appendice A e il fuzzing su grafi casuali.
# Uso: ./run.sh [GRAFI=10000] [SEME=1]
set -e
cd "$(dirname "$0")"
for f in Engine Outcome Position; do
  cmp "engine/$f.java" "../../two-players game engine/src/engine/$f.java"
done
sha256sum engine/*.java
java Test.java
java Fuzz.java "${1:-10000}" "${2:-1}"
