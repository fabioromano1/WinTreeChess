# G3 — forza di gioco (spec §9.5, §11), 2026-10-17 → 2026-10-20

Stessa binaria per i due lati (`src/stockfish-hybrid/stockfish` di M3, sha256 `aa26126d…f47f47`, congelata in
`tests/results/g3-bin/stockfish`, ignorata da git), differenza solo nell'opzione `Prover`. fastchess 1.8.2
(`tools/`), 10+0.1, `Threads=1`, `Hash=16`, `SyzygyPath=~/syzygy/5man` per entrambi, 4 partite in parallelo (4 core
fisici + HT), coppie a colori invertiti sulla stessa apertura (`-repeat`), libro sequenziale, pentanomiale.
SPRT con α = β = 0,05, **modello `logistic`** (Elo "classico", come le ipotesi di §9.5; non nElo).
Tempo-macchina usato: ~44 h (budget ~2 giorni).

**Verdetto G3: non superato.** La non regressione a 10+0.1 fallisce (H0 accettata, −2,15 ± 2,43 Elo), lo SPRT
sui finali accetta H0 (nessun guadagno), 60+0.6 non eseguito. Per §11: "se regressione: trigger/budget da ridurre;
se nessun guadagno: il progetto resta uno strumento di analisi, non un motore più forte".

## Comandi

```bash
# libro di aperture: 32000 aperture casuali a 8 semimosse (semi 1-8), tenute se |eval| <= 150 cp a depth 12
for s in 1 2 3 4 5 6 7 8; do python3 src/stockfish-hybrid/tests/random_openings.py $EXE 4000 8 $s > rand.$s.epd & done; wait
cat rand.?.epd | awk '!s[$0]++' > rand.epd             # 32000 uniche; filtro su 8 blocchi in parallelo:
python3 tests/g3/eval_filter.py $EXE 12 150 < blocco > blocco.out   # → tests/books/openings8-filtered.epd (20084)
# finali: 2000 partite off contro off a 2+0.02 dal libro (order=random -srand 7, concorrenza 8, PGN in
# tests/results/g3-books/), una posizione per partita (4 semimosse dopo la prima con <= 10 pezzi, >= 6 pezzi),
# tenute se |eval| <= 300 cp a depth 12; le prime 500 → tests/books/endgames10.epd
python3 tests/g3/extract_endgames.py $EXE games.pgn | awk '!s[$0]++' > eg.epd       # 1580
python3 tests/g3/eval_filter.py $EXE 12 300 6 < eg.epd | head -500 > tests/books/endgames10.epd   # (821 → 500)
ADJ="-resign movecount=3 score=600 twosided=true -draw movenumber=34 movecount=8 score=20"   # adjudication fishtest
export EXE=tests/results/g3-bin/stockfish
tests/g3/sprt.sh tests/results/g3-nr         tests/books/openings8-filtered.epd -3 1 20000 10+0.1 ""                     "$ADJ"
tests/g3/sprt.sh tests/results/g3-probeonly  tests/books/openings8-filtered.epd -3 1 1500  10+0.1 option.ProverMaxNodes=0 "$ADJ"
tests/g3/sprt.sh tests/results/g3-aa         tests/books/openings8-filtered.epd -3 1 1500  10+0.1 option.Prover=off       "$ADJ"
tests/g3/sprt.sh tests/results/g3-endgames   tests/books/endgames10.epd           0 5 1500  10+0.1 ""                     ""
python3 tests/g3/analyze.py tests/results/g3-nr/games.pgn $EXE > tests/results/g3-nr/analysis.md   # idem per le altre
```

- `sprt.sh`: un solo comando fastchess (vedi il file); `match_line` mette le righe `info string prover: ...` nel
  commento della mossa del PGN (`line="..."`). Riprendere una corsa interrotta: `-config file=config.json`.
- `analyze.py`: (A) tempo per mossa, profondità, nodi, perdite per il tempo per motore; (B) partite con righe del
  prover e coppie con esito diverso dalla speculare; (C) classi (§9.5) con, se si passa il binario, la ricerca di
  ogni mossa provata con `Prover=off` a `go nodes` = nodi mediani di `prover-off` nella stessa partita.
