#!/usr/bin/env python3
"""Oracolo esterno per le posizioni a 7 pezzi del set (c) (spec §12.2, autorizzato: API tablebase di Lichess, sola lettura,
una richiesta per posizione, cache su file). Riscrive l'EPD: nel campo c0 `wdl=?` diventa il WDL dal punto di vista del goal
(win 2, cursed-win 1, draw 0, blessed-loss -1, loss -2) e si aggiunge `c2 "lichess=<category> dtz=<n> dtm=<n>";`.
Uso: lichess_wdl.py FILE.epd [CACHE.json]   (le righe con più di 7 pezzi o con wdl già noto restano intatte)"""
import json, os, re, sys, time, urllib.parse, urllib.request

CAT = {"win": 2, "cursed-win": 1, "draw": 0, "blessed-loss": -1, "loss": -2}

def probe(fen, cache):
    if fen not in cache:
        url = "https://tablebase.lichess.ovh/standard?fen=" + urllib.parse.quote(fen.replace(" ", "_"))
        with urllib.request.urlopen(urllib.request.Request(url, headers={"User-Agent": "wintree-g2"}), timeout=30) as r:
            cache[fen] = json.load(r)
        time.sleep(1.5)                                        # cortesia verso l'API pubblica
    return cache[fen]

def main(path, cache_path):
    cache = json.load(open(cache_path)) if os.path.exists(cache_path) else {}
    out = []
    for line in open(path):
        line = line.rstrip("\n"); f = line.split()
        m = re.search(r'c0 "goal=(white|black) eval=(-?\d+) wdl=\?"', line)
        if len(f) < 4 or not m or sum(ch.isalpha() for ch in f[0]) != 7:
            out.append(line); continue
        fen = " ".join(f[:4]) + " 0 1"
        j = probe(fen, cache); cat = j.get("category", "unknown")
        if cat not in CAT: out.append(line); print("categoria non decisa:", cat, fen, file=sys.stderr); continue
        wdl = CAT[cat] if m.group(1)[0] == f[1] else -CAT[cat]
        line = line.replace("wdl=?", "wdl=%d" % wdl) + ' c2 "lichess=%s dtz=%s dtm=%s";' % (cat, j.get("dtz"), j.get("dtm"))
        out.append(line)
    json.dump(cache, open(cache_path, "w"), indent=0)
    open(path, "w").write("\n".join(out) + "\n")

if __name__ == "__main__":
    main(sys.argv[1], sys.argv[2] if len(sys.argv) > 2 else os.path.join(os.path.dirname(sys.argv[1]), "lichess-cache.json"))
