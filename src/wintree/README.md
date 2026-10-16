# src/wintree — M1 (spec §9.1, §12.1 punto 3)

Nucleo WinTree generico in C++17 (template sul gioco, interfaccia di §8.1), adapter a grafo esplicito e test differenziali. Nessuna dipendenza; `Stockfish-sf_19/` e i Java originali non sono toccati.

```bash
make -C src/wintree test      # ~10 s (C++ 2,6 s + Java), output riproducibile; GRAPHS=10000 SEED=1 come tests/wintree-java/run.sh
```

| File | Contenuto |
|---|---|
| `wintree.h` | `Result` (PROVEN_WIN con `d` e `rev`, PROVEN_NOWIN con `req`, mossa testimone, UNKNOWN_CYCLE con `p`, UNKNOWN_BUDGET con `resid`, flag `cut`), `Goal` (WIN/NOWIN), `Budget` (nodi, profondità, deadline, `stop`), `ProofTable` (2 parole, xor-check, mai un PROVEN sovrascritto da un UNKNOWN), `Prover<G>`: Sez. 3 + O1 corretto (il secondo ciclo legge via tabella anche i figli diventati PROVEN, bitset dei figli già letti), dejavu compatto con propagazione, intervalli di validità sul contatore (§7.5), controllo cooperativo ogni 1024 nodi, assert I1-I4 attivi anche in release |
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

## Aggiunte di M2 (2026-10-16, con `src/wintree-chess`)

- **`rev/req` (§7.5)**: `G` espone `rule50()` e `reversible(Move)`; ogni entry PROVEN porta 7 bit di vincolo sul contatore: `PROVEN_WIN` vale se `c + rev <= 100` (il matto al 100° ply conta, `Position::is_draw`), `PROVEN_NOWIN` se `c >= req`. Sink: matto/stallo/morta 0; TB vinta `rev = 100` (solo con contatore 0, nuovo `Terminal::WIN_GOAL_C0`); 50 mosse `req = 100` (`Terminal::NOWIN_GOAL_C100`); TB non vinta `req = 0` (monotona nel contatore). Un'entry fuori intervallo non viene usata e il nodo si ricerca (il nuovo risultato ha intervallo più ampio e sovrascrive). I grafi di test dichiarano ogni mossa irreversibile (nessuna regola delle 50 mosse: `rev = 1`, `req = 0`).
- **Mossa testimone** nell'entry (16 bit, `G::code`/`G::move`): il figlio che chiude un OR (PROVEN_WIN) o confuta un AND (PROVEN_NOWIN). Serve all'estrazione degli oggetti di prova (§7.9) e al `bestMove`.
- **`Goal`**: `WIN` limita la profondità a 245 ply (§8.1) **e nei nodi AND interrompe al primo figlio NIL** (potatura da risolutore di matti: un figlio irrisolto nega la vittoria del nodo in questa iterazione; gli altri figli servirebbero solo a un PROVEN_NOWIN). `NOWIN` esplora tutti i figli (Sez. 3) con tetto 4096. Senza la potatura la ricerca di matto a profondità 5 su una posizione di mediogioco superava 10^7 nodi; con la potatura i matti in 4-5 del bench si trovano in 10^5-4·10^5 nodi. Entrambe le modalità sono solide (I1: PROVEN solo da figli PROVEN); la potatura perde solo PROVEN_NOWIN intermedi, e il grafo di strategia di patta resta estraibile (il figlio NIL trovato è una scelta valida del difensore, Thm 4.8). Il test di M1 usa `NOWIN`.
- **Correzione**: `ProofTable::store` controllava il bit di stato sulla *chiave* vecchia (`w0 ^ w1`) invece che sui *dati* (`w1`); con le chiavi piccole dei grafi era inerte, con chiavi Zobrist a 64 bit avrebbe rifiutato a caso le scritture UNKNOWN. Ora: mai un PROVEN sovrascritto da un UNKNOWN, qualunque sia la chiave.

Risultati di `make test` invariati (stessi numeri della tabella sopra).

Non fatto: generazione/età nella PT (rimpiazzo: l'ultimo scrive, mai un PROVEN con un UNKNOWN), collisione forzata dell'hash (§9.2 punto 8), `key_after` (il primo ciclo di O1 fa `do_move`/`undo_move` per sondare i figli).
