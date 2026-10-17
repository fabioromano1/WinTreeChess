#!/usr/bin/env python3
"""Misura di G2 (spec §11): per ogni riga EPD (campo c0 "goal=<lato> eval=<cp> wdl=<n|?>") una ricerca `wintree-chess --mode nowin`
con goal = il lato che Stockfish crede vincente, tetto di tempo, un thread, TB come sink fino a --tbmax pezzi, `--verify`.
Certificata = PROVEN_NOWIN o radice UNKNOWN_CYCLE con grafo di strategia di patta chiuso (§7.9.2). CSV per riga + riepilogo.
Uso: run.py FILE.epd --csv OUT.csv [--tbmax 5] [--time 10] [--hash 256] [--jobs 4] [--count N] [--step K] [--label NOME]"""
import argparse, concurrent.futures, csv, os, re, statistics, subprocess, sys

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
BIN = os.path.join(ROOT, "src/wintree-chess/build/wintree-chess")
TB = os.path.expanduser("~/syzygy/5man") + ":" + os.path.expanduser("~/syzygy/6man")

def read_epd(path, count, step):
    rows = []
    for n, line in enumerate(open(path), 1):
        f = line.split(); m = re.search(r'c0 "goal=(white|black) eval=(-?\d+) wdl=(-?\d|\?)"', line)
        if len(f) < 4 or not m or line.startswith("#"): continue
        rows.append({"id": n, "fen": " ".join(f[:4]) + " 0 1", "pieces": sum(ch.isalpha() for ch in f[0]),
                     "goal": m.group(1), "eval": int(m.group(2)), "wdl": m.group(3)})
    rows = rows[::step]
    return rows[:count] if count else rows

def run(row, a):
    cmd = [BIN, "--fen", row["fen"], "--goal", row["goal"], "--mode", "nowin", "--tb", TB, "--tbmax", str(a.tbmax),
           "--time", str(a.time), "--hash", str(a.hash), "--nodes", "1000000000", "--verify"]   # solo il tempo limita (default: 10^6 espansioni)
    out = subprocess.run(cmd, capture_output=True, text=True).stdout
    g = lambda rx, d="": (re.search(rx, out) or [None, d])[1]
    r = dict(row, tbmax=a.tbmax, time=a.time, status=g(r"esito (\S+)"), req=g(r"req=(\d+)"), resid=g(r"resid=(\d+)"),
             nodes=g(r"nodi (\d+) do_move"), domove=g(r"do_move (\d+)"), secs=g(r"tempo (\S+) s"), cpu=g(r"cpu (\S+) s"),
             maxply=g(r"ply massimo (\d+)"), cert=int("verifica certificato" in out), why=g(r"NON certificato \((.*)\): nodi"),
             cert_nodes=g(r"verifica .*: nodi (\d+)"), cert_leaves=g(r"foglie (\d+)"), cert_tb=g(r"foglie \d+ TB (\d+)"))
    r["cause"] = cause(r)
    return r

# Classificazione delle non certificate (task G2, punto 4) dallo stato alla radice e dal motivo del verificatore.
def cause(r):
    if r["cert"]: return "certificata"
    if r["status"] == "UNKNOWN_BUDGET": return "budget"
    if r["status"] == "PROVEN_WIN": return "PROVEN_WIN (goal vince: non è una fortezza)"
    w = r["why"]
    if "(entry assente)" in w: return "entry sostituita"
    if "(entry PROVEN_NOWIN)" in w: return "sink 50 mosse/ripetizione"
    if "(entry UNKNOWN_CYCLE)" in w or "(entry UNKNOWN_BUDGET)" in w: return "taint alla radice senza grafo chiuso"   # figlio con entry aperta: sovrascritta da una ri-ricerca
    return "altro: " + w[:60]

def main():
    p = argparse.ArgumentParser(); p.add_argument("epd"); p.add_argument("--csv", required=True)
    p.add_argument("--tbmax", type=int, default=5); p.add_argument("--time", type=float, default=10); p.add_argument("--hash", type=int, default=256)
    p.add_argument("--jobs", type=int, default=4); p.add_argument("--count", type=int, default=0); p.add_argument("--step", type=int, default=1)
    p.add_argument("--label", default="")
    a = p.parse_args()
    rows = read_epd(a.epd, a.count, a.step)
    with concurrent.futures.ThreadPoolExecutor(a.jobs) as ex: res = list(ex.map(lambda r: run(r, a), rows))
    cols = ["id", "fen", "pieces", "goal", "eval", "wdl", "tbmax", "time", "status", "req", "resid", "nodes", "domove", "secs", "cpu", "maxply",
            "cert", "why", "cert_nodes", "cert_leaves", "cert_tb", "cause"]
    with open(a.csv, "w", newline="") as f:
        w = csv.DictWriter(f, cols); w.writeheader(); w.writerows(res)
    n = len(res); c = sum(r["cert"] for r in res)
    med = lambda k, rs: statistics.median([float(r[k]) for r in rs]) if rs else float("nan")
    cert = [r for r in res if r["cert"]]
    print(f"{a.label or a.epd}: tbmax {a.tbmax}, {a.time:g} s, {n} posizioni: certificate {c} ({100*c/max(n,1):.1f} %)"
          f" [PROVEN_NOWIN {sum(r['status']=='PROVEN_NOWIN' and r['cert'] for r in res)}, UNKNOWN_CYCLE+grafo {sum(r['status']=='UNKNOWN_CYCLE' and r['cert'] for r in res)}];"
          f" mediana certificate: espansioni {med('nodes', cert):.0f}, do_move {med('domove', cert):.0f}, tempo {med('secs', cert):.2f} s, grafo {med('cert_nodes', cert):.0f} nodi")
    for k in sorted({r["cause"] for r in res}): print(f"  {k}: {sum(r['cause']==k for r in res)}")
    for wdl in sorted({r["wdl"] for r in res}):
        sub = [r for r in res if r["wdl"] == wdl]; print(f"  wdl={wdl}: {sum(r['cert'] for r in sub)}/{len(sub)} certificate")

if __name__ == "__main__": main()
