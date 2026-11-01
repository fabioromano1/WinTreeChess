#!/bin/bash
# G3 LTC §7 (tests/g3/README-ltc.md): trigger anticipato del prover alla radice (`ProverAdvantage=100`: parte anche con punteggio
# precedente >= +1.00 per il lato al tratto) su un libro mirato: le posizioni del set per esito (§6) in cui, nel campione
# ../outcome/, il lato in vantaggio ha raggiunto almeno +1.00 (non decisivo) in almeno una partita (tests/g3/adv_book.py):
# le sole in cui il trigger puo' partire. Stesso protocollo della fase `sample` di ../outcome/run.sh: 500 coppie a 60+0.6,
# auto (ProverBudget=30, ProverLog, ProverAdvantage=100) contro off, Hash 16, un thread, Syzygy 5-man, adjudication solo TB,
# concorrenza 4; poi gli script di analisi (CSV del set intero: le posizioni si abbinano per FEN) e games.pgn compresso.
# Uso: EXE=<binario congelato> setsid nohup bash tests/results/g3-ltc/outcome-trigger/run.sh > tests/results/g3-ltc/outcome-trigger/run.out 2>&1 < /dev/null &
# Ripresa dopo un'interruzione (spegnimento della macchina il 2026-11-01 11:36, a 201 partite): `bash run.sh resume` (stesso EXE,
# `>> run.out`). fastchess riparte dal suo config.json (stato salvato ogni 20 partite: 99 coppie); prima games.pgn viene troncato
# alle coppie salvate (scartate le 3 partite dei round 100-101), cosi' nessuna partita e' doppia e i round restano unici.
set -eu
cd "$(dirname "$0")/../../../.."
OUT=tests/results/g3-ltc/outcome-trigger; G=tests/g3; BOOK=tests/books/endgames-outcome-adv100.epd; CSV=tests/books/endgames-outcome.csv
EXE=$(realpath "${EXE:-src/stockfish-hybrid/stockfish}"); export EXE
echo "$(date) start"
if [ "${1:-}" = resume ]; then
    FC=$PWD/tools/fastchess
    (cd $OUT && sha256sum -c binary.sha256 > /dev/null && python3 - <<'EOF'
import json, re
pairs = sum(sum(v[k] for k in v if k.startswith("penta_")) for v in json.load(open("config.json"))["stats"].values())
gs = [g for g in re.split(r'(?m)^(?=\[Event )', open("games.pgn").read()) if g.strip()]
keep = [g for g in gs if int(re.search(r'\[Round "(\d+)"\]', g)[1]) <= pairs]
assert len(keep) == 2 * pairs, (len(keep), pairs)
open("games.pgn", "w").write("".join(keep)); print("ripresa dalla coppia", pairs + 1, "scartate", len(gs) - len(keep), "partite")
EOF
    "$FC" -config file=config.json 2>&1 | tee -a fastchess.out > /dev/null)
else
    (cd $G && python3 adv_book.py ../results/g3-ltc/outcome/games.pgn ../books/endgames-outcome.csv 1.00 ../../$BOOK) > $OUT/book.out
    $G/sprt.sh $OUT $BOOK - - 500 60+0.6 \
      "option.ProverBudget=30 option.ProverLog=true option.ProverAdvantage=100" "-tb $HOME/syzygy/5man -tbpieces 5" > /dev/null 2>&1
fi
python3 $G/analyze.py $OUT/games.pgn "$EXE" > $OUT/analysis-raw.md 2>&1
python3 $G/prover_log.py $OUT/games.pgn $OUT/prover-starts.csv > $OUT/prover-log.md 2>&1
(cd $G && python3 ltc_pairs.py ../../$OUT/games.pgn ../../$CSV ../../$OUT/pairs-odd.csv) > $OUT/pairs.md 2>&1
(cd $G && python3 outcome_analysis.py ../../$OUT/games.pgn ../../$CSV) > $OUT/conversion.md 2>&1
xz -f $OUT/games.pgn
echo "$(date) done"; touch $OUT/sample.done
