# tests/wintree-java — M0 (spec §12.1 punto 1)

Harness Java di riferimento per WinTree. Nessun file originale del progetto è modificato.

```bash
./run.sh            # = ./run.sh 10000 1   (GRAFI, SEME): ~4 s, output identico a ogni esecuzione
```

| File | Contenuto |
|---|---|
| `engine/*.java` | copia dei tre sorgenti originali; `run.sh` verifica con `cmp` che siano identici (sha256 `Engine.java` `b4749232…0521`) |
| `enginep1/Engine.java` | copia con **P1 corretto** (§2.7), solo riferimento di test: pacchetto `enginep1`, il primo ciclo registra i figli BLACK trattati, il secondo legge l'esito di tutti gli altri (anche quelli diventati BLACK nel frattempo) senza ricorrere sui BLACK. `diff engine/Engine.java enginep1/Engine.java` = 7 righe |
| `Test.java` | harness dell'Appendice A + colonna `p1fix=` e controlli `OK/FAIL` (exit 1 se falliscono). Contiene il solver a minimo punto fisso (`forced`) e la trascrizione di Sez. 3 (`Base`) |
| `Gen.java` | generatore di grafi con seme e parser del formato testuale (con verifica di Def. 2.6) |
| `Fuzz.java` | 10^4 grafi × 2 goal; solidità, completezza alla radice (Thm 3.4), model checker del Thm 4.8 |

Formato del grafo (`java Gen.java SEME` oppure `java Gen.java SEME N GRADO CICLI SINK`): una riga `id turno esito figli...`, radice `0`, turno `1|2`, esito `.` (non-sink) / `1` / `2` / `0` (patta). `CICLI` = probabilità che un arco punti a un id minore. Distribuzione di Fuzz: n ∈ [10,40], grado ∈ [1,3], cicli ∈ [0,0.7), sink ∈ [0.1,0.4). Gli archi uniscono solo vertici di turno opposto, quindi non ci sono self-loop. Per riprodurre un caso: `java Gen.java 191`.

## Risultati (2026-10-15, OpenJDK 25.0.5-ea, `./run.sh 10000 1`)

`Test.java`: la colonna `java=` riproduce l'Appendice A. `p1fix`: **P1a e P1b risolti** (v0 e v FALSE in P1a, TRUE in P1b, uguali a `base` e `ref`), **P2 ancora presente** (v0 NIL con eval = g). Tutti i 4 controlli sono `OK`.

`Fuzz.java`: 20000 ricerche, eval(radice) = g in 4142. I valori contano le ricerche con almeno una violazione. NOWIN = radice non TRUE.

| | NOWIN | TRUE non solido | FALSE non solido | radice incompleta (3.4) | patta (4.8) |
|---|---:|---:|---:|---:|---:|
| java-orig | 15942 | 7 | 45 | 89 | 129 |
| java-p1fix | 15859 | 0 | 1 | 1 | 2 |
| sez3 | 15858 | 0 | 0 | 0 | 0 |

Esempi (seme/goal): orig TRUE non solido `2638/P1, 3804/P2, 5148/P1`; orig FALSE non solido `191/P1, 199/P1, 421/P1`; p1fix `191/P1` (tre controlli), `7058/P1` (solo 4.8).

## Cosa dicono i numeri

1. **Sez. 3 ha 0 violazioni**: oracolo, trascrizione e model checker sono coerenti con i teoremi. Se la trascrizione viola un controllo, Fuzz esce con codice 1.
2. **P1 compare nei grafi casuali**: l'originale ha TRUE e FALSE non solidi. Con la correzione spariscono tutti i TRUE non solidi e 44 FALSE su 45.
3. **Nuovo: P2 rende non solido un FALSE alla radice** (seme 191, goal P1). La radice ha l'avversario al tratto. `solve()` tratta un figlio NIL come FALSE (deviazione 3 di §2.6, giustificata dal Thm 3.4). Con il dejavu quel figlio può restare NIL pur avendo eval = g, quindi la radice diventa BLACK FALSE in modo errato. Questo smentisce, per la radice, la frase di §2.7/P2 «la solidità è preservata». Verifica: in una copia di prova senza dejavu (WHITE sempre riesplorato), `p1fix` ha 0 violazioni sui 10^4 grafi.
4. **Il Thm 4.8 cade con il dejavu**: in `7058/P1` la radice resta NIL ma il difensore può scegliere un figlio NIL con eval = g. È la conseguenza di P2 descritta in §2.7.
5. Con n ≤ 16 (prima distribuzione provata) P2 non è mai comparso in 10^4 grafi: per questo la distribuzione usa n ≥ 10.

Precisazione sul model checker (§9.2 punto 10). Da un nodo **FALSE** il difensore sceglie solo figli FALSE. Il Thm 4.8 copre solo i cammini tutti NIL, e un figlio WHITE di un nodo FALSE può avere eval = g. Con la regola «NIL o FALSE ovunque», Sez. 3 dava un falso allarme (seme 3240). La radice conta come NIL, perché il Java la marca FALSE quando un figlio è NIL.