- `pgn_games.py` (lettore PGN, self-check `python3 tests/g3/pgn_games.py`), `eval_filter.py`, `extract_endgames.py`.
- A/A: `prover-auto` riceve `Prover=auto` e poi `Prover=off` (verificato nel log del motore di fastchess: vale l'ultimo).

## Risultati SPRT (10+0.1)

| Corsa | Ipotesi | Partite | Elo (logistic) | nElo | LOS | LLR (limiti ±2,94) | Pentanomiale [0..2] | Esito | Durata | Timeout auto/off |
|---|---|---:|---|---|---:|---:|---|---|---:|---:|
| **non regressione** `auto` (default: 100‰, 10^6, depth 6) vs `off` | [−3, 1] | 13574 | **−2,15 ± 2,43** | −5,16 ± 5,84 | 4,2 % | −2,98 | [42, 1062, 4652, 1000, 31] | **H0 accettata (regressione)** | 27 h 45 | 24 / 22 |
| controllo: solo probe (`ProverMaxNodes=0`) vs `off` | [−3, 1] | 3000 | −0,58 ± 5,09 | −1,41 ± 12,43 | 41,2 % | +0,25 | [5, 233, 1030, 226, 6] | nessuno (tetto) | 6 h 10 | 2 / 7 |
| controllo A/A: `off` vs `off` | [−3, 1] | 3000 | +3,47 ± 5,32 | +8,12 ± 12,43 | 90,0 % | +2,41 | [12, 217, 1007, 257, 7] | nessuno (tetto) | 6 h 11 | 1 / 1 |
| **guadagno nei finali** `auto` vs `off`, `endgames10.epd`, senza adjudication | [0, 5] | 408 | **−3,41 ± 3,31** | −34,7 ± 33,7 | 2,2 % | −2,96 | [0, 4, 200, 0, 0] | **H0 accettata (nessun guadagno)** | 0 h 36 | 0 / 0 |

Nessun crash né mossa illegale in nessuna corsa. Patte: 68,5 % (NR), 98,0 % (finali). Log e PGN in
`tests/results/g3-*` (`fastchess.out` completo, `analysis.md`, `games.pgn.xz`; il PGN di `g3-nr`, 72 MB e 17 MB
compresso, è ignorato da git).

### Statistiche delle partite (`analysis.md`)

| Corsa | Motore | mosse | tempo/mossa medio / mediano / p99 (s) | profondità media | nodi medi | tl minimo (s) |
|---|---|---:|---|---:|---:|---:|
| NR | auto | 740761 | 0,267 / 0,157 / 1,753 | 17,35 | 91486 | −0,160 |
| NR | off | 740759 | 0,267 / 0,156 / 1,758 | 17,27 | 91719 | −0,256 |
| solo probe | auto | 165731 | 0,265 / 0,156 / 1,733 | 17,18 | 90052 | −0,005 |
| solo probe | off | 165768 | 0,265 / 0,156 / 1,751 | 17,27 | 90661 | −0,096 |
| A/A | "auto" (off) | 166306 | 0,265 / 0,156 / 1,739 | 17,19 | 90059 | −0,051 |
| A/A | off | 166320 | 0,265 / 0,156 / 1,751 | 17,25 | 89833 | 0,126 |
| finali | auto | 15839 | 0,270 / 0,198 / 1,611 | 36,41 | 194731 | 0,132 |
| finali | off | 15839 | 0,270 / 0,197 / 1,746 | 35,85 | 187064 | 0,131 |

Le perdite per il tempo (46 su 13574 nella NR, 0,34 %) sono equilibrate fra i lati e presenti anche nell'A/A:
sono dell'ambiente (desktop attivo durante le corse), non del prover. Tempi per mossa, profondità e nodi sono
indistinguibili fra i lati in tutte le corse.

### Attività del prover, coppie speculari, classi (§9.5)

| Corsa | partite con `info string prover` | righe per tipo | coppie con esito ≠ speculare (punti auto ≠ 1) | di queste con il prover | pentanomiale delle coppie con il prover |
|---|---:|---|---:|---:|---|
| NR | 10 / 13574 | 12 `proven mate`, 4 `proven win (tablebase leaves)` | 2135 / 6787 | 4 | [0, 0, 6, 4, 0] |
| finali | 24 / 408 | 13 `proven mate`, 70 `proven win (tablebase leaves)` | 4 / 204 | 0 | [0, 0, 24, 0, 0] |

Classi delle partite con righe del prover (priorità: foglie TB > matto lungo, `mate in N` con N ≥ 10 > altro);
"mossa diversa" = almeno una mossa provata diversa dalla `bestmove` di `Prover=off` sulla stessa posizione:

| Corsa | Classe | partite | V/P/S di auto | coppie [0..2] | partite con mossa diversa | V/P/S di quelle |
|---|---|---:|---|---|---:|---|
| NR | finale con TB | 3 | 3/0/0 | [0, 0, 2, 1, 0] | 1 | 1/0/0 |
| NR | altro (matti brevi) | 7 | 7/0/0 | [0, 0, 4, 3, 0] | 2 | 2/0/0 |
| finali | finale con TB | 20 | 20/0/0 | [0, 0, 20, 0, 0] | 12 | 12/0/0 |
| finali | matto lungo | 1 | 1/0/0 | [0, 0, 1, 0, 0] | 1 | 1/0/0 |
| finali | altro | 3 | 3/0/0 | [0, 0, 3, 0, 0] | 2 | 2/0/0 |

Le partite con una prova sono tutte vinte da `auto`: il prover certifica solo PROVEN_WIN(us) con l'orologio (M3,
scostamento 4). Nella NR 4 coppie su 10 con il prover finiscono 1,5 (l'avversario `off` non aveva vinto dallo
stesso lato): +2 punti su 13574 partite, un effetto reale ma ~20 volte più piccolo della perdita misurata
(−0,31 % di punteggio ≈ −42 punti). Nei finali le 24 coppie con il prover sono tutte 1-1: `off` vince comunque
dallo stesso lato, le mosse provate diverse da `off` (15 partite) sono vie di vittoria alternative.

