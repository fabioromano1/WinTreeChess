# tests/g2 — esperimento G2 (spec §11, §12.2): certificazione delle patte nelle fortezze

Data: 2026-10-17. Prover: nucleo df-pn di M2c (`src/wintree-chess/build/wintree-chess`, `--mode nowin`, un thread, 256 MiB di tabella,
`--verify`). **Certificata** = esito `PROVEN_NOWIN` alla radice, oppure radice `UNKNOWN_CYCLE` (ricerca completa, confutazione tainted)
con grafo di strategia di patta chiuso dal verificatore di §7.9.2. Il goal è sempre il lato che Stockfish crede vincente; il prover
deve dimostrare che quel lato **non** vince. Con `--tbmax 5` le posizioni a ≤ 5 pezzi sono sink TB (§7.6): il prover non può
consultare le 6-man, deve dimostrare.

## Corpus

### (a) `fortress6.epd` — 168 posizioni a 6 pezzi (obiettivo ≥ 100)
Costruzione (`wintree-chess --g2sample`, `main.cpp`): posizioni casuali legali a 6 pezzi (re compresi) con almeno un pedone, tipi e
colori casuali, lato al tratto casuale, contatore 0, niente arrocco né en passant, senza stallo/matto alla radice; filtro 1: WDL
6-man locale con |wdl| ≤ 1 (patta regolamentare per entrambi i lati); filtro 2: Stockfish 19 in-process (`SfEngine`, rete
`nn-1a298aa575a0.nnue`, un thread, Hash 16 MiB, tabella azzerata, **`SyzygyProbeLimit 0`**: nessuna TB) `go movetime 1000` con
|eval| > 150 cp per il lato al tratto; prefiltro a 100 ms (scartate le patte già entro 100 cp a 100 ms: 0 su 114 patte del pilota
superavano 150 cp a 1 s). 8 processi (semi 1-8) × 20 000 campioni, ~55 min:

| Seme | Campioni | Patte WDL (|wdl| ≤ 1) | Tenute (|eval| > 150 cp a 1 s) |
|---|---|---|---|
| 1-8 (totale) | 160 000 | 27 526 (17,2 %) | **168** (0,61 % delle patte) |

Riga EPD: `<fen> c0 "goal=<lato> eval=<cp> wdl=<n>";` con eval e WDL dal punto di vista del goal (il lato che Stockfish crede
vincente). Log per seme in `sample/s*.log`, righe grezze in `sample/s*.epd`.

Composizione: goal bianco 90 / nero 78; eval 151-513 cp (mediana 232; 41 oltre 300; nessun punteggio di matto); WDL(goal) = 0 in 166,
+1 (vittoria maledetta dalle 50 mosse) in 2. **139 posizioni su 168 (83 %) hanno ≥ 2 alfieri del goal sullo stesso colore**
(KBBB v KP 58, KBBP v KB 19, KBBP v KN 19, KBBP v KP/KR 18, …): la rete NNUE non sa che alfieri dello stesso colore non mattano
(il campionamento uniforme produce coppie di alfieri dello stesso colore nella metà dei casi); 31 materiali distinti; le altre 29
sono fortezze "classiche" (KQ v KRNP, KRN v KBP, KNP v KRP, KBP v KRP, KQP v KQR, …).

### (c) `fortress-lit.epd` — 23 fortezze note a 7-10 pezzi + 1 controllo
FEN trascritti dai diagrammi (template `Chess diagram`) di en.wikipedia (Fortress (chess), Opposite-colored bishops endgame,
Wrong rook pawn, Endgame study, Two knights endgame); riferimento in `c1`, nota e tratto in `c3`; le fortezze con > 10 pezzi
(fortezze di pedoni, 11-28 pezzi) e quelle a ≤ 6 sono escluse. Lato al tratto: quello della didascalia; se assente, l'attaccante.
`c0` da `wintree-chess --g2eval` (stesso Stockfish senza TB a 1 s): goal = lato che Stockfish crede in vantaggio, eval dal suo punto
di vista. Per le 6 posizioni a 7 pezzi `c2` = oracolo esterno (API tablebase di Lichess, sola lettura, una richiesta per posizione,
cache `lichess-cache.json`; autorizzato in §12.2) e `wdl` riscritto di conseguenza: 5 `draw`, 1 `blessed-loss` (Kling-Horwitz:
il Nero matta in 193, patta per le 50 mosse ⇒ wdl(goal) = +1). Controllo: Salov-Korchnoi 1997 (vinta per il Nero secondo le TB
citate da en.wiki, tenuta in partita): il prover **non** deve certificarla.

