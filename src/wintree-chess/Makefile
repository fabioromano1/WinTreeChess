# M2: wintree-chess = nucleo WinTree (../wintree) + adapter scacchi, verificatore, baseline df-pn, CLI, regressioni,
# G0 e benchmark G1, linkato con i sorgenti di Stockfish 19 non modificati (tutti tranne main.cpp: tt.cpp e
# position.cpp trascinano ThreadPool e UCIEngine, quindi quasi tutto il motore). Opzioni di compilazione: le stesse
# che il Makefile di Stockfish sceglie per ARCH=native (lette da `make config-sanity`), senza LTO (link in secondi)
# e senza la rete NNUE incorporata (-DNNUE_EMBEDDING_OFF: il prover non la usa; la ricerca di Stockfish per
# l'ordinamento a caldo e per `go mate` la carica dal file $(NNUE)).
SF      := ../../Stockfish-sf_19/src
SFFLAGS := $(filter-out -flto%,$(shell $(MAKE) -s --no-print-directory -C $(SF) config-sanity ARCH=native 2>/dev/null | sed -n 's/^CXXFLAGS: *//p')) -DNNUE_EMBEDDING_OFF
LDLIBS  := -lpthread -lrt
SFSRCS  := $(filter-out $(SF)/main.cpp,$(wildcard $(SF)/*.cpp $(SF)/nnue/*.cpp $(SF)/nnue/features/*.cpp $(SF)/syzygy/*.cpp))
SFOBJS  := $(patsubst %.cpp,build/sf/%.o,$(notdir $(SFSRCS)))
TB      ?= $(HOME)/syzygy/5man
G0      ?= 1000
SEED    ?= 1
NODES   ?= 200000
NNUE    ?= $(abspath $(SF))/nn-1a298aa575a0.nnue
SUITE   ?= ../../tests/suites/mates2000.epd
RESULTS ?= ../../tests/results
TIME    ?= 10
THREADS ?= 4
HASH    ?= 256
BENCH   := ./build/wintree-chess --suite $(SUITE) --time $(TIME) --threads $(THREADS) --nnue $(NNUE) --hash $(HASH)

vpath %.cpp $(SF) $(SF)/nnue $(SF)/nnue/features $(SF)/syzygy

.PHONY: all test g0full bench-g1 bench-g1-full clean

all: build/wintree-chess

build/wintree-chess: build/main.o $(SFOBJS)
	$(CXX) $(SFFLAGS) -o $@ $^ $(LDLIBS)

build/main.o: main.cpp bench.h solve.h warm.h dfpn.h chess_game.h verifier.h ../wintree/wintree.h | build/sf
	$(CXX) $(SFFLAGS) -I$(SF) -c -o $@ $<

build/sf/%.o: %.cpp | build/sf
	$(CXX) $(SFFLAGS) -I$(SF) -c -o $@ $<

build/sf:
	mkdir -p build/sf

# regressioni + campione ridotto di G0 (pochi minuti); il campione completo da 10^4 posizioni: make g0full
test: build/wintree-chess
	./build/wintree-chess --regress --tb $(TB)
	./build/wintree-chess --g0 $(G0) --seed $(SEED) --nodes $(NODES) --tb $(TB)

g0full: build/wintree-chess
	./build/wintree-chess --g0 10000 --seed 1 --nodes $(NODES) --tb $(TB)

$(SUITE):
	sh $(dir $(SUITE))fetch-mates2000.sh

# G1 rapido (§11): B6 su 100 posizioni (una ogni 20), tetto $(TIME) s per solver, $(THREADS) posizioni in parallelo;
# poi B2 (nodi/s vs perft) su 20 posizioni, sequenziale. ~10-20 min.
bench-g1: build/wintree-chess $(SUITE)
	$(BENCH) --step 20 --solvers cold,warm,dfpn,dfpn-warm,sf --csv $(RESULTS)/g1-quick.csv
	./build/wintree-chess --b2 $(SUITE) --count 20 --depth 4 --csv $(RESULTS)/b2.csv

# G1 completo: B6 sull'intera suite, poi B4 (ordinamento) e B5 (O6, dejavu) su 200 posizioni (una ogni 10), in
# background con log in $(RESULTS)/bench-g1-full.log. Ore: seguire con `tail -f`.
bench-g1-full: build/wintree-chess $(SUITE)
	nohup sh -c '$(BENCH) --solvers cold,warm,dfpn,dfpn-warm,sf --csv $(RESULTS)/g1-full.csv && \
	             $(BENCH) --step 10 --solvers random,cold,warm --csv $(RESULTS)/b4-order.csv && \
	             $(BENCH) --step 10 --solvers cold,cold-noo6,cold-nodejavu,warm-noo6,warm-nodejavu --csv $(RESULTS)/b5-ablation.csv' \
	      > $(RESULTS)/bench-g1-full.log 2>&1 &
	@echo "avviato in background: tail -f $(RESULTS)/bench-g1-full.log"

clean:
	rm -rf build
