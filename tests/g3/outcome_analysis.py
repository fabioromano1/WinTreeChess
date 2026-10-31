#!/usr/bin/env python3
"""G3 LTC, set scelto per esito (README-ltc.md §6): conversione del lato in vantaggio (lato al tratto della FEN) quando è
giocato da auto e quando è giocato da off, per classe del libro e per esito nel filtro. Punti(auto) - punti(off) del lato
in vantaggio in una coppia = punti di auto nella coppia - 1: è la stessa misura dell'Elo, con l'intervallo bootstrap
sulle POSIZIONI (le coppie sulla stessa posizione non sono indipendenti). Poi le coppie non pari, una per riga: chi ne
beneficia e se il vincitore aveva una prova certificata (riga `proven`: semimossa, valutazione della mossa PRECEDENTE dello
stesso motore, che è il punteggio che ha fatto partire il prover). Infine la conversione dopo il primo punteggio decisivo
(banda matto/TB, in centipedoni >= +100.00: il trigger del prover alla radice è `is_decisive(bestPreviousScore)`).
Uso: outcome_analysis.py <games.pgn> <libro.csv> [boot=10000]"""
import csv, random, re, sys
from collections import defaultdict
from pgn_games import games

AUTO = "prover-auto"
PROVEN = re.compile(r'line="info string prover: (proven [^"]*?) (?:move|nodes)')
EVAL = re.compile(r"^\+(M\d+|\d+\.\d+)/")
key = lambda fen: " ".join(fen.split()[:2])
decisive = lambda c: bool(m := EVAL.match(c)) and (m[1][0] == "M" or float(m[1]) >= 100)
pgn, book = sys.argv[1], sys.argv[2]
BOOT = int(sys.argv[3]) if len(sys.argv) > 3 else 10000
rows = {key(r["fen"]): r for r in csv.DictReader(open(book))}

def adv_points(res, fen):   # punti del lato al tratto della FEN di partenza
    return {"1/2-1/2": .5, "1-0": 1., "0-1": 0.}[res] if fen.split()[1] == "w" else {"1/2-1/2": .5, "1-0": 0., "0-1": 1.}[res]

assert decisive("+199.95/40 1s") and decisive("+M7/20 1s") and not decisive("+94.50/30 1s") and not decisive("-M3/9 1s")
assert adv_points("1-0", "x w") == 1 and adv_points("1-0", "x b") == 0 and adv_points("1/2-1/2", "x b") == .5

conv = {"auto": [0, 0], "off": [0, 0]}   # motore -> [partite in cui il motore raggiunge un punteggio decisivo, di cui vinte]
pairs = defaultdict(dict)   # Round -> {"auto" | "off": (punti del lato in vantaggio, prima prova, n. mosse, FEN)}
for g in games(pgn):
    t = g["tags"]; fen = t["FEN"]; white_stm = fen.split()[1] == "w"
    who = "auto" if (t["White"] == AUTO) == white_stm else "off"   # chi gioca il lato in vantaggio
    proof = next(((i, g["comments"][i - 2].split("/")[0] if i >= 2 else "-", PROVEN.search(c)[1])
                  for i, c in enumerate(g["comments"]) if PROVEN.search(c)), None)
    for w, side in (("auto", t["White"] == AUTO), ("off", t["White"] != AUTO)):   # side: il motore ha il bianco
        if any(decisive(c) and (i % 2 == 0) == (side == white_stm) for i, c in enumerate(g["comments"])):
            conv[w][0] += 1; conv[w][1] += t["Result"] == ("1-0" if side else "0-1")
    pairs[t["Round"]][who] = (adv_points(t["Result"], fen), proof, len(g["moves"]), fen)
full = [(r, p) for r, p in pairs.items() if len(p) == 2]
assert all(key(p["auto"][3]) == key(p["off"][3]) for _, p in full), "una coppia deve avere una sola posizione"