Scoperta collaterale: Stockfish 19 a 1 s valuta già entro ±120 cp 20 delle 23 fortezze di letteratura (eval mediana 38 cp);
solo Simkhovich 1926 (380 e 216 cp) e Ķeņģis-Kasparov (341 cp) superano i 150 cp della definizione di G2. Il set (c) misura quindi
"il prover certifica le fortezze note", non "dove Stockfish sbaglia".

## Comandi
```bash
make -C src/wintree-chess -j8
tests/g2/g2.sh sample    # corpus (a): SEEDS="1 … 8" N=20000, ~55 min su 8 core → fortress6.epd
tests/g2/g2.sh lit       # set (c): --g2eval (goal/eval) + lichess_wdl.py (WDL a 7 pezzi)
tests/g2/g2.sh measure   # tutte le corse sotto, JOBS=4 TIME=10 LONG=100 → tests/results/g2-*.csv
python3 tests/g2/run.py tests/g2/fortress6.epd --tbmax 5 --time 10 --csv tests/results/g2-a-tb5-10s.csv   # una corsa
python3 tests/g2/report.py verdict tests/results/g2-a-*.csv                                               # tabelle del README
src/wintree-chess/build/wintree-chess --fen "<fen>" --goal white --mode nowin --tb ~/syzygy/5man:~/syzygy/6man --tbmax 5 --time 10 --hash 256 --verify
```
`run.py`: una ricerca per posizione (`--jobs 4` posizioni in parallelo, ogni ricerca a un thread), CSV con esito, `req`/`resid`,
espansioni, `do_move`, tempo e CPU della ricerca, ply massimo, esito e motivo del verificatore, dimensione del grafo, causa
classificata. `--count N`/`--step K` scelgono un sottoinsieme.

## Risultati (2026-10-17, binario da HEAD 98fb9e3: nucleo df-pn di M2c con `tbLeaf` e `CHECK_EVERY 64` di M3; 4 posizioni in parallelo, un thread ciascuna)

Condizioni: sulla stessa macchina girava lo SPRT di G3 (8 processi `stockfish-g3`, ~4 core): la colonna CPU/tempo dà la frazione
di CPU ottenuta dalla ricerca nei 10 s di parete (mediana 0,92); l'hyper-threading condiviso riduce inoltre i nodi/s per thread.
La corsa a 100 s su tutto il set (a) dà per ogni certificata il tempo di CPU effettivo: "certificate entro 10 s di CPU" è la
misura insensibile alla contesa.

### Tabella del verdetto
| Corsa | Posizioni | Certificate | PROVEN_NOWIN | UNKNOWN_CYCLE + grafo | Espansioni mediane (cert.) | Tempo mediano (cert.) | CPU/tempo mediano | Budget | Sink 50 mosse | Entry sostituita | Taint non chiuso | Altro |
|---|---|---|---|---|---|---|---|---|---|---|---|---|
| `g2-a-tb5-10s.csv` | 168 | **59 (35.1 %)** | 25 | 34 | 783 | 0.02 s | 0.92 | 103 | 0 | 0 | 6 | 0 |
| `g2-a-tb0-10s.csv` | 168 | **0 (0.0 %)** | 0 | 0 | nan | nan s | 0.93 | 168 | 0 | 0 | 0 | 0 |
| `g2-a-tb5-100s.csv` | 168 | **59 (35.1 %)** | 25 | 34 | 783 | 0.02 s | 0.93 | 93 | 0 | 0 | 16 | 0 |
| `g2-c-tb5-10s.csv` | 24 | **1 (4.2 %)** | 1 | 0 | 760 | 0.06 s | 0.92 | 23 | 0 | 0 | 0 | 0 |
| `g2-c-tb6-10s.csv` | 24 | **2 (8.3 %)** | 1 | 1 | 3162 | 0.09 s | 0.74 | 22 | 0 | 0 | 0 | 0 |
| `g2-c-tb0-10s.csv` | 24 | **0 (0.0 %)** | 0 | 0 | nan | nan s | 0.90 | 24 | 0 | 0 | 0 | 0 |
| `g2-c-tb6-100s.csv` | 24 | **2 (8.3 %)** | 1 | 1 | 3162 | 0.05 s | 0.86 | 20 | 0 | 0 | 2 | 0 |

