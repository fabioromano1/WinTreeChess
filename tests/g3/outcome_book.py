#!/usr/bin/env python3
"""G3 LTC, set scelto per esito (README-ltc.md §6). Il lato in vantaggio è sempre il lato al tratto della FEN.
  candidates <games.csv> <mates.csv> <out.epd> <out.csv> [escludi.csv]: unisce i candidati di outcome_candidates.py (classe G) e di
    mate_candidates.py (classe M; i candidati da partita sono F con <= 10 pezzi, P con 11-32), toglie le FEN duplicate e le posizioni a 6 pezzi che le 6-man non danno vinte per il
    lato al tratto (nessuno può convertirle) e quelle già in escludi.csv; ordine: partite poi M, come nei CSV.
  book <cand.csv> <filter.pgn> <out.epd> <out.csv>: tiene i candidati con almeno una partita del filtro (off contro off)
    che non è una vittoria del lato al tratto, tranne quelli da partita (F, P) persi in entrambe dal lato al tratto (il +3
    era sbagliato: off converte per l'altro lato; i matti M restano, il matto è forzato); mescola con seme fisso; stampa la distribuzione."""
import csv, os, random, subprocess, sys
from collections import Counter, defaultdict
from pgn_games import games

key = lambda fen: " ".join(fen.split()[:4])
cnt = lambda fen, s: sum(c in s for c in fen.split()[0])

def wdl6(fens):   # come hard_endgames.wdl: go depth 1 con la radice in TB
    tb = ":".join(os.path.expanduser(p) for p in ("~/syzygy/5man", "~/syzygy/6man"))
    p = subprocess.Popen([os.environ.get("EXE", "../../src/stockfish-hybrid/stockfish")], stdin=subprocess.PIPE,
                         stdout=subprocess.PIPE, text=True, bufsize=1)
    p.stdin.write(f"setoption name Hash value 16\nsetoption name Prover value off\nsetoption name SyzygyPath value {tb}\n")
    res = {}
    for f in fens:
        p.stdin.write(f"ucinewgame\nposition fen {f}\ngo depth 1\n"); p.stdin.flush()
        last = ""
        while not (l := p.stdout.readline()).startswith("bestmove"):
            if " score " in l: last = l
        t = last.split()
        if "tbhits" not in t or int(t[t.index("tbhits") + 1]) == 0: res[f] = ""; continue
        v = int(t[t.index("score") + 2]) if t[t.index("score") + 1] == "cp" else 0
        res[f] = "W" if v >= 10000 else "L" if v <= -10000 else "D"
    p.stdin.write("quit\n"); p.stdin.flush()
    return res

def write(rows, epd, out):
    open(epd, "w").writelines(r["fen"] + "\n" for r in rows)
    with open(out, "w", newline="") as f:
        w = csv.DictWriter(f, fieldnames=list(rows[0])); w.writeheader(); w.writerows(rows)

def candidates(gcsv, mcsv, epd, out, excl=None):
    rows, seen = [], {key(r["fen"]) for r in csv.DictReader(open(excl))} if excl else set()
    for r in csv.DictReader(open(gcsv)):
        rows.append(dict(fen=r["fen"], cls="F" if int(r["pieces"]) <= 10 else "P", pieces=r["pieces"], pawns=r["pawns"], cp=r["eval"], wdl6="",
                         provenance=f"{r['source']} round {r['round']} {r['white']}-{r['black']} ply {r['ply']} "
                                    f"(lato al tratto: {'patta' if r['side_result'] == 'D' else 'sconfitta'})"))
    for r in csv.DictReader(open(mcsv)):
        rows.append(dict(fen=r["fen"], cls="M", pieces=r["pieces"], pawns=r["pawns"], cp=f"#{r['mate']}", wdl6="",
                         provenance=f"tests/suites/mates2000.epd riga {int(r['idx']) + 1} (off a 1500 ms: {r['score']})"))
    rows = [r for r in rows if key(r["fen"]) not in seen and not seen.add(key(r["fen"]))]
    w = wdl6([r["fen"] for r in rows if int(r["pieces"]) == 6])
    for r in rows: r["wdl6"] = w.get(r["fen"], "")
    drop = [r for r in rows if int(r["pieces"]) == 6 and r["wdl6"] != "W"]
    rows = [r for r in rows if r not in drop]
    write(rows, epd, out)
    print(f"{len(rows)} candidati ({', '.join(f'{c} {n}' for c, n in sorted(Counter(r['cls'] for r in rows).items()))}); "
          f"tolti a 6 pezzi non vinti in TB: {len(drop)} {Counter(r['wdl6'] for r in drop)}")

def book(ccsv, pgn, epd, out):
    res = defaultdict(list)   # FEN -> punti del lato al tratto della FEN di partenza, per partita
    for g in games(pgn):
        f, r = g["tags"]["FEN"], g["tags"]["Result"]
        res[key(f)].append({"1/2-1/2": 0.5, "1-0": 1.0, "0-1": 0.0}[r] if f.split()[1] == "w" else
                           {"1/2-1/2": 0.5, "1-0": 0.0, "0-1": 1.0}[r])
    rows = list(csv.DictReader(open(ccsv)))
    assert all(len(res[key(r["fen"])]) == 2 for r in rows), "ogni candidato deve avere 2 partite nel filtro"
    for r in rows: r["filter"] = "/".join(str(x) for x in res[key(r["fen"])])
    kept = [r for r in rows if min(res[key(r["fen"])]) < 1 and (r["cls"] == "M" or max(res[key(r["fen"])]) > 0)]
    random.Random(0).shuffle(kept)
    write(kept, epd, out)
    print(f"Candidati {len(rows)}, nel set {len(kept)}\n")
    print("| Pezzi | candidati | nel set | punti del lato in vantaggio nel filtro (2 partite) 0/0,5/1/1,5/2 |\n|---|---:|---:|---|")
    band = lambda n: "6-7" if n <= 7 else "8-10" if n <= 10 else "11-16" if n <= 16 else "17-32"
    for b in ("6-7", "8-10", "11-16", "17-32"):
        for c in "FPM":
            rs = [r for r in rows if r["cls"] == c and band(int(r["pieces"])) == b]
            if rs:
                s = Counter(sum(res[key(r["fen"])]) for r in rs)
                print(f"| {c} {b} | {len(rs)} | {sum(r in kept for r in rs)} | "
                      f"{'/'.join(str(s[x]) for x in (0, 0.5, 1, 1.5, 2))} |")

if __name__ == "__main__":
    {"candidates": candidates, "book": book}[sys.argv[1]](*sys.argv[2:])
