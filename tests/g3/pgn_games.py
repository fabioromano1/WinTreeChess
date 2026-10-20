"""G3: lettore minimo dei PGN di fastchess (notation=uci). games(path) -> dict per partita: tag, mosse, commento per
mossa. fastchess mette nel commento della mossa anche le righe di match_line (`line="..."`) e la causa di fine partita."""
import re

TOKEN = re.compile(r"\{([^}]*)\}|(\S+)")

def games(path):
    tags, body = {}, []
    for line in open(path, encoding="utf-8", errors="replace"):
        if line.startswith("["):
            if body: yield parse(tags, body); tags, body = {}, []
            k, v = line[1:].split(" ", 1); tags[k] = v.strip()[1:-2]
        elif line.strip(): body.append(line)
    if tags: yield parse(tags, body)

def parse(tags, body):
    moves, comments = [], []
    for m in TOKEN.finditer(" ".join(body)):
        c, w = m.groups()
        if c is not None:
            if moves: comments[-1] = c.strip()
        elif re.fullmatch(r"[a-h][1-8][a-h][1-8][qrbn]?", w):
            moves.append(w); comments.append("")
    return dict(tags=tags, moves=moves, comments=comments)

if __name__ == "__main__":   # self-check su un PGN sintetico nel formato di fastchess 1.8.2
    import tempfile, os
    t = tempfile.NamedTemporaryFile("w", suffix=".pgn", delete=False)
    t.write('[White "a"]\n[Result "1-0"]\n[FEN "8/8/8/8/8/8/8/K6k w - - 0 1"]\n\n1. a1a2 {+M7/1 0.017s, tl=1s, n=5, line="info string prover: proven mate in 4 move a1a2 nodes 4 time 7 ms"}\nh1h2 {-1.00/9 0.2s, tl=1s, n=9}\n2. a2a3 1-0\n')
    t.close(); g = next(games(t.name)); os.unlink(t.name)
    assert g["moves"] == ["a1a2", "h1h2", "a2a3"] and "proven mate in 4" in g["comments"][0] and g["comments"][1].startswith("-1.00/9"), g
    assert g["comments"][2] == "" and g["tags"]["FEN"].startswith("8/8") and g["tags"]["Result"] == "1-0"
    print("ok")