Set (c): 24 righe = 23 fortezze + 1 controllo; le frazioni sul solo set di fortezze sono 1/23, 2/23, 0/23, 2/23.
**Certificate entro 10 s di CPU** (dalla corsa a 100 s su tutto il set (a), `report.py cpu … 10`): **59/168 = 35,1 %**
(CPU massima di una certificata 2,2 s): la contesa di CPU non ha cambiato il verdetto.

### Verdetto G2 (§11)
- **Set (a), tb ≤ 5 pezzi, 10 s/1 thread: 59/168 = 35,1 % certificate ≥ 25 %: soglia superata.** 25 `PROVEN_NOWIN` + 34 radici
  `UNKNOWN_CYCLE` con grafo di strategia chiuso. Nessun `PROVEN_WIN` (nessun disaccordo con le WDL 6-man: coerente con G0).
- Le certificate sono **immediate**: tempo mediano 0,02 s, massimo 2,4 s; espansioni mediane 783 (max 85 864); grafo mediano
  104 posizioni (max 14 881). Nessuna certificata usa un `PROVEN_NOWIN` con `req` > 0.
- **Tutte le 59 certificate chiudono su foglie TB a 5 pezzi** (0 grafi senza foglie TB); 17 hanno un grafo di 2 nodi (il difensore
  cattura subito e la posizione a 5 pezzi è patta per le TB). **Senza TB (tb0): 0/168.** Il prover certifica dove la patta si
  riduce a ≤ 5 pezzi entro poche mosse del difensore, non le fortezze "stabili" a 6 pezzi, il cui grafo chiuso è l'intero spazio
  raggiungibile con l'attaccante libero di muovere (ordine 10^5-10^6 posizioni per KBBB v KP).
- Le 109 non certificate: 103 per budget (mediana 442 k espansioni e 13,8 M `do_move` in 10 s, ply massimo mediano 26) e 6 radici
  `UNKNOWN_CYCLE` con grafo non chiuso (sotto). Nessun caso "sink per 50 mosse/ripetizione" né "entry sostituita" (tabella 256 MiB).
  Budget 10× (100 s, sotto): le stesse 59 certificate; le 93 budget arrivano a 4,6 M espansioni e 143 M `do_move` mediani (93 s di CPU, ply massimo mediano 53)
  senza esito; 16 radici diventano `UNKNOWN_CYCLE` senza grafo chiuso: la ricerca si completa, ma la strategia
  non è ricostruibile dalla tabella.
- Per materiale (certificate/posizioni): KBBB v KP 16/58, KBBP v KB 4/19, KBBP v KN 7/19, KBBP v KR 8/9, KBB v KQP 5/5, KQBB v KP 3/3,
  KQ v KBBP 0/5, KNNP v KN 0/3; famiglia "≥ 2 alfieri del goal sullo stesso colore" 53/139, altre 6/29. Eval mediana di Stockfish
  166 cp nelle certificate contro 253 nelle non certificate (oltre 300 cp: 6/41 certificate). Le 2 con WDL(goal) = +1 (vittoria
  maledetta) sono entrambe budget: per costruzione non ammettono un grafo indipendente dal contatore (§7.9.2).
- **Lettura**: H2 regge nella forma debole (il prover certifica patte che Stockfish valuta > 150 cp e che nessun motore produce
  come certificato), ma la capacità è "riduzione a sink TB + chiusura locale", non "fortezza". La soglia del 25 % era dichiarata
  iniziale in §11: con questo corpus (83 % alfieri dello stesso colore, artefatto del campionamento uniforme) va riletta sulle famiglie.

