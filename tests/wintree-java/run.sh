#!/bin/sh
# M0: verifica le copie dei sorgenti, esegue l'harness dell'Appendice A e il fuzzing su grafi casuali.
# Uso: ./run.sh [GRAFI=10000] [SEME=1]
set -e
cd "$(dirname "$0")"
# engine/*.java = sorgenti originali del paper (cartella "two-players game engine/" del commit 1103f81, tolta dal tree il 2026-11-03)
sha256sum -c <<'SUMS'
b4749232996a52919f497bbc679b8e144d63cd953ffeb8d8a37f0f0ac40b0521  engine/Engine.java
dec71e41adc47e6a7399f9ac0820f615ad9f2d76a0edb8db3c94c173904ff384  engine/Outcome.java
a9827e21e352ebaccd19a84707fcb73589dd233568554feaacd60da3acab0c9e  engine/Position.java
SUMS
java Test.java
java Fuzz.java "${1:-10000}" "${2:-1}"
