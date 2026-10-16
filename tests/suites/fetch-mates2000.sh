#!/bin/sh
# Scarica (o rigenera) tests/suites/mates2000.epd da vondele/matetrack e ne verifica l'impronta sha256.
# Formato: 4 campi FEN, poi `bm #N;` (N > 0: il tratto matta in N mosse; N < 0: il tratto è mattato in |N| mosse).
set -eu
cd "$(dirname "$0")"
URL=https://raw.githubusercontent.com/vondele/matetrack/master/mates2000.epd
SHA=234fea364673c76e1690b7e01747c9cd35258c2779418b8aa1e0c2c58610c411
curl -fsSL -o mates2000.epd.tmp "$URL"
echo "$SHA  mates2000.epd.tmp" | sha256sum -c - >/dev/null
mv mates2000.epd.tmp mates2000.epd
echo "mates2000.epd: $(wc -l < mates2000.epd) posizioni, sha256 $SHA"
