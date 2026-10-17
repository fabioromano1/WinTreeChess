#!/usr/bin/env python3
"""Tabelle markdown del README da uno o più CSV di run.py: verdetto per configurazione, cause delle non certificate,
frazione certificata entro T s di CPU (dalle corse lunghe), tabella per posizione per il set (c).
Uso: report.py verdict CSV...  |  report.py cpu CSV T  |  report.py lit CSV..."""
import csv, statistics, sys

def load(p): return list(csv.DictReader(open(p)))
def med(v): return statistics.median(v) if v else float("nan")

def verdict(paths):
    print("| Corsa | Posizioni | Certificate | PROVEN_NOWIN | UNKNOWN_CYCLE + grafo | Espansioni mediane (cert.) | Tempo mediano (cert.) | CPU/tempo mediano | Budget | Sink 50 mosse | Entry sostituita | Taint non chiuso | Altro |")
    print("|---|---|---|---|---|---|---|---|---|---|---|---|---|")
    for p in paths:
        r = load(p); c = [x for x in r if x["cert"] == "1"]; n = len(r)
        cnt = lambda k: sum(x["cause"].startswith(k) for x in r)
        other = n - len(c) - cnt("budget") - cnt("sink") - cnt("entry") - cnt("taint")
        print(f"| `{p.split('/')[-1]}` | {n} | **{len(c)} ({100*len(c)/max(n,1):.1f} %)** | {sum(x['status']=='PROVEN_NOWIN' for x in c)} | {sum(x['status']=='UNKNOWN_CYCLE' for x in c)}"
              f" | {med([int(x['nodes']) for x in c]):.0f} | {med([float(x['secs']) for x in c]):.2f} s | {med([float(x['cpu'])/max(float(x['secs']),1e-9) for x in r]):.2f}"
              f" | {cnt('budget')} | {cnt('sink')} | {cnt('entry')} | {cnt('taint')} | {other} |")

def cpu(path, t):                       # corsa lunga: certificate con cpu <= t (equivalente a una corsa di t s su macchina scarica)
    r = load(path); t = float(t)
    for wdl in [None] + sorted({x["wdl"] for x in r}):
        s = [x for x in r if wdl is None or x["wdl"] == wdl]; c = [x for x in s if x["cert"] == "1"]
        print(f"{'tutte' if wdl is None else 'wdl=' + wdl}: {len(s)} posizioni, certificate entro {t:g} s CPU: {sum(float(x['cpu']) <= t for x in c)}"
              f" ({100*sum(float(x['cpu']) <= t for x in c)/max(len(s),1):.1f} %), entro il tetto: {len(c)} ({100*len(c)/max(len(s),1):.1f} %)")

def lit(paths):
    runs = [load(p) for p in paths]; names = [p.split("/")[-1].replace("g2-c-", "").replace(".csv", "") for p in paths]
    print("| # | Posizione | Pezzi | Goal | Eval SF | WDL | " + " | ".join(names) + " |"); print("|---|---|---|---|---|---|" + "---|" * len(names))
    for i, row in enumerate(runs[0]):
        cells = []
        for r in runs:
            x = next((y for y in r if y["fen"] == row["fen"]), None)
            cells.append("—" if not x else ("**cert.** " + x["status"] + " " + x["secs"][:5] + " s") if x["cert"] == "1" else x["cause"][:28])
        print(f"| {i+1} | `{row['fen'][:-4]}` | {row['pieces']} | {row['goal']} | {row['eval']} | {row['wdl']} | " + " | ".join(cells) + " |")

if __name__ == "__main__":
    {"verdict": lambda a: verdict(a), "cpu": lambda a: cpu(a[0], a[1]), "lit": lambda a: lit(a)}[sys.argv[1]](sys.argv[2:])
