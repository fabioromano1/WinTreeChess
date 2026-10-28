# WinTree con dejavu propagato: completezza alla radice (lemma di §12.1 punto 2)

2026-10-25. Oggetto: la variante del dejavu implementata in `src/wintree/wintree.h` (entry `UNKNOWN_CYCLE` = `(p, hash del prefisso)`, insieme dei ply dei bloccanti nel frame, propagazione ai padri) e descritta in §8.3 della spec (`docs/superpowers/specs/2026-10-15-wintree-stockfish-design.md`). Riferimenti al paper (arXiv 2607.09715v2): Sez. 3 (algoritmo, Thm 3.1-3.6), Sez. 4.2 (Def. 4.4 alberi vincenti, Lemmi 4.5-4.7, Thm 4.8), pag. 21 (idea del dejavu).

## 0. Risultato in breve

1. **Confutata** per la variante implementata. Il salto di un figlio `w` trasmette al padre solo il ply massimo `p` dei bloccanti di `w` (`inherit`, `wintree.h:222-225`), mentre l'induzione richiede l'intero insieme `B(w) \ {ply del padre}`. Esiste un game graph (Def. 2.6) a 12 vertici con `eval(v0) = g` su cui il nucleo C++ con dejavu termina senza budget con la radice `UNKNOWN_CYCLE` (§6; verificato eseguendo il nucleo contro l'oracolo: senza dejavu la radice è `PROVEN_WIN` con `d = 7`). Sullo stesso grafo cade anche l'analogo del Thm 4.8 (strategia di patta). Un secondo grafo a 15 vertici fa cadere la radice in entrambe le modalità `Goal` (`WIN` e `NOWIN`).
2. **Dimostrata** per la regola corretta: se al salto il padre eredita un soprainsieme di `B(w) \ {ply del padre}` (insieme esatto, variante A; oppure tutto il prefisso `[0..p]`, variante B), vale l'invariante "ogni albero vincente da un nodo NIL tocca uno dei suoi bloccanti" (Lemma 3), da cui la completezza alla radice (Teorema 4, analogo del Thm 3.4) e la versione forte (Corollario 5, analogo del Lemma 4.7, salto compreso). Modulo collisioni dell'hash a 32 bit, che perdono solo completezza.
3. **Misurato** (prima nello scratchpad su copie patchate dell'header, poi con la variante A applicata a `wintree.h`, §8): entrambe le varianti risolvono i controesempi e passano il test ufficiale (`regression` + fuzz 10^4 grafi: 0 radici incomplete, 0 fallimenti del Thm 4.8). Espansioni totali sui 20 000 casi del fuzz: originale 135 087, A 134 562, B 142 439, senza dejavu 217 370.

4. **Thm 4.8 per la variante A (§10, 2026-10-28): non confutato, dimostrato a meno di un lemma.** Nessun controesempio in circa 5·10^7 ricerche mirate (grafi di 4-24 vertici con cicli densi, controllo diretto del cammino NIL e model checker; anche per la variante B di ripiego). Il teorema è **ridotto rigorosamente** al Lemma H1 ("i bloccanti finali di un vertice NIL raggiungibile dalla radice per vertici NIL sono a loro volta raggiungibili"), e H1 è dimostrato per induzione in tutti i casi tranne uno, isolato come Lemma 9 (enunciato preciso in §10.3), che resta **aperto**: verificato senza eccezioni nel fuzz, insieme a invarianti strutturali più forti (§10.4). La dimostrazione del paper non si trasferisce perché con la tabella non esiste in generale un istante in cui tutto il cammino NIL è GRAY (§10.5).

Il resto del documento dà definizioni, dimostrazione, controesempio con traccia, conseguenze per la spec e ciò che resta aperto.

## 1. Ipotesi e notazione

- `G = ⟨V, E, v0, win, turn⟩` game graph (Def. 2.6, turni alternati su ogni arco, nessun self-loop sui non-sink), `g` il giocatore obiettivo. Un vertice `v` è **OR** se `turn(v) = g`, **AND** altrimenti. `eval` e `d_g` come in §2.2 della spec (minimo punto fisso); `eval(v) = g ⟺ d_g(v) < ∞` (Lemma 2.9).
- **Alberi vincenti** `WinTrees(G, g)` (Def. 4.4): foglie = sink vinti da `g`; un nodo OR ha esattamente un figlio (un vertice adiacente); un nodo AND ha un figlio per ogni vertice adiacente. `nodes(T)` e altezza `h(T)` come in Def. 4.2-4.3. Si usano i fatti del paper: **(F1)** `T ∈ WinTrees ⟹ eval(T.root) = g` (Lemma 4.5); **(F2)** `eval(v) = g ⟹ ∃ T ∈ WinTrees, T.root = v` (Lemma 4.6); **(F3)** ogni sottoalbero di un albero vincente è un albero vincente (immediato dalla Def. 4.4). Lo stesso vertice può comparire più volte in un albero vincente.
- **Ricerca**: una chiamata `solve` senza budget (`maxNodes`, `maxMoves`, `deadline`, `stop` infiniti, profondità sufficiente), senza regola delle 50 mosse (`rule50() = 0`, ogni mossa irreversibile: è il caso dei grafi di test; le entry `PROVEN` sono sempre valide), chiavi a 64 bit senza collisioni, tabella senza rimpiazzi di entry (è la situazione del test: 1024 entry, ≤ 40 vertici). Entrambe le modalità `Goal` sono coperte: la potatura `firstNil` di `WIN` interrompe un nodo AND al primo figlio NIL e non altera nessun passo della dimostrazione (§4, caso AND).
- Durante la visita di un vertice a profondità (ply) `k` il **cammino** corrente è `path[0..k-1]` (`path[0] = v0`): sono i vertici GRAY del paper. `H[j]` è l'hash del prefisso `path[0..j-1]` (`H[0] = 0` per il prefisso vuoto; `wintree.h:173,189,260`). Per un insieme di ply `B ⊆ {0..k-1}` scriviamo `path[B] = {path[q] : q ∈ B}`; essendo il cammino semplice, `q ↦ path[q]` è iniettiva.
- Ogni frame a ply `k` ha l'insieme dei ply dei bloccanti `B_k` (`blockers(k)`, un bitset; `wintree.h:175-177, 212-218`).

## 2. L'algoritmo come implementato

Trascrizione di `visit` (`wintree.h:229-242`) e `rec` (`244-318`), limitata a ciò che conta per la completezza (le righe non citate riguardano budget, `rev/req`, `d`, mossa testimone).

`visit(k)` per il vertice `w` raggiunto a ply `k` (figlio di `path[k-1]`, oppure la radice se `k = 0`):

- **(V1) GRAY.** Se `w = path[q]` per un `q < k` (`231-232`): esito NIL; se `k ≥ 1`, `inherit(k, q)`. Poiché `q ≠ k-1` (sarebbe un self-loop), `inherit` fa `B_{k-1} ∪= {q}`.
- **(V2) PROVEN.** Se la tabella ha un'entry `PROVEN_*` per `w` (`236`): esito TRUE/FALSE, nessuna ricorsione.
- **(V3) SALTO (dejavu).** Se la tabella ha `UNKNOWN_CYCLE(p, h)` con `p < k` e `u32(H[p+1]) = h` (`237`): esito NIL e `inherit(k, p)`, cioè (`222-225`):
  - se `p < k-1`: `B_{k-1} ∪= {p}`;
  - se `p = k-1` (il bloccante più profondo è il padre stesso): `B_{k-1} ∪= {k-2}` se `k ≥ 2`, nulla altrimenti;
  - se `p = -1` (`h = 0`, prefisso vuoto): nulla. Un'entry con `p = -1` soddisfa la condizione di salto a ogni ply, in ogni ricerca con la stessa tabella.
- **(V4) ESPANSIONE.** Altrimenti `rec(k)`.

`rec(k)` per `u`:

- **(R1)** Se `u` è un sink: entry `PROVEN_WIN` se `win(u) = g`, `PROVEN_NOWIN` altrimenti.
- **(R2)** `path[k] := u`, `H[k+1] := mix(H[k] ^ key(u))`, `B_k := ∅` (`260-261`).
- **(R3)** Per ogni figlio `w` (prima quelli già `PROVEN` in tabella, senza ricorsione; poi gli altri, in ordine; `281-300`): `r := visit(k+1)`. OR: TRUE ⟹ `u` TRUE e stop; NIL ⟹ `nil`. AND: FALSE ⟹ `u` FALSE e stop; NIL ⟹ `nil` (in `WIN` con `firstNil`: stop). Se il figlio è stato espanso (V4) ed è finito NIL, il suo frame ha già eseguito `B_k ∪= B_{k+1} \ {k}` (`313-315`).
- **(R4)** Fine: TRUE/FALSE ⟹ entry `PROVEN`. Altrimenti (`nil`, nessun taglio) `p := max B_k` (`-1` se vuoto), entry `UNKNOWN_CYCLE(p, u32(H[p+1]))` (`310-312`) e, se `k > 0`, `B_{k-1} ∪= B_k \ {k-1}`.

Si verifica per induzione che `B_k ⊆ {0..k-1}`: (V1) aggiunge `q ≤ k-1`; (V3) aggiunge `p ≤ k-1` oppure `k-2`; (R4) aggiunge `B_{k+1} \ {k} ⊆ {0..k-1}`. Quindi `path[B_k]` è un insieme di **antenati propri** di `u`, tutti GRAY.

**Nota sulla condizione di salto.** `u32(H[p+1]) = h` con lo stesso `p` significa, salvo collisione a 32 bit, che il prefisso `path[0..p]` corrente coincide con il prefisso `P[0..p]` del cammino `P` in vigore quando l'entry fu scritta. Siccome `B ⊆ {0..p}` (p ne è il massimo), i vertici bloccanti `P[B]` sono esattamente `path[B]`: stessi vertici, stessi ply, tutti GRAY adesso. È questa la proprietà che il salto usa; l'entry però contiene solo `p`, non `B`.

## 3. Invariante

**Definizione (bloccanti di un nodo NIL).** Per un'entry `UNKNOWN_CYCLE` di `w`, scritta in (R4) alla fine di `rec(k_w)` con cammino `P` e insieme `B` (`p = max B`), chiamiamo `blocc(entry) := P[B]`. Per un frame a ply `k` che termina NIL con `B_k`: `blocc := path[B_k]`.

**Invariante (I).** Per ogni entry `UNKNOWN_CYCLE` (e per ogni frame che termina NIL): **ogni `T ∈ WinTrees(G, g)` con `T.root = w` ha `nodes(T) ∩ blocc ≠ ∅`**, e inoltre `blocc ⊆ Reach(w)` (vertici raggiungibili da `w`). In particolare `B = ∅` (cioè `p = -1`) significa: nessun albero vincente da `w`, quindi `eval(w) ≠ g` per (F2).

(I) è una proprietà **statica** del grafo: una volta vera resta vera, qualunque cosa accada nella tabella (vertici che diventano BLACK, entry sovrascritte, ricerche successive).

**Lemma 1 (le due formulazioni coincidono).** L'enunciato del README M1 ("ogni albero vincente da `u` tocca `path[0..p]` oppure contiene `u` stesso") nella forma "ogni albero vincente da `u` tocca `blocc` **oppure contiene `u` sotto la radice**" è equivalente a (I).
*Dimostrazione.* (I) implica banalmente la forma debole. Viceversa, se esistesse un albero vincente da `u` che evita `blocc`, se ne prenda uno `T` di altezza minima; per la forma debole `T` contiene `u` sotto la radice; il sottoalbero `T'` radicato in quell'occorrenza è vincente (F3), evita `blocc` (`nodes(T') ⊆ nodes(T)`) e ha `h(T') < h(T)`: contraddizione. ∎

Il Lemma 1 giustifica anche la lettura "`p = -1` ⟹ nessun albero vincente" del README, **a patto che (I) valga**.

**Lemma 2 (solidità, Thm 3.1 e Cor. 3.3).** `PROVEN_WIN(v) ⟹ eval(v) = g`; `PROVEN_NOWIN(v) ⟹ eval(v) ≠ g`.
*Dimostrazione.* Un'entry `PROVEN` nasce solo in (R1) per un sink o in (R4) da esiti TRUE/FALSE dei figli, che sono a loro volta entry `PROVEN` lette (V2) o esiti di ricorsioni; GRAY e salto producono solo NIL. Induzione sull'ordine delle scritture, con l'argomento del Thm 3.1. ∎ (È l'invariante I1 del progetto.)

## 4. Lemma di conservazione

Chiamiamo **regola di eredità** il modo in cui un figlio `w` finito NIL arricchisce `B_k` del padre `u` (ply `k`): sia `B_w` l'insieme di ply associato a `w`, cioè `{q}` in (V1), l'insieme `B'` dell'entry saltata in (V3), `B_{k+1}` in (V4). La regola è **ammissibile** se garantisce

  **(H)**  `B_k ⊇ B_w \ {k}`.

La regola implementata soddisfa (H) in (V1) e (V4). In (V3) aggiunge solo `{p}` (o `{k-1}`), quindi soddisfa (H) **soltanto** se `B' \ {k} ⊆ {p}` (caso `p < k`) oppure `B' ⊆ {k-1, k}` (caso `p = k`): cioè se il figlio saltato ha al più un bloccante diverso dal padre. Le varianti di §8 (A: eredita `B' \ {k}`; B: eredita `{0..p} \ {k} ⊇ B' \ {k}`) sono ammissibili.

**Lemma 3 (conservazione di (I)).** Con una regola di eredità ammissibile, (I) vale per ogni entry `UNKNOWN_CYCLE` scritta e per ogni frame che termina NIL (salvo collisioni dell'hash del prefisso).

*Dimostrazione.* Induzione sul tempo di esecuzione: si assume (I) per tutte le entry scritte prima e per tutte le chiamate concluse prima della fine di `rec(k)` per `u`, e si prova (I) per il frame di `u` (l'entry scritta in (R4) copia `B_k`, e la proprietà è statica: vale quindi anche per l'entry, per sempre). Sia `u` NIL alla fine di (R3). Per assurdo esista un albero vincente da `u` che evita `path[B_k]`; sia `T` uno di altezza minima.

*Passo 1 (u non si ripete).* `u ∉ nodes(T) \ {T.root}`: altrimenti il sottoalbero radicato nell'occorrenza sarebbe vincente (F3), eviterebbe `path[B_k]` e sarebbe più basso.

*Passo 2 (un figlio NIL con sottoalbero in T).* Esiste un figlio `w` di `u`, esaminato in (R3) con esito NIL, tale che `T` ha un sottoalbero `T_w` con `T_w.root = w`.
- `u` OR: `u` NIL implica che nessun figlio ha dato TRUE, dunque la scansione non si è interrotta e **tutti** i figli sono stati esaminati (in entrambe le modalità: `firstNil` riguarda solo i nodi AND). `T` ha un solo figlio `T.c1`, radicato in un `w ∈ Adj(u)` (Def. 4.4). L'esito di `w` non è TRUE (nessuno lo è) e non è FALSE (Lemma 2 e (F1): da un vertice con `eval ≠ g` non parte alcun albero vincente, ma `T.c1` è vincente). Quindi NIL.
- `u` AND: `u` NIL implica che un figlio esaminato `w` ha dato NIL (è ciò che pone `nil`); per la Def. 4.4 (AND) `T` ha un sottoalbero radicato in **ogni** vertice adiacente, in particolare in `w`.

*Passo 3 (contraddizione).* `T_w` è vincente (F3), radicato in `w`, ed evita `path[B_k] ∪ {u}` (per il Passo 1 e perché `T` evita `path[B_k]`). Tre casi secondo l'origine del NIL di `w`:
- **(V1) GRAY**: `w = path[q]`, `q ∈ B_k` (la regola marca `q`). Allora `w ∈ path[B_k] ∩ nodes(T)`. Contraddizione.
- **(V4) espanso e NIL**: per ipotesi induttiva ogni albero vincente da `w` tocca `path[B_{k+1}]` (il cammino di `w` è `path[0..k]`, lo stesso di adesso). Per (H), `path[B_{k+1}] ⊆ path[B_k] ∪ {path[k]} = path[B_k] ∪ {u}`. Dunque `T_w` tocca `path[B_k] ∪ {u}`. Contraddizione.
- **(V3) saltato**: l'entry fu scritta con cammino `P` e insieme `B'`, `p = max B'`; per ipotesi induttiva ogni albero vincente da `w` tocca `P[B']`. La condizione di salto dà `path[0..p] = P[0..p]` (salvo collisione), quindi `P[B'] = path[B']`. Per (H), `path[B'] ⊆ path[B_k] ∪ {u}`. Dunque `T_w` tocca `path[B_k] ∪ {u}`. Contraddizione.

Quindi nessun albero vincente da `u` evita `path[B_k]`. Infine `path[B_k] ⊆ Reach(u)`: in (V1) `w = path[q] ∈ Adj(u)`; in (V4) e (V3) i bloccanti di `w` sono raggiungibili da `w` per ipotesi induttiva e `w ∈ Adj(u)`. ∎

**Dove la variante implementata esce dalla dimostrazione.** Nel caso (V3) la regola implementata dà `B_k ⊇ {p}` ma `B' \ {k}` può contenere ply `q < p`: il vertice `path[q]` è un bloccante di `w` (ogni albero vincente da `w` passa di lì) che il padre dimentica. Finché `q` resta sotto il massimo di `B_k` il danno è nascosto dalla condizione di salto sul prefisso (che copre tutto `[0..max B_k]`); ma quando la propagazione (R4) risale fino all'antenato a ply `p`, questo toglie il proprio ply e **non ha più nulla** di `w`: la sua entry (e quelle sopra) hanno `p` troppo piccolo, fino a `-1`. Il commento di `inherit` ("se quel massimo è il padre si assume tutto il prefisso sopra") tratta solo il caso `p = k`, e lo tratta ancora con un solo ply (`k-2`), che il nonno a sua volta toglie. Il controesempio di §6 realizza esattamente questa perdita.

## 5. Completezza alla radice e versione forte

**Teorema 4 (analogo del Thm 3.4).** Con una regola di eredità ammissibile, se `eval(v0) = g` allora la ricerca senza budget termina con la radice `PROVEN_WIN`.
*Dimostrazione.* La radice è a ply 0 e `B_0 ⊆ ∅`. Se l'esito fosse NIL, per il Lemma 3 ogni albero vincente da `v0` toccherebbe `path[∅] = ∅`: non esistono alberi vincenti da `v0`, contro (F2). Se fosse FALSE, `eval(v0) ≠ g` per il Lemma 2. Senza budget non ci sono tagli (I2). Resta TRUE. ∎

**Corollario 5 (analogo del Lemma 4.7, salto compreso).** Sia `v` il vertice esaminato da una chiamata `visit(k)` che non cade nel caso (V1) (cioè `v` non è GRAY), e sia `S` l'insieme dei vertici GRAY raggiungibili da `v` in quel momento (`S = path[0..k-1] ∩ Reach(v)`). Se esiste `T ∈ WinTrees(G, g)` con `T.root = v` e `nodes(T) ∩ S = ∅`, la chiamata restituisce TRUE.
*Dimostrazione.* L'esito non è FALSE (Lemma 2 + (F1)). Se fosse NIL: in (V3) ogni albero vincente da `v` tocca `P[B'] = path[B'] ⊆ path[0..k-1] ∩ Reach(v) = S` (invariante statica e condizione di salto); in (V4) ogni albero vincente da `v` tocca `path[B_k] ⊆ S` (Lemma 3, con `path[B_k] ⊆ Reach(v)`). In entrambi i casi nessun albero vincente da `v` evita `S`, contro l'ipotesi. Resta TRUE, letto in (V2) o prodotto da (V4). ∎

**Osservazione (vertici diventati BLACK fra la prima esplorazione e il salto).** Un nodo saltato `w` potrebbe essere dimostrabile *in quel momento*: per esempio se un suo figlio `c` è diventato `PROVEN_WIN` dopo che `w` fu memorizzato, con un albero di prova che passa per il bloccante GRAY `y` (coerente con (I): l'albero `w → c → … → y → …` tocca `y`). Il salto lo lascia NIL, perdendo completezza **locale** (è lo stesso fenomeno dei WHITE con `eval = g` del paper, §2.4 della spec). La completezza **alla radice** non ne soffre: l'invariante (I) del padre resta vero (Lemma 3) e l'albero vincente minimo dalla radice non può passare due volte per `y`. La correttezza del salto non dipende da "w è irrisolvibile adesso", ma solo da "i bloccanti di w sono sul cammino".

**Osservazione (hash del prefisso a 32 bit).** Un falso positivo fa saltare `w` con un prefisso diverso: `P[B'] ≠ path[B']` in generale, il Passo 3 non si applica e la completezza (alla radice) può cadere; la solidità no, perché il salto produce solo NIL. La probabilità per confronto è 2^-32 (prefissi della stessa lunghezza `p+1`, stessa chiave). Le collisioni delle chiavi a 64 bit sono fuori perimetro (I8).

**Osservazione (riuso fra ricerche).** (I) è statica, perciò un'entry `UNKNOWN_CYCLE` lasciata da una ricerca precedente con la stessa tabella è riutilizzabile: per `p ≥ 0` il salto richiede lo stesso prefisso dalla radice (stessa radice), per `p = -1` è legittimo ovunque (nessun albero vincente da `w`). Le entry `UNKNOWN_CYCLE` non nascono mai da tagli di budget (I2, `wintree.h:306-308`), quindi anche ricerche interrotte lasciano solo entry che rispettano (I), **se la regola di eredità è ammissibile**. Con la regola implementata un'entry `p = -1` errata (come il vertice `x` di §6) viene saltata **in ogni ricerca successiva** finché non è rimpiazzata: il buco di completezza è permanente.

## 6. Controesempio verificato

### 6.1 Il grafo (formato di `tests/wintree-java/Gen.java`, `g = 1`)

```
# P4: il salto trasmette solo il massimo dei bloccanti. eval(0) = 1, d_g(0) = 7; con dejavu la radice resta UNKNOWN_CYCLE
0 1 . 1 5
1 2 . 2 3
2 1 . 5 4
3 1 2
4 2 1
5 2 . 6
6 1 . 7 8
7 2 . 9 3
8 2 . 9
9 1 . 10 11
10 2 . 6
11 2 . 2
```

| id | ruolo | tipo (g = 1) | figli | perché c'è |
|---|---|---|---|---|
| 0 | `r` radice | OR | 1, 5 | vince solo attraverso 5 → 6 |
| 1 | `m` | AND | 2, 3 | porta la DFS a `b` e poi diventa FALSE (via 3) |
| 2 | `b` | OR | 5, 4 | il bloccante "alto" di `w`; esplora `x` e poi diventa TRUE (via 4) |
| 3 | `f` | sink vinto da 2 | | confuta `m` e `u` |
| 4 | `s` | sink vinto da 1 | | vittoria di `b` |
| 5 | `n` | AND | 6 | via a `x` sia da `b` sia dalla radice |
| 6 | `x` | OR | 7, 8 | il bloccante "profondo" di `w`; finisce con `p = -1` |
| 7 | `u` | AND | 9, 3 | sotto `u` `w` viene espanso; `u` diventa FALSE e **non propaga** |
| 8 | `u2` | AND | 9 | sotto `u2` `w` viene saltato |
| 9 | `w` | OR | 10, 11 | NIL con due bloccanti: `x` (ply 4) e `b` (ply 2) |
| 10 | `c1` | AND | 6 | ponte di parità verso `x` |
| 11 | `c2` | AND | 2 | ponte di parità verso `b` |

Oracolo (`d_g`, vertici 0..11): `7 ∞ 1 ∞ 0 6 5 ∞ 4 3 6 2`. L'unico albero vincente dalla radice è `0 → 5 → 6 → 8 → 9 → 11 → 2 → 4`: passa per `b` (vertice 2), il bloccante che il salto dimentica.

### 6.2 Traccia (modalità `NOWIN`, dejavu attivo; ply fra parentesi)

1. `r(0) → m(1) → b(2) → n(3) → x(4) → u(5) → w(6) → c1(7) → x`: GRAY a ply 4. `B(c1) = {4}`; `c1` NIL, entry `(4, H[5])`; `B(w) ∪= {4}`.
2. `w → c2(7) → b`: GRAY a ply 2. `B(c2) = {2}`; `c2` NIL, entry `(2, H[3])`; `B(w) = {2, 4}`.
3. `w` NIL: entry **`(4, H[5])`**, l'informazione "anche 2" resta solo nel frame; propagazione `B(u) ∪= {2, 4}`.
4. `u → f`: FALSE ⟹ `u` FALSE (`PROVEN_NOWIN`). **Nessuna propagazione**: `{2, 4}` è perduto.
5. `x → u2(5) → w`: tabella `(4, H[5])`, `4 < 6` e `H[5]` uguale (stesso prefisso `r, m, b, n, x`) ⟹ **salto**; `inherit(6, 4)` dà `B(u2) = {4}`.
6. `u2` NIL: entry `(4, H[5])`; propagazione `B(u2) \ {4} = ∅`: `x` non riceve nulla.
7. `x`: `u` FALSE, `u2` NIL ⟹ NIL con `B(x) = ∅` ⟹ entry **`(-1, 0)`**: "nessun albero vincente da `x`", falso (`d_g(x) = 5`). Propagazione vuota a `n`, che chiude NIL con `(-1, 0)`.
8. `b → s`: TRUE ⟹ `b` `PROVEN_WIN` (`d = 1`). `m → f`: FALSE ⟹ `m` FALSE.
9. `r → n(1)`: tabella `(-1, 0)`, `-1 < 1`, `H[0] = 0` ⟹ **salto**. `r`: `m` FALSE, `n` NIL ⟹ **radice `UNKNOWN_CYCLE`** con `eval(r) = g`.

Con una regola ammissibile il passo 5 dà `B(u2) = {2, 4}`, il passo 6 propaga `{2}`, `x` chiude con `(2, H[3])`; al passo 9 `n` e `x` vengono riespansi sotto `r` (prefisso diverso), `w` viene riespanso (`p = 4 ≥ ply 4`), `c2 → b` legge `PROVEN_WIN` e la catena `w, u2, x, n, r` chiude `PROVEN_WIN` con `d = 7`.

### 6.3 Esecuzione del nucleo C++ (driver nello scratchpad, `Prover<GraphGame>` di `wintree.h`, tabella da 1024 entry)

```
goal 1: oracolo eval(0)=g, d_g=7; d_g: 7 inf 1 inf 0 6 5 inf 4 3 6 2
  NOWIN dejavu=1: radice UNKNOWN_CYCLE, espansioni 12, Thm 4.8 fallito=si
    0:N(p=-1) 1:F 2:W1 3:F 4:W0 5:N(p=-1) 6:N(p=-1) 7:F 8:N(p=4) 9:N(p=4) 10:N(p=4) 11:N(p=2)
  NOWIN dejavu=0: radice PROVEN_WIN d=7, espansioni 21
    0:W7 1:F 2:W1 3:F 4:W0 5:W6 6:W5 7:F 8:W4 9:W3 10:N(p=2) 11:W2
  WIN   dejavu=1: radice PROVEN_WIN d=7, espansioni 19
```

In modalità `WIN` questo grafo non cade: `u` (AND) si ferma al primo figlio NIL (`firstNil`), resta NIL e propaga `{2, 4}` a `x`. Il grafo seguente, a 15 vertici, cade in **entrambe** le modalità perché la catena è spezzata da un nodo OR che diventa TRUE (`6 → 8`), e l'esplorazione completa dei due bloccanti avviene in un nodo OR (`10`):

```
# P4bis: radice UNKNOWN_CYCLE in WIN e NOWIN; eval(0) = 1, d_g(0) = 10
0 2 . 1 2
1 1 . 3
2 1 . 4
3 2 . 5
4 2 . 6 7
5 1 . 4 8
6 1 . 9 8
7 1 . 9
8 2 1
9 2 . 10
10 1 . 11 12
11 2 . 13
12 2 . 14
13 1 . 4
14 1 . 3
```

Esiti: dejavu attivo (`WIN` e `NOWIN`) radice `UNKNOWN_CYCLE`, stati `N W3 N W2 N W1 W1 N W0 N N N N N N` (vertice 4 con `p = -1`); dejavu spento `PROVEN_WIN d = 10`, stati `W10 W3 W9 W2 W8 W1 W1 W7 W0 W6 W5 N W4 N W3`.

### 6.4 Thm 4.8 (strategia di patta)

Sul grafo di §6.1 il model checker del test (`drawStrategyFails` di `test.cpp`) fallisce: il difensore che dalla radice NIL sceglie solo figli NIL percorre `0 → 5 → 6 → 8 → 9 → 11` e in `11` (NIL) ha come unica mossa `2`, che è `PROVEN_WIN`: raggiunge il sink `4` vinto da `g`. Cade quindi anche la garanzia di §9.2 punto 10 e di `src/wintree/README.md` (M2) "il figlio NIL trovato è una scelta valida del difensore".

### 6.5 Minimalità e perché il fuzz non lo trova

Il grafo di §6.1 è minimo per la famiglia costruita: `c1`, `c2`, `m` sono imposti dall'alternanza dei turni (il ciclo `x → u → w → c1 → x` deve avere lunghezza pari, `b` e `x` devono avere lo stesso turno e la radice non può avere `b` come figlio OR); `f` serve a spezzare la catena di propagazione; `u2` è il padre del salto; `n` è condiviso fra `b` e la radice. Ne è un prerequisito un nodo NIL con **due bloccanti a ply diversi**, saltato sotto un fratello di un nodo che è diventato `PROVEN` dopo aver esplorato quel nodo. Una ricerca casuale con il generatore di `graph.h` (parametri `n ∈ [5, 11]` e `[10, 18]`, grado 1-3, cicli 0,3-0,9, sink 0,1-0,35; 18·10^6 ricerche) non ha prodotto alcuna radice incompleta, coerentemente con gli 0 del fuzz di M1 su 10^4 grafi: la distribuzione del test è cieca a questo difetto. Non è escluso un controesempio più piccolo con struttura diversa.

## 7. Conseguenze

- **§8.3 (proof table, dejavu compatto).** L'entry `(p, hash)` basta per il **salto** (la condizione sul prefisso fino a `p` copre tutti i bloccanti), ma non per la **propagazione** attraverso il salto: il padre deve ereditare `B(w) \ {ply padre}` o un soprainsieme, e `p` da solo non lo determina. La frase "il padre eredita i bloccanti del figlio ristretti ai ply sopra il padre" è corretta come specifica, ma l'entry compatta non permette di realizzarla; la correzione di M1 (insieme nel frame) copre il figlio **espanso**, non quello **saltato**. L'argomento "da dimostrare" riportato in §8.3 e nel README (M1, scostamento 1) è l'invariante (I) nella forma del Lemma 1, ed è corretto come invariante; manca il passo (H) al salto, che l'implementazione viola.
- **§12.1 punto 2.** L'obbligo di prova è chiuso in due parti: negativa per la variante implementata (controesempio), positiva per la variante con eredità ammissibile (Lemma 3, Teorema 4, Corollario 5). Resta la scelta ingegneristica fra A e B (§8) e la misura B1/B5 citata in §8.3.
- **§2.7 (correzione proposta).** Il "colpo virtuale su ciascun bloccante" è esattamente l'eredità esatta (variante A, nella forma pigra "ogni antenato toglie il proprio ply"): questo documento ne è la dimostrazione. La congettura "con la propagazione la completezza alla radice sopravvive" è vera per quella correzione, non per la compattazione di §8.3.
- **§9.2 e `src/wintree/test.cpp`.** Il fuzz non copre il difetto; i due grafi di §6 sono regressioni pronte per `regression()` nel formato già usato (`{"P4 …", testo del grafo, stati attesi}`), con stati attesi `W7 F W1 F W0 W6 W5 F W4 W3 N W2` e `W10 W3 W9 W2 W8 W1 W1 W7 W0 W6 W5 N W4 N W3` (quelli corretti, che il nucleo attuale non produce). Il model checker del Thm 4.8 va eseguito anche su di essi.
- **Portata pratica.** Solo completezza: nessun `PROVEN` errato (Lemma 2), nessuna violazione di I1-I4. Il nucleo df-pn (`dfpn.h`, nucleo di produzione in `wintree-chess` e `stockfish-hybrid`) non usa il salto a prefisso (nessun riferimento a `inherit`/`H[]`), quindi non è toccato da questo difetto; lo è il nucleo depth-first `Prover`, usato come riferimento, nei test differenziali e nelle ablazioni B5. Dove `Prover` è usato con tabella persistente, un'entry `p = -1` errata è un buco permanente (§5, ultima osservazione).
- **Thm 4.8.** Cade sul controesempio (§6.4). Per la variante corretta non è dimostrato qui (§9).

## 8. Varianti corrette e misure

Due regole ammissibili, provate prima su copie di `wintree.h` nello scratchpad (un solo punto cambiato ciascuna, il ramo (V3)):

- **A (insieme esatto)**: a fianco dell'entry si conserva l'insieme dei ply (`std::unordered_map<u64, std::vector<u64>> sets`, scritto in (R4)); al salto il padre fa `B_{k-1} |= sets[w] & ~bit(k-1)`. È la regola di §2.7 della spec. Nell'entry a 64 bit l'insieme non entra (fino a 245 ply): servono una struttura laterale per ricerca o un'entry più larga.
- **B (prefisso intero)**: al salto il padre marca tutti i ply `0..min(p, k-2)`. Nessun cambiamento di layout. Soddisfa (H) perché `B' ⊆ {0..p}`. Costo: gli antenati a ply `≤ p` ricevono tutto il prefisso sopra di sé e diventano saltabili solo con cammino identico fino a loro (`p` = proprio ply − 1).

Risultati (stessi grafi e stesse asserzioni di `make -C src/wintree test`, parte C++):

| nucleo | controesempi §6 | test ufficiale (`wintree_test 10000 1`) | Σ espansioni fuzz (20 000 ricerche, `NOWIN`) | max espansioni |
|---|---|---|---|---|
| originale | radice `UNKNOWN_CYCLE` | `tutto OK` (cieco) | 135 087 | 356 |
| A | `PROVEN_WIN` d = 7 / 10 | `tutto OK`, 0 radici incomplete, 0 Thm 4.8 | 134 562 | 356 |
| B | `PROVEN_WIN` d = 7 / 10 | `tutto OK`, 0 radici incomplete, 0 Thm 4.8 | 142 439 | 653 |
| dejavu spento | `PROVEN_WIN` d = 7 / 10 | — | 217 370 | — |

Il totale dell'originale coincide con i 135 087 del README (M2c): il driver riproduce le esecuzioni ufficiali. **Applicazione (2026-10-25)**: la variante A è ora in `src/wintree/wintree.h` (`Prover::sets`, insieme dei ply a lato di ogni entry `UNKNOWN_CYCLE` riconosciuto da `(p, hash)`; `inherit_skip` lo eredita al salto meno il ply del padre e, se l'insieme manca perché l'entry fu scritta da un altro `Prover` sulla stessa tabella, come fa `wintree-chess` a ogni ricerca, ripiega sulla variante B); i due grafi di §6 sono le regressioni P4/P4bis di `test.cpp`; `make -C src/wintree test` passa (Σ espansioni 134 562, 0 radici incomplete, 0 Thm 4.8; df-pn chiude anch'esso le due radici). `make -C src/wintree-chess test` passa con entrambi i nuclei (`CORE=dfpn` e `CORE=wintree`); il G0 su 10^3 posizioni con `--core wintree` dà gli stessi numeri del binario costruito con l'header originale (379/1000 risolte, 75 `PROVEN_WIN` e 304 `PROVEN_NOWIN`, 0 disaccordi con le WDL, 75/75 alberi e 304/304 strategie certificati, stesse mediane di espansioni): su quel campione il salto con eredità diversa non interviene mai. A costa leggermente meno dell'originale (due effetti opposti: l'originale sovrastima nel caso `p = k`, dove marca `{k-1}`, e sottostima dove perde bloccanti; A è esatta); B costa il 5 % in più sui grafi del fuzz e raddoppia il massimo. Entrambe mantengono la proprietà "seconda chiamata a tabella piena: 0 espansioni".

## 9. Cosa resta aperto

1. **Scelta fra A e B** e misura sul caso scacchistico (B1/B5 di §9.4): quante volte il salto interviene con `p` sotto il padre e quanti bloccanti ha in media un nodo NIL. A richiede memoria per insieme (32 byte per entry `UNKNOWN_CYCLE` a 245 ply) o una compressione (per esempio i 32 ply sotto `p` come maschera, con "tutto il resto" conservativo oltre).
2. **Thm 4.8 per la variante corretta.** La dimostrazione del paper richiede un istante in cui tutti i `v_i` del cammino NIL sono GRAY insieme; con la tabella l'entry finale di `v_i` può essere stata scritta sotto un cammino diverso da `⟨v_0..v_{i-1}⟩`, e il passo induttivo non si trasferisce direttamente. **Aggiornamento 2026-10-28 (§10):** ridotto al Lemma H1 e questo al Lemma 9 (caso residuo dell'induzione), che resta aperto; 0 fallimenti in circa 5·10^7 ricerche mirate, anche per la variante B.
3. **Hash del prefisso**: la perdita per collisione è stimata, non misurata (§9.2 punto 8, collisione forzata, resta non fatto).
4. **Regola delle 50 mosse e budget**: fuori perimetro. Con `rev/req` una `PROVEN` fuori intervallo è riesplorata e può chiudere NIL: (I) continua a valere, ma la nozione di `eval` cambia (§7.5); da riesaminare quando il dejavu entrerà in `wintree-chess` con `Prover`.
5. **Estensione al df-pn**: il README (M2c) ipotizza un dejavu a prefisso per le confutazioni tainted; se sarà fatto, dovrà usare l'eredità ammissibile (A o B), non `(p, hash)` da solo.

## 10. Teorema 4.8 per la variante A: riduzione a un lemma, ricerca di controesempi (2026-10-28)

Oggetto: il Thm 4.8 del paper (strategia di patta) per il dejavu con eredità esatta dei bloccanti (variante A, `src/wintree/wintree.h`), nelle ipotesi di §1 (ricerca `NOWIN` senza budget su tabella vuota, nessuna regola delle 50 mosse, nessuna collisione, nessun rimpiazzo). Risultato: **nessun controesempio** (§10.4); il teorema è **ridotto rigorosamente** (§10.2) a un lemma di chiusura (H1), dimostrato per induzione tranne un caso residuo isolato con precisione (Lemma 9, §10.3), che **resta aperto** ed è verificato senza eccezioni nel fuzz insieme a invarianti più forti. Gli enunciati "naturali" che chiuderebbero il caso residuo e che il fuzz **confuta** sono elencati in §10.5, perché non vengano ritentati.

### 10.1 Enunciato

Notazione di §1-§3, più la seguente. La ricerca termina con la **tabella finale**: ogni vertice visitato ha un'entry `PROVEN_WIN` (insieme `W`), `PROVEN_NOWIN` (`F`) o `UNKNOWN_CYCLE` (`N`); i vertici mai espansi non hanno entry. Per `v ∈ N` l'entry finale è scritta dall'**ultima espansione** `E_v` di `v` (ogni espansione di un vertice `N` termina NIL e sovrascrive la precedente); `P^v = (a_0, …, a_{k-1})` è il cammino (gli antenati GRAY) al momento di `E_v`, con `a_0 = v_0` e `v` a ply `k`; `supp(v) = P^v[B_v]` sono i **bloccanti finali** (vertici); `p_v = max B_v` il ply del più profondo (`-1` se nessuno). Sia **`R`** l'insieme dei vertici raggiungibili da `v_0` con cammini di soli vertici `N` (`R = ∅` se `v_0 ∉ N`). Ogni figlio di un vertice `N` ha un'entry (un nodo NIL esamina tutti i figli), quindi un figlio `N` di un vertice di `R` sta in `R`.

**Teorema 4.8-A (enunciato per la tabella).** Dopo una ricerca completa, se `v_0 ∈ N` allora `eval(v) ≠ g` per ogni `v ∈ R`; equivalentemente, per ogni cammino `⟨v_0, …, v_n⟩` di `G` con tutti i `v_i ∈ N` si ha `eval(v_n) ≠ g`.

È la lettura che fa il verificatore di §7.9 della spec e il model checker `drawStrategyFails` di `test.cpp` (il difensore sceglie figli `N` da un nodo `N` o figli `F`, l'attaccante qualsiasi; fallisce se manca la scelta o si raggiunge un sink vinto da `g`). I due enunciati sono equivalenti: se il teorema vale, da un nodo `N` dell'attaccante nessun figlio è `W` (altrimenti `eval = g`), da un nodo `N` del difensore esiste un figlio `N` o `F` (uno con `eval ≠ g`, che non è `W` per solidità ed ha un'entry), i nodi `F` hanno figli `F` con la stessa regola del paper e i sink raggiunti non sono vinti da `g`; viceversa se il model checker passa, il difensore ha una strategia che resta in `R ∪ F` o termina in sink non vinti da `g`, quindi `g` non forza la vittoria da alcun vertice di `R`.

### 10.2 Riduzione al Lemma H1

**Lemma H1 (chiusura di `R` sotto i bloccanti).** Per ogni `v ∈ R`: `supp(v) ⊆ R`.

**Lemma 6.** H1 implica il Teorema 4.8-A.

*Dimostrazione.* Per assurdo esista `v ∈ R` con `eval(v) = g`; lo si scelga con `d_g(v)` minimo. Per (F2) e la costruzione del Lemma 4.6 del paper esiste un albero vincente `T` da `v` in cui ogni nodo a profondità `j` ha `d_g ≤ d_g(v) − j`; in particolare ogni nodo di `T` diverso dalla radice ha `d_g < d_g(v)`. Per l'invariante (I) (Lemma 3, valido per la variante A perché la sua regola di eredità è ammissibile) ogni albero vincente da `v` tocca `supp(v)`, che è un insieme di antenati **propri** di `v`: dunque `T` contiene un `y ∈ supp(v)`, `y ≠ v`, con `eval(y) = g` (F1 applicata al sottoalbero) e `d_g(y) < d_g(v)`. Per H1 `y ∈ R`, contro la minimalità di `d_g(v)`. ∎

Il caso `n = 0` del teorema (la radice) è il Teorema 4 di §5: `supp(v_0) = ∅`.

### 10.3 Dimostrazione di H1 a meno del Lemma 9

Fatti usati (tutti dimostrati qui).

- **(U) Unicità del frame per prefisso.** Un vertice è espanso al più una volta sotto un dato cammino di antenati `π`: dopo un'espansione NIL sotto `π` l'entry ha bloccanti a ply `≤ |π| − 1`, e ogni visita successiva con lo stesso prefisso la salta; dopo un esito PROVEN non è più espanso.
- **(S) Entry stantia.** Dopo `E_v` (con `p_v ≥ 0`) ogni visita di `v` è un salto con `path[0..p_v] = P^v[0..p_v]`, quindi avviene dentro il frame di `a_{p_v}` sotto il prefisso `P^v[0..p_v−1]` (unico per (U)); terminato quel frame `v` non è più visitato né GRAY.
- **(Inh) Eredità.** Se nell'espansione `E_x` di `x` il figlio `w` dà NIL per espansione `E'_w` o per salto dell'entry scritta da `E''_w`, allora `supp(E'_w) \ {x} ⊆ supp(E_x)` (per (R3)/(R4) e `inherit_skip`; nel salto i vertici coincidono per la condizione sul prefisso). Se `w` è GRAY, `w ∈ supp(E_x)`.
- **(T) Ordine temporale dei padri.** Sia `u ∈ R` un padre di `v ∈ R` (arco `u → v`). In `E_u` il figlio `v` è esaminato (un nodo NIL non si ferma) con esito NIL, in uno di quattro modi: **(a)** espansione `E_v` stessa (`u = a_{k−1}` e `E_u ⊃ E_v`); **(b)** salto dell'entry finale di `v` (dopo `E_v`, dentro `E_u`); **(c)** espansione o salto di un'entry **non** finale (allora `E_u` termina prima che `E_v` inizi: dentro `E_u`, dopo l'esame, `v` è sempre saltato per (S) applicata all'entry di allora); **(d)** `v` GRAY (`E_u` è dentro un'espansione di `v`). In (a) e (b) `E_u` termina dopo `E_v` e, per (Inh), **`supp(v) \ {u} ⊆ supp(u)`**.

**Lemma 8 (induzione).** Si supponga il Lemma 9 qui sotto. Allora H1 vale.

*Dimostrazione.* Induzione sui vertici di `R` in ordine **decrescente** dell'istante di fine di `E_v` (il primo è `v_0`, con `supp(v_0) = ∅`). Siano `v ∈ R` e `y ∈ supp(v)`.
1. Se esiste `u ∈ R` con `E_u` che termina dopo `E_v` e `y ∈ supp(u)`: per ipotesi induttiva `supp(u) ⊆ R`, quindi `y ∈ R`.
2. Altrimenti, se qualche padre `u ∈ R` di `v` ha esaminato `v` nel modo (a) o (b): per (T) `supp(v) \ {u} ⊆ supp(u)` con `E_u` che termina dopo `E_v`; se `y ≠ u` si ricade nel caso 1 (escluso), quindi `y = u ∈ R`.
3. Altrimenti valgono le ipotesi (M) e (P) del Lemma 9, che dà `y ∈ R`. ∎

**Lemma 9 (caso residuo, APERTO).** Siano `v ∈ R` e `y ∈ supp(v)` tali che **(M)** nessun `u ∈ R` la cui ultima espansione termina dopo `E_v` ha `y ∈ supp(u)`, e **(P)** nessun padre `u ∈ R` di `v` ha esaminato `v` in `E_u` nel modo (a) o (b) (tutti i padri in `R` lo hanno esaminato nel modo (c) o (d)). Allora `y ∈ R`.

Conseguenze delle ipotesi, dimostrabili con (S), (U), (Inh): per (M) ogni frame `a_l` fra `y` e `v` che termina NIL nella catena di propagazione da `v` (e quindi eredita `y`) non è l'ultima espansione di un vertice di `R`; per (P) il padre `a_{k−1}` non è un vertice di `R` con frame finale, e se il suo frame non è finale la sua riespansione successiva termina PROVEN prima di esaminare `v` quando `p_v = k−1`. Empiricamente (§10.4, proprietà "(d) annidato") in (d) l'espansione di `v` che contiene `E_u` è sempre `E_v` stessa (altrimenti `v` sarebbe un bloccante di `u` con frame non finale, contro F1). Queste conseguenze non bastano a chiudere il lemma: il fuzz mostra (§10.5) che nel caso residuo il bloccante `y` può stare **sotto** un antenato `W`/`F` di `v` ed essere in `R` solo grazie a un altro padre, estraneo al cammino `P^v`.

### 10.4 Misure

Driver nello scratchpad (Appendice), compilati contro l'header del repository (variante A) e, per la riga B, contro una copia in cui `inherit_skip` usa sempre il ripiego "prefisso intero" (è il caso di un `Prover` che legge entry scritte da un altro, come in `wintree-chess`). Grafi casuali con il generatore di `graph.h` (`n` vertici, grado 1-4, densità di cicli 0,3-0,95, sink 0,05-0,3), due goal per grafo, ricerca `NOWIN` su tabella vuota; su ogni esito si verificano solidità, completezza alla radice, l'enunciato diretto (ogni vertice di `R` ha `eval ≠ g`, con l'oracolo) e il model checker.

| variante | `n` | ricerche | radici `N` | Thm 4.8 fallito | radici incomplete |
|---|---|---|---|---|---|
| A | 4-8 | 10 000 000 | 9 096 925 | 0 | 0 |
| A | 6-12 | 10 000 000 | 8 934 597 | 0 | 0 |
| A | 8-16 | 6 000 000 | 5 287 466 | 0 | 0 |
| A | 5-10 | 10 000 000 | 9 010 022 | 0 | 0 |
| A | 4-9, 6-12 (`cexL2`, `cexV`: enunciato diretto e Lemma V) | 1 600 000 | 1 119 362 | 0 | 0 |
| B (ripiego) | 4-10 | 8 000 000 | 7 233 723 | 0 | 0 |
| B (ripiego) | 8-18 | 4 000 000 | 3 513 242 | 0 | 0 |

Sulla stessa distribuzione, con l'header strumentato (bloccanti finali come vertici, frame e istanti di ogni espansione), valgono senza eccezioni le proprietà seguenti, più forti di H1:

| proprietà (per ogni `v ∈ R`) | ricerche | violazioni |
|---|---|---|
| **H1** `supp(v) ⊆ R` | > 20 000 000 | 0 |
| **F1** ogni `y ∈ supp(v)` è `N` e il frame di `y` che contiene `E_v` è l'ultima espansione di `y` | 600 000 (n 4-9) + 4 000 000 (n 6-24, 13·10^6 bloccanti) | 0 |
| **Λ** nessun bloccante sotto l'antenato più basso che è in `R` con frame finale | 2 600 000 | 0 |
| **D** i bloccanti "diretti" (figli GRAY di `v`, insiemi ereditati da figli con entry non finale) sono in `R` | 600 000 | 0 |
| **(d) annidato** se `u ∈ R` esamina `v ∈ R` come GRAY, `E_u ⊂ E_v` | 2 600 000 | 0 (su 1,1·10^6 casi (d)) |
| **Lemma V** ogni albero vincente dalla fine di un cammino NIL semplice tocca il cammino | 3 952 811 cammini | 0 |

Il caso residuo del Lemma 9 compare: su 2·10^6 ricerche con `n` 4-10, 533 vertici di `R` hanno solo padri (c)/(d) (471 solo (c), 20 solo (d), 42 misti), per 272 coppie `(v, y)` valgono (M) e (P) insieme; su 6·10^6 ricerche con `n` 6-24, 3 631 coppie. In tutte `y ∈ R`.

### 10.5 Rafforzamenti falsi e perché la dimostrazione del paper non si trasferisce

La dimostrazione del Thm 4.8 del paper usa un istante in cui `v_0, …, v_{n−1}` sono GRAY insieme e `v_n` viene visitato (le chiamate ricorsive sono annidate lungo il cammino). Con la tabella il cammino NIL è letto a posteriori: `E_{v_{i+1}}` non è in generale dentro `E_{v_i}` (modi (b), (c), (d) di (T)), e l'entry finale di `v_{i+1}` può essere scritta sotto antenati che poi diventano `W` o `F`. Gli enunciati seguenti chiuderebbero il Lemma 9 ma sono **falsi** (controesempi trovati dal fuzz, grafi nel formato di `Gen.java`, `g = 1` salvo indicazione; la verifica si ripete con `dbg/showone FILE [goal]`):

1. *"Ogni `v ∈ R` ha un padre in `R` la cui ultima espansione ha visto l'entry finale di `v`"* (C1, che renderebbe superfluo il Lemma 9). Falso: `0 2 . 4 1 / 1 1 . 3 2 / 2 2 1 / 3 2 . 4 / 4 1 . 0 3`: il vertice 3 è in `R` (`0 → 4 → 3`), ma la sua entry finale è scritta sotto il padre 1, che diventa `W`; l'unico padre in `R` (4) lo ha esaminato prima, con un'espansione non finale.
2. *"Il cammino `P^v` dell'ultima espansione di `v ∈ R` è tutto in `N`"*, o anche solo il prefisso fino al bloccante più profondo (H9). Falso: `0 1 . 1 4 / 1 2 . 3 5 2 / 2 1 . 4 / 3 1 . 1 4 / 4 2 . 2 0 / 5 1 2`: il vertice 2 ha `supp = {4}` con `P^2 = (0, 1, 3, 4)` e `1 ∈ F`; il bloccante 4 è in `R` perché la radice ne salta l'entry finale. Lo stesso accade nel caso residuo (6 coppie su 3 631 con `n ≤ 24`).
3. *"I livelli (cammino NIL minimo) decrescono dal vertice ai suoi bloccanti"* (H3): falso (`0 1 . 2 1 / 1 2 . 4 / 2 2 . 3 0 4 / 3 1 . 2 5 / 4 1 . 5 2 / 5 2 . 0 3 4`: il vertice 3, livello 2, ha il bloccante 5 di livello 3).
4. *"`supp(v) \ {r} ⊆ supp(r)` con `r` l'antenato più basso in `R` con frame finale"* (Λ'): falso (115 casi su 2·10^6 ricerche, `n` 4-10), benché Λ valga.
5. *"Nel caso residuo `supp(v) ⊆ supp(u') ∪ {u'}` per un padre `u'` di tipo (c)/(d)"* (eredità all'indietro nel tempo, che permetterebbe un'induzione crescente): falso anche escludendo la radice fra i bloccanti (5 casi su 2·10^6 ricerche, `n` 8-16).
6. *"L'invariante H1 vale a ogni istante della ricerca, con `R_t` definito su GRAY ∪ entry correnti"*: falso (entry stantie in `R_t` con bloccanti divenuti `W`, poi riespanse: 418 violazioni su 2·10^5 ricerche), quindi la prova non può essere un invariante temporale sulla tabella corrente.

Conseguenze. **Per il motore** nulla cambia: il certificato di patta è emesso dal verificatore di §7.9, che controlla il grafo di strategia posizione per posizione, e §7.9 punto 3 già classifica il lemma sul dejavu come desiderabile per l'efficienza, non necessario per la correttezza; il model checker del test resta il controllo giusto (equivalenza in §10.1) e il fuzz di `make -C src/wintree test` continua a eseguirlo. **Per la teoria**, il Lemma 9 è l'unico anello mancante: una via è dimostrare prima F1 (i frame dei bloccanti sono ultime espansioni), da cui ogni bloccante `y` di `v ∈ R` ha `E_y ⊃ E_v` e il caso residuo si riduce a mostrare che `y` ha un padre in `R`; un'altra è un'induzione sul tempo d'inizio con un enunciato intermedio, visto che i padri di tipo (c) terminano prima di `E_v` mentre quelli di tipo (a)/(b) terminano dopo (nessuna delle due direzioni, da sola, copre entrambi i tipi).

## Appendice: materiale di verifica

Nello scratchpad della sessione (`/tmp/claude-1000/-home-dev-Scrivania-WinTreeChess/e564df6a-bddc-41fb-a58d-2351d9d86ca3/scratchpad/`): `cex.cpp` (driver: `cex FILE` esiti e oracolo nelle due modalità con dejavu on/off e model checker del Thm 4.8; `cex --search SEMI NMIN NMAX` ricerca casuale di radici incomplete; `cex --count SEMI` violazioni e Σ espansioni sulla distribuzione di `Gen.java`), `cex12.txt` e `cex15.txt` (i grafi di §6), `varA/` e `varB/` (copie patchate di `wintree.h` con `graph.h`, `dfpn.h`, `test.cpp` e il binario `wintree_test`). Compilazione: `g++ -std=c++17 -O2 -I<dir con wintree.h> cex.cpp`. La variante A è stata poi applicata al repository (§8), insieme alle regressioni P4/P4bis e all'aggiornamento di `src/wintree/README.md`; `dfpn.h` e l'ibrido non sono toccati.

Materiale di §10 (scratchpad della sessione del 2026-10-28, `/tmp/claude-1000/-home-dev-Scrivania-WinTreeChess/48751ff9-354f-4900-8ea0-5bae38531424/scratchpad/`): `cex48.cpp` (fuzz del Thm 4.8 sull'header del repository: `cex48 --search SEMI NMIN NMAX [SEME0]` con controllo diretto del cammino NIL e model checker, `cex48 FILE` esiti su un grafo); `varB/` (copia di `wintree.h` con il solo ripiego "prefisso intero" e il fuzz `cex48B`); `dbg/wintree.h` (copia strumentata: bloccanti finali come vertici, cammino e numeri di frame dell'ultima espansione, esito e numero d'espansione di ogni esame figlio, contributi ai bloccanti, istanti di fine) con i driver `cexL2`, `cexC1`, `cexD`, `cexF`, `cexLam`, `cexHard`, `cexMax`, `cexCK`, `cexRC`, `cexV`, `cexInv`, `showone` usati per le misure di §10.4 e per i controesempi di §10.5. Compilazione: `g++ -std=c++17 -O2 -I<dir con wintree.h> file.cpp`.