def boot(by_pos):   # intervallo al 95 % della media delle differenze, ricampionando le posizioni
    rng = random.Random(0); pos = list(by_pos); ms = []
    for _ in range(BOOT):
        s = [d for p in rng.choices(pos, k=len(pos)) for d in by_pos[p]]
        ms.append(sum(s) / len(s))
    ms.sort()
    return ms[int(.025 * BOOT)], ms[int(.975 * BOOT)]

def line(name, ps):
    if not ps: return
    wdl = lambda who: "/".join(str(sum(p[who][0] == x for _, p in ps)) for x in (1., .5, 0.))
    by = defaultdict(list); byw = defaultdict(list)
    for _, p in ps:
        k = key(p["auto"][3]); by[k].append(p["auto"][0] - p["off"][0])
        byw[k].append((p["auto"][0] == 1) - (p["off"][0] == 1))
    n = len(ps); d = sum(map(sum, by.values())) / n; lo, hi = boot(by)
    w = sum(map(sum, byw.values())) / n; wlo, whi = boot(byw)
    print(f"| {name} | {len(by)} | {n} | {wdl('auto')} | {wdl('off')} | {d:+.3f} [{lo:+.3f}; {hi:+.3f}] | {w:+.1%} [{wlo:+.1%}; {whi:+.1%}] |")

print("| Sottoinsieme | posizioni | coppie | V/P/S del lato in vantaggio giocato da auto | giocato da off | punti auto - off per coppia [95 %] | vittorie auto - off [95 %] |")
print("|---|---:|---:|---|---|---:|---:|")
cl = lambda p: rows[key(p["auto"][3])]
line("tutte", full)
for c in "FPM": line(f"classe {c}", [x for x in full if cl(x[1])["cls"] == c])
line("patta in entrambe le partite del filtro", [x for x in full if cl(x[1])["filter"] == "0.5/0.5"])
line("esiti diversi nel filtro", [x for x in full if len(set(cl(x[1])["filter"].split("/"))) == 2])

odd = sorted(((r, p) for r, p in full if p["auto"][0] != p["off"][0]), key=lambda x: int(x[0]))
plus = [x for x in odd if x[1]["auto"][0] > x[1]["off"][0]]; minus = [x for x in odd if x[1]["auto"][0] < x[1]["off"][0]]
print(f"\nCoppie non pari: {len(odd)}; a favore di auto {len(plus)}, a favore di off {len(minus)}.")
print(f"A favore di auto con prova certificata di auto col lato in vantaggio: {sum(bool(p['auto'][1]) for _, p in plus)}; "
      f"a favore di off con prova di auto col lato in vantaggio: {sum(bool(p['auto'][1]) for _, p in minus)}.")
print(f"Coppie pari, lato in vantaggio vince con auto e con off: {sum(p['auto'][0] == p['off'][0] == 1 for _, p in full)}; "
      f"auto prova la vittoria in {sum(p['auto'][0] == p['off'][0] == 1 and bool(p['auto'][1]) for _, p in full)} di queste.")
print(f"\nPartite in cui il motore raggiunge un punteggio decisivo a suo favore, di cui vinte: auto {conv['auto'][1]} su "
      f"{conv['auto'][0]}, off {conv['off'][1]} su {conv['off'][0]}.")
print("\n| Round | classe | filtro | lato in vantaggio: auto | off | mosse auto / off | prima prova di auto col lato in vantaggio: semimossa, eval della mossa precedente, esito |")
print("|---:|---|---|---:|---:|---|---|")
for r, p in odd:
    b = cl(p); pr = p["auto"][1]
    print(f"| {r} | {b['cls']} {b['pieces']}p {b['cp']} | {b['filter']} | {p['auto'][0]:g} | {p['off'][0]:g} | {p['auto'][2]} / {p['off'][2]} | "
          f"{f'{pr[0]}, {pr[1]}, {pr[2]}' if pr else 'nessuna'} |")
