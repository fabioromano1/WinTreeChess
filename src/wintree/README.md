# src/wintree — M1 (spec §9.1, §12.1 punto 3)

Nucleo WinTree generico in C++17 (template sul gioco, interfaccia di §8.1), adapter a grafo esplicito e test differenziali. Nessuna dipendenza; `Stockfish-sf_19/` e i Java originali non sono toccati.

```bash
make -C src/wintree test      # ~10 s (C++ 2,6 s + Java), output riproducibile; GRAPHS=10000 SEED=1 come tests/wintree-java/run.sh
```

| File | Contenuto |
|---|---|
| `wintree.h` | `Result` (PROVEN_WIN con `d`, PROVEN_NOWIN, UNKNOWN_CYCLE con `p`, UNKNOWN_BUDGET con `resid`, flag `cut`), `Budget` (nodi, profondità, deadline, `stop`), `ProofTable` (2 parole, xor-check, mai un PROVEN sovrascritto da un UNKNOWN), `Prover<G>`: Sez. 3 + O1 corretto (il secondo ciclo legge via tabella anche i figli diventati PROVEN, bitset dei figli già letti), dejavu compatto con propagazione, controllo cooperativo ogni 1024 nodi, assert I1-I4 attivi anche in release |
| `graph.h` | `Graph::parse` (formato di `Gen.java`, verifica Def. 2.6), `GraphGame` (adapter), `JavaRandom` + `generate` (port di `Gen.java`: stessi grafi dagli stessi semi, verificato) |
| `test.cpp` | oracolo `d_g` a minimo punto fisso, trascrizione di Sez. 3, regressioni P1a/P1b/P2/Sanity, fuzz, budget casuali, stop/deadline, model checker del Thm 4.8, `--java FILE` |
| `../../tests/wintree-java/Cmp.java` | esiti del Java con P1 corretto (stati, `d`, preordine = chiamate a `legalMoves()`) per il confronto di §9.3 |

## Risultati (2026-10-16, g++ 16.2, OpenJDK 25, `make test`)

**Regressioni**: P1a `v0`,`v` NOWIN; P1b `v0`,`v` WIN; **P2 risolto** (`v0` WIN, `d`=5: la propagazione dei bloccanti ripristina il Thm 3.4 sul grafo di §2.7); Sanity: `c2` UNKNOWN_CYCLE con eval = g (completezza solo per i BLACK, come atteso).

**Fuzz**: 10^4 grafi (semi 1..10000) × 2 goal = 20000 ricerche, eval(radice) = g in 4142.

| | NOWIN | PROVEN in contrasto con l'oracolo | radice incompleta (Thm 3.4) | tagli senza budget | patta (Thm 4.8) fallita | `d` < `d_g` | max espansioni |
|---|---:|---:|---:|---:|---:|---:|---:|
| C++ | 15858 | 0 | 0 | 0 | 0 | 0 | 356 |

NOWIN = 15858 = trascrizione di Sez. 3 in M0; radici UNKNOWN_CYCLE 7238 = radici NIL di Sez. 3. Seconda chiamata a tabella piena: stesso esito, 0 espansioni. Budget casuale (nodi 1..50, profondità 1..15): 0 PROVEN errati; ogni interruzione dà radice UNKNOWN_BUDGET con `cut` (4765); le 4466 radici UNKNOWN_CYCLE hanno tutte eval ≠ g (I2: un taglio non si nasconde mai dietro un UNKNOWN_CYCLE). `stop` alzato e scadenza passata: interruzione esattamente al 1024-esimo nodo (grafo n = 3000).

**Java-C++ (§9.3)**: 20000 grafi (10^4 standard + 10^4 con cicli = 0): generatore identico; 6955 grafi aciclici con stati, `d` e preordine **identici** al Java con P1 corretto; 13045 ciclici: `win` uguali sui PROVEN comuni salvo i 2 errori del Java (seme 191, P2b: radice FALSE, C++ WIN); 6273 radici FALSE nel Java e UNKNOWN_CYCLE nel C++ (deviazione 3 di §2.6: il Java marca FALSE una radice NIL).

## Cosa è emerso (scostamenti dalla spec)

1. **Il solo "ply del bloccante più profondo" non basta per la propagazione** (seme 1672, goal 2): un padre esclude il proprio ply e deve conoscere il massimo dei bloccanti *restanti*; con il solo massimo il nodo 28 dimenticava il bloccante 17 e finiva con `p = −1` (saltato per sempre, radice incompleta). Il frame tiene quindi l'insieme dei ply dei bloccanti (bitset per ply, `(maxDepth+64)/64` parole); l'entry nella tabella resta `(p, hash)`. Argomento usato per il salto (da dimostrare, §12.1 punto 2): se `u` resta NIL con bloccanti a ply ≤ `p`, ogni albero vincente da `u` tocca `path[0..p]` oppure contiene `u` stesso (induzione sull'altezza); con lo stesso prefisso fino a `p` il salto non perde la completezza alla radice. `p = −1` significa che nessun albero vincente da `u` esiste.
2. **Il contenimento "PROVEN C++ ⊇ PROVEN Java" di §9.3 non vale**: seme 191, vertice 15. Il Java non chiude 13 (P2), prosegue con il fratello 5 e dimostra 15; il C++ chiude 4 al primo testimone. Nel test è informativo, con esempi.
3. **Costo**: con la propagazione corretta il dejavu a prefix-hash salta meno del test insiemistico del Java (salta solo con prefisso uguale fino a `p`, non "bloccanti tutti sul cammino"); sul grafo n = 3000 la ricerca completa non termina in minuti (§2.9). Da misurare in B1/B5; candidato: test insiemistico esatto sui bloccanti nel frame.
4. Hash del prefisso a 32 bit (spec: 16): i bit liberi ci sono e a 16 bit i falsi positivi comparirebbero in 10^4 grafi.

Non fatto: `rev/req` e `reversible()/rule50()` (M2), modalità `Goal` WIN/NOWIN (stessa ricerca; serve solo per i tetti di profondità degli scacchi), generazione/età nella PT (rimpiazzo: l'ultimo scrive, mai un PROVEN con un UNKNOWN), collisione forzata dell'hash (§9.2 punto 8), `key_after` (il primo ciclo di O1 fa `do_move`/`undo_move` per sondare i figli).