### Le 6 radici `UNKNOWN_CYCLE` con grafo non chiuso
Il verificatore si ferma su un nodo del difensore con entry `UNKNOWN_CYCLE` e nessun figlio con entry `PROVEN_NOWIN`/`UNKNOWN_CYCLE`
(messaggio esteso in `verifier.h`: "nessuna mossa del difensore (entry …)"). Ri-risolti da soli, i 6 nodi sono tutti patte certificate in
≤ 1,2 s (4 `UNKNOWN_CYCLE` con grafo chiuso, 2 `PROVEN_NOWIN`): non è GHI che nasconde una vittoria, è l'entry del figlio sovrascritta
da una ri-ricerca in un altro contesto di cammino (la confutazione tainted non è memorizzabile come confutazione: "l'ultimo scrive"
la rimpiazza con (pn, dn) aperti). Tutti e 6 i nodi hanno una donna appena promossa dal difensore. Rimedio possibile (non fatto):
nel verificatore, ri-cercare localmente il nodo che fallisce con budget piccolo prima di rinunciare.

**Esito (2026-10-18, ri-ricerca locale nel verificatore; `src/wintree-chess/README.md`, scostamento 16).** La diagnosi "entry sovrascritta"
era sbagliata: contate le scritture di un'entry aperta sopra un'`UNKNOWN_CYCLE` della stessa chiave, 0 su tutte e 16 le radici a 100 s; al nodo
bloccato i figli con entry sono aperti e non sono antenati nel grafo, perché la confutazione del nodo era la ripetizione di un antenato del
*cammino di ricerca*, abbandonato aperto dal suo padre. Rimedio adottato: il verificatore ri-cerca il nodo bloccato dalla stessa tabella
(`DfPn::solve(b, retaint = true)`, 10^6 `do_move`, al più 64 nodi distinti; riavvio dalla radice se il nodo resta senza mossa) e la chiusura
riprende. Corsa ripetuta a 10 s (`g2-a-tb5-10s-v2.csv`, colonna `researches`, stessa contesa dello SPRT di G3, CPU/tempo mediano 0,89):
**certificate 63/168 = 37,5 %** (prima 59), nessuna persa, +4 (id 3, 13, 80, 131: 16, 9, 1 e 4 ri-ricerche); 1 "taint senza grafo chiuso" residua
(id 121: il nodo bloccato risulta `PROVEN_WIN`, la scelta a monte andrebbe rifatta ri-cercando il padre, non fatto); id 128 esce in budget a
10,0 s invece di `UNKNOWN_CYCLE` a 9,6 s (contesa). Sulle 16 radici a 100 s: 10/16 certificate (3, 13, 29, 80, 113, 128, 131, 149, 153, 157,
ri-ricerche 1-47, costo ≤ 1,5 s); restano 121 (sopra), 34 (entry evicted: 3·10^6 espansioni in 256 MiB) e 49, 54, 133, 144 (nodi che non si
risolvono entro il budget; 49 è `UNKNOWN_BUDGET` anche da solo a 10 s). Il verdetto di G2 non cambia (35,1 % → 37,5 %, sempre riduzione a foglie TB).