### Corse aggiuntive sul set dei finali (2026-10-19/20, via dell'utente): budget ridotto e controllo

Stesso comando della corsa "finali" con `option.ProverBudget=30`, `option.ProverMaxNodes=200000`,
`option.ProverBudget=0` per `prover-auto` (cartelle `tests/results/g3-endgames-{b30,n2e5,b0}`), SPRT [0, 5].

| `prover-auto` | Partite | Elo | LOS | LLR | Pentanomiale | Esito | Durata | partite con il prover | coppie 0,5 / 1,5 |
|---|---:|---|---:|---:|---|---|---:|---:|---|
| default (100‰, 10^6) — corsa "finali" sopra | 408 | −3,41 ± 3,31 | 2,2 % | −2,96 | [0, 4, 200, 0, 0] | H0 | 0 h 36 | 24 | 4 / 0 |
| `ProverBudget=30` | 598 | +0,58 ± 2,55 | 67,3 % | −2,96 | [0, 2, 294, 3, 0] | H0 | 0 h 52 | 11 | 2 / 3 |
| `ProverMaxNodes=200000` | 416 | −0,84 ± 2,83 | 28,2 % | −2,96 | [0, 2, 205, 1, 0] | H0 | 0 h 36 | 25 | 2 / 1 |
| `ProverBudget=0` (nessun prover alla radice con l'orologio) | 408 | 0,00 ± 0,00 | — | −2,95 | [0, 0, 204, 0, 0] | H0 | 0 h 36 | 0 | 0 / 0 |

Nessun timeout né crash; tempi per mossa uguali fra i lati in tutte e tre (`analysis.md`). Le coppie non pari
sono poche posizioni "a moneta" che cambiano esito da una corsa all'altra: la posizione 39 (`8/8/4B3/6bR/...`)
è 0,5 con il default (`off` converte, `auto` no), 1,5 con 30‰ (`auto` vince da nero con una prova, `off` no) e
di nuovo 0,5 con 2·10^5; la 10 è 0,5 con il default e 1,5 con 30‰; la 12 è 0,5 con il default e con 2·10^5.
In tutte, chi non converte pareggia per ripetizione, 50 mosse o materiale insufficiente.

**Lettura**: le differenze fra le configurazioni sono di 1-4 coppie su ~200-300 (default 0 coppie migliori su 4
diverse: p = 0,125 al test dei segni bilaterale; 30‰ 3 su 5). Il budget ridotto va nella direzione dell'ipotesi
(meno conversioni mancate, qualche conversione in più), ma **nessuna differenza è significativa**: l'ipotesi
"il tempo perso dal prover alla radice costa Elo" resta né confermata né confutata. Su questo set, quasi tutto
patta, lo SPRT [0, 5] col modello logistic si chiude per la varianza quasi nulla (anche `ProverBudget=0`, identico
a `off` con l'orologio, accetta H0 in 408 partite con Elo esattamente 0): H0 qui significa solo "nessuna prova di
un guadagno ≥ 5 Elo", non "regressione".

## Lettura

1. **Non regressione.** La regressione è piccola e al limite (LOS 4,2 %; l'intervallo al 95 % [−4,6; +0,3]
   tocca lo zero): SPRT con α = 0,05 la dichiara, e va presa come tale. Non viene dagli esiti del prover (10 partite,
   tutte a favore) né dal probe H3 (controllo a 3000 partite: −0,6 ± 5,1, coerente con il −0,24 % di nps di G4).
   L'A/A (+3,5 ± 5,3) non mostra asimmetrie del banco, ma alla sua precisione (±5) non esclude un bias di 2 Elo.
   L'ipotesi rimasta è il **costo delle ricerche alla radice senza esito**: con l'orologio il prover parte (10 %
   dell'optimum) a ogni mossa che soddisfa un trigger (≤ 7 pezzi con TB 5-man, punteggio precedente decisivo) e,
   se non certifica, quel tempo è perso; il PGN non lo registra (si stampano solo gli esiti certificati). Non
   verificata: le corse con budget ridotto sul set dei finali (sopra) non sono abbastanza sensibili per deciderla.
2. **Finali.** Il set è poco sensibile (98 % di patte; coppie 200/204 a 1 punto). Le uniche 4 coppie non pari sono
   tutte a 0,5 e hanno la stessa forma: `off` converte una posizione vinta (matto a fine partita) e `auto`, dallo
   stesso lato, pareggia (2 per materiale insufficiente, 2 per ripetizione), senza alcuna riga del prover. È
   coerente con l'ipotesi del punto 1 (tempo perso dal prover nei finali a ≤ 7 pezzi proprio nelle conversioni),
   ma sono 4 eventi; con varianza pentanomiale quasi nulla il modello logistic chiude lo SPRT dopo 408 partite.
   Il risultato utile è: **nessuna coppia vinta da `auto` grazie al prover** con i default. Le corse aggiuntive
   (30‰, 2·10^5, 0) mostrano che quelle coppie sono posizioni a esito instabile, non un effetto misurabile.
3. **Classi (§9.5).** Il prover cambia la mossa in 18 partite su 13982 (finali con TB 13, matti 5) e non cambia
   l'esito della coppia in nessuna di quelle dei finali; nella NR le coppie migliorate sono 4. Il "vantaggio
   limitato a classi" non è dimostrato a 10+0.1.

## Verdetto G3 (§11)

| Misura di §11 | Esito | Soglia | Stato |
|---|---|---|---|
| SPRT [−3, 1] a 10+0.1 | H0 accettata, −2,15 ± 2,43 Elo (13574 partite) | non regressione superata | **non superato** |
| SPRT [−3, 1] a 60+0.6 | non eseguito (budget: ~44 h usate; 500 partite a 60+0.6 ≈ 7 h) | non regressione superata | non misurato |
| SPRT [0, 5] sui set di finali | H0 accettata con i default (−3,41 ± 3,31, 408 partite), con 30‰ (+0,58 ± 2,55, 598), con 2·10^5 (−0,84 ± 2,83, 416) | almeno un set con guadagno significativo | **non superato** |
| **G3** | | | **non superato**: trigger/budget da ridurre; nessun guadagno ⇒ per ora strumento di analisi, non motore più forte |

## Raccomandazione sui parametri

- `ProverProbeDepth = 6`: tenere (probe senza costo misurabile, qui e in G4).
- `Prover = off` come default resta corretto; `auto` con i default attuali non va usato per giocare.
- `ProverBudget`: se si vuole giocare con `auto`, **30‰** al posto di 100‰ è la scelta prudente (sul set dei
  finali è la sola configurazione con più coppie migliori che peggiori, 3 contro 2, e consuma un terzo del tempo
  alla radice), ma il dato non è significativo: va confermato con una NR a 10+0.1 con `ProverBudget=30` (~1 giorno,
  stesso comando della NR con `option.ProverBudget=30`). `ProverMaxNodes=2·10^5` non mostra differenze dal default.
- Il set `endgames10.epd` (posizioni pari, 98 % di patte) non discrimina: per misurare un guadagno serve un set
  sensibile, posizioni vinte ma difficili (es. 150-600 cp, ≤ 7 pezzi non in TB 5-man, matti lunghi da
  `mates2000`), e lo SPRT col modello `normalized` o un numero fisso di coppie invece del logistic a varianza ~0.
- Nel motore (fuori da G3): limitare il trigger "pochi pezzi" a una volta per posizione/radice già fallita,
  o far partire il prover solo con punteggio decisivo, ridurrebbe il tempo perso senza esito.

## Scostamenti dal task

1. Libro: UHO/8-moves non presente in locale (nessun download): libro riproducibile da `random_openings.py`
   (8 semimosse casuali, semi 1-8) filtrato a |eval| ≤ 150 cp (le aperture casuali grezze delle partite di fumo
   arrivavano a ±8 pedoni).
2. Adjudication fishtest (resign 3 × 600 cp a due lati, draw dalla mossa 34 con 8 × ≤ 20 cp) nella NR e nei
   controlli; nessuna nei finali. Può nascondere guadagni del prover nei finali (rende la NR più severa per `auto`).
3. Controlli dopo la regressione: **scelti dall'utente** (2026-10-18) solo probe (`ProverMaxNodes=0`) e A/A al posto
   di `ProverBudget=30` e `ProverMaxNodes=2·10^5` (il prover produce esiti in 10 partite su 13574: i due parametri
   avrebbero cambiato poco di visibile nelle partite con apertura). Tetto di 3000 partite ciascuno (budget).
   I due parametri del task sono stati provati poi sul set dei finali (sezione "Corse aggiuntive"), non nella NR.
4. Finali estratti da 2000 partite dedicate `off` contro `off` a 2+0.02 (le 200 partite di fumo davano solo 141
   posizioni); 6-10 pezzi (1 da 6, 22 da 7, 71 da 8, 183 da 9, 223 da 10). Lo SPRT si è chiuso a 408 partite, prima
   di riusare il libro (500 coppie possibili).
5. "Mossa diversa" misurata ricercando con `Prover=off` (Hash vuota, `go nodes` = nodi mediani di `off` nella
   partita), non dalla partita speculare (le due partite di una coppia divergono quasi subito per il tempo).
6. A 2+0.02 il prover non parte mai (quota < 10 ms, M3): le partite di generazione dei finali non ne dipendono.
7. La macchina si è riavviata fra la NR e i controlli: binario ricopiato da `src/stockfish-hybrid/stockfish`, stesso
   sha256 (`binary.sha256` di `g3-nr` riporta il vecchio percorso nello scratchpad).
