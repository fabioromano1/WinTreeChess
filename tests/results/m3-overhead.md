# M3 — B3/G4: overhead del probe della tabella delle prove sul `bench` (2026-10-17)

Prodotto da `src/stockfish-hybrid/tests/prover_overhead.sh <riferimento> 10 2`: riferimento = Stockfish 19 intatto compilato con `make ARCH=x86-64-bmi2 all` da una copia temporanea; `off` = ibrido con `Prover=off`; `probeN` = ibrido con `Prover=auto`, `ProverMaxNodes=0` (solo probe H3, tabella vuota) e `ProverProbeDepth=N`. 10 corse alternate per configurazione su un core fisso (`taskset -c 2`). Default adottato: `ProverProbeDepth=6`.

| Configurazione | Signature | nps mediana (10 corse) | nps max | mediana / riferimento |
|---|---|---:|---:|---:|
| ref | 2497913 | 737500 | 753745 | 100.00% |
| off | 2497913 | 738266 | 749448 | 100.10% |
| probe1 | 2497913 | 719030 | 725294 | 97.50% |
| probe4 | 2497913 | 727830 | 737717 | 98.69% |
| probe6 | 2497913 | 735762 | 741660 | 99.76% |
| probe8 | 2497913 | 735438 | 742321 | 99.72% |