### Set (c) — fortezze di letteratura (23 + 1 controllo)
| # | Posizione | Pezzi | Goal | Eval SF | WDL | tb5-10s | tb6-10s | tb0-10s | tb6-100s |
|---|---|---|---|---|---|---|---|---|---|
| 1 | `6k1/2r5/3pK2Q/4b1P1/8/8/8/8 w - -` | 7 | white | 35 | 0 | budget | budget | budget | budget |
| 2 | `8/8/8/3k2p1/4p3/2b1P1pP/4KNP1/8 w - -` | 10 | black | 73 | ? | budget | budget | budget | budget |
| 3 | `8/8/8/4b1p1/8/6pP/6P1/4k2K b - -` | 7 | black | 82 | 0 | budget | budget | budget | taint alla radice senza graf |
| 4 | `8/2k5/4Bp2/2b1p1p1/4K2p/7P/8/8 b - -` | 9 | black | 108 | ? | budget | budget | budget | budget |
| 5 | `8/5pK1/4k3/6B1/5PbP/6P1/8/8 b - -` | 8 | white | 25 | ? | budget | budget | budget | budget |
| 6 | `6k1/1R5p/4K1p1/8/8/2b3P1/7P/8 b - -` | 8 | white | 38 | ? | budget | budget | budget | budget |
| 7 | `5K2/k7/8/8/N7/8/5PP1/2b4b w - -` | 7 | black | 31 | 1 | budget | budget | budget | taint alla radice senza graf |
| 8 | `4k3/4n3/2r5/2P1Q1PK/8/8/8/8 w - -` | 7 | white | 97 | 0 | budget | budget | budget | budget |
| 9 | `4K3/2k1Bp1N/6p1/5PP1/8/7p/b7/8 w - -` | 10 | black | 380 | ? | budget | budget | budget | budget |
| 10 | `8/3k1pKN/5PpB/6P1/4q3/8/b7/8 b - -` | 10 | black | 216 | ? | budget | budget | budget | budget |
| 11 | `8/3k4/p2P4/2P4p/2bB4/P6P/5K2/8 w - -` | 10 | white | 8 | ? | budget | budget | budget | budget |
| 12 | `7b/4k2P/6K1/2p2P2/7P/1B6/8/8 b - -` | 8 | white | 29 | ? | budget | budget | budget | budget |
| 13 | `4k2b/7P/5PK1/7P/8/1B6/8/8 w - -` | 7 | white | 118 | 0 | budget | budget | budget | budget |
| 14 | `3b4/p4B1p/8/6k1/6P1/8/1P3PK1/8 w - -` | 9 | white | 0 | ? | budget | budget | budget | budget |
| 15 | `7k/8/5b1p/1B5P/8/3P2P1/7P/7K b - -` | 9 | white | 39 | ? | budget | budget | budget | budget |
| 16 | `6B1/4b3/7p/3Pk2P/6PP/7K/8/8 w - -` | 9 | white | 8 | ? | budget | budget | budget | budget |
| 17 | `8/8/p7/4Bp2/4b1k1/1P1pK3/8/8 b - -` | 8 | black | 341 | ? | budget | budget | budget | budget |
| 18 | `8/5b2/3k2p1/pB3p2/P2K1P1P/8/8/8 b - -` | 10 | white | 3 | ? | budget | budget | budget | budget |
| 19 | `8/3k4/3b4/1K6/P6p/5B1P/1P6/8 b - -` | 8 | black | 0 | ? | budget | budget | budget | budget |
| 20 | `r7/8/8/5bk1/8/5B2/5RPP/6K1 b - -` | 8 | white | 113 | ? | **cert.** PROVEN_NOWIN 0.062 s | **cert.** PROVEN_NOWIN 0.007 s | budget | **cert.** PROVEN_NOWIN 0.005 s |
| 21 | `8/7k/3b1K1B/8/p7/p7/R7/1b6 w - -` | 8 | black | 49 | ? | budget | budget | budget | budget |
| 22 | `8/7k/8/8/2bBK3/p7/pb6/6R1 b - -` | 8 | black | 4 | ? | budget | budget | budget | budget |
| 23 | `8/8/2k2NnP/5Nn1/3K4/8/8/8 b - -` | 7 | white | 6 | 0 | budget | **cert.** UNKNOWN_CYCLE 0.174 s | budget | **cert.** UNKNOWN_CYCLE 0.104 s |
| 24 | `8/5p1k/7p/5R2/6P1/5P2/8/4q1K1 w - -` | 8 | black | 175 | ? | budget | budget | budget | budget |

Riga 24 = controllo (Salov-Korchnoi). Riferimenti e note nelle colonne `c1`/`c3` di `fortress-lit.epd`.

- **tb ≤ 5, 10 s: 1/23 (4,3 %)**; **tb ≤ 6, 10 s: 2/23 (8,7 %)**; tb ≤ 6, 100 s: 2/23 (le stesse); senza TB: 0/23. Il controllo non è
  mai certificato (budget), come deve.
