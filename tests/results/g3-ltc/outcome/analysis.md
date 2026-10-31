# Set scelto per esito: `auto` contro `off`, 60+0.6, 500 coppie (2026-10-31)

Costruzione del set e lettura: `tests/g3/README-ltc.md` §6. Corsa: `run.sh sample`, 08:07-16:07 (8 h 00; stima prima del via
~8,7 h), binario sha256 `5d91e4d6…cad607`, libro `tests/books/endgames-outcome.epd` (133 posizioni: F 19, P 101, M 13).

**Il prover cambia l'esito dove `off` non converte? No, entro l'intervallo.** Punti di `auto` − punti di `off` col lato in
vantaggio: +0,002 per coppia [−0,010; +0,014] (bootstrap al 95 % sulle posizioni); vittorie del lato in vantaggio 40 contro 40,
differenza 0,0 % [−2,2 %; +2,4 %]. Il prover parte solo dopo un punteggio decisivo, e tutte le partite che lo raggiungono sono
state vinte: `auto` 56 su 56, `off` 54 su 54.

| Misura | Valore |
|---|---|
| Elo (logistic) | +0,69 ± 3,85 (nElo +3,88 ± 21,53) |
| Pentanomiale [0..2] di `auto` | [0, 15, 468, 17, 0] |
| V/S/P di `auto` | 56 / 54 / 890 |
| Timeout, crash | 0, 0 |
| Avvii del prover | 771 in 111 partite su 1000 (73 coppie su 500) |
| Esiti certificati | 298 in 56 partite (165 matti, 133 vittorie a foglie TB) |
| Senza esito | 473, 12,2 s in tutto, 110 ms per partita con avvio (~0,2 % della riflessione) |
| nps per partita, `auto` / `off` − 1 | −0,06 % ± 0,63 % (mediana +0,08 %) |
| nps, probe saltato (prima del primo avvio) | +0,44 % ± 0,53 % (945 partite) |
| nps, dopo un avvio e prima di una prova (probe forse attivo) | −0,23 % ± 7,47 % (34 partite, 422 mosse) |
| nps, dopo la prima prova | non misurabile: 0 mosse usabili, ogni mossa rilegge la prova ed esce (H5) |

Costo del probe con entry PROVEN in partita: **non risolto**. Il probe attivo copre al massimo l'1,3 % delle mosse di `auto`.

Dettagli: `conversion.md` (conversione per classe, 32 coppie non pari una per riga), `pairs.md` e `pairs-odd.csv` (coppie per
classe), `prover-log.md` e `prover-starts.csv` (registro del prover), `analysis-raw.md` (uscita di `analyze.py`),
`fastchess.out`, `games.pgn.xz`.