- Le due certificate sono di nuovo riduzioni a sink: Greco (riga 20: il Nero sacrifica e la posizione a 5 pezzi è patta per le TB,
  grafo di 9 posizioni) e Karpov-Korchnoi 1981 (riga 23, 7 pezzi: i cavalli si scambiano e si chiude su 75 foglie 6-man; a tb5 è
  budget). Con `--tbmax 6` il prover ha un sink in più e certifica una posizione in più: la capacità cresce di un pezzo per ogni
  pezzo di TB, non per la ricerca.
- A 100 s con tb ≤ 6 le 20 budget arrivano a 4,3 M espansioni mediane (85 s di CPU, ply massimo mediano 44) senza esito; due radici
  diventano `UNKNOWN_CYCLE` senza grafo chiuso: Serper-Nakamura finale (riga 3: figlio con entry aperta, lo stesso meccanismo dei 6
  casi del set (a)) e Kling-Horwitz (riga 7: WDL(goal) = +1, il Nero matta in 193: una strategia di patta indipendente dal contatore
  **non esiste**, il verificatore fa bene a rifiutarla; la patta regolamentare passa solo da un `PROVEN_NOWIN` con `req`, cioè da
  una ricerca con il contatore nello stato, fuori portata).
- Le fortezze "da manuale" (Simkhovich, Kasparyan, alfieri di colore contrario con 3-4 pedoni, Benko-Bonin a 7 pezzi, la fortezza
  f7-g7-h7) restano tutte budget a 10 e a 100 s: lo spazio chiuso che l'attaccante può esplorare è troppo grande per un grafo esplicito.

### Variante "niente sink di patta per 50 mosse in NOWIN"
Non esposta dal nucleo (README di `src/wintree-chess`, "Non fatto"), quindi non misurata. Qui sarebbe comunque ininfluente: nessuna
non certificata del set (a) dipende da un sink per 50 mosse o ripetizione (0 casi "entry PROVEN_NOWIN senza mossa"): in df-pn le
confutazioni che toccano una ripetizione sono tainted e passano dal grafo, non dal sink.

## Scostamenti e note
1. **Prefiltro a 100 ms** nel campionatore: la definizione resta "|eval| > 150 cp a 1 s", ma una patta che a 100 ms sta entro 100 cp
   non viene rivalutata a 1 s (perdita di richiamo, non di validità). Pilota senza prefiltro: 600 campioni, 114 patte, 0 tenute, 1 s/patta.
2. **Corpus (a) dominato dagli alfieri dello stesso colore** (139/168): è ciò che il campionamento uniforme con oracolo produce;
   le 29 posizioni restanti sono il sottoinsieme "fortezze" in senso proprio (6/29 certificate).
3. **CPU condivisa** con lo SPRT di G3 durante tutte le misure (vedi sopra); i numeri a 10 s sono quindi prudenti.
4. **Esposizione di Stockfish in-process**: `SfEngine::cp()` (centipawn dell'ultima `info`, matto = ±10 000) in `warm.h`;
   `SyzygyProbeLimit 0` impostato solo nel campionatore (le TB restano caricate per il WDL e per il prover).
5. **Verificatore**: il motivo "nessuna mossa del difensore" riporta lo stato dell'entry (assente / PROVEN_NOWIN / UNKNOWN_CYCLE /
   UNKNOWN_BUDGET), per classificare le cause; prefisso invariato (G0 lo usa). Regressioni e G0 10^3 rieseguiti: tutto OK.
6. Le modifiche a `main.cpp`, `verifier.h`, `warm.h` sono finite nel commit M3 `98fb9e3` (albero condiviso); `run.py` passa
   `--nodes 10^9` perché il budget predefinito del binario (10^6 espansioni ≈ 20 s di CPU) tagliava le corse a 100 s.
7. Set (c): solo en.wikipedia come fonte (de/fr/es sono stub, it è una traduzione); Salov-Korchnoi trascritta con il Bianco al tratto
   (con il Nero al tratto il Re bianco sarebbe in presa); nessun oracolo oltre i 7 pezzi.
