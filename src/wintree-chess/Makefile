# M2: wintree-chess = nucleo df-pn (../wintree/dfpn.h, default) e WinTree depth-first (../wintree/wintree.h) + adapter
# scacchi, verificatore, baseline df-pn di M2b, CLI, regressioni, G0 e benchmark G1, linkato con i sorgenti di Stockfish 19 non modificati (tutti tranne main.cpp: tt.cpp e
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
CORE    ?= dfpn
NODES   ?= 200000
MOVES   ?= 1000000
NNUE    ?= $(abspath $(SF))/nn-1a298aa575a0.nnue
SUITE   ?= ../../tests/suites/mates2000.epd
RESULTS ?= ../../tests/results
TIME    ?= 10
THREADS ?= 4
HASH    ?= 256
BENCH   := ./build/wintree-chess --suite $(SUITE) --time $(TIME) --threads $(THREADS) --nnue $(NNUE) --hash $(HASH)

vpath %.cpp $(SF) $(SF)/nnue $(SF)/nnue/features $(SF)/syzygy

.PHONY: all test g0full bench-g1 bench-g1-full bench-eps clean

all: build/wintree-chess

build/wintree-chess: build/main.o $(SFOBJS)
	$(CXX) $(SFFLAGS) -o $@ $^ $(LDLIBS)

build/main.o: main.cpp bench.h solve.h warm.h dfpn_baseline.h chess_game.h verifier.h ../wintree/wintree.h ../wintree/dfpn.h | build/sf
	$(CXX) $(SFFLAGS) -I$(SF) -c -o $@ $<

build/sf/%.o: %.cpp | build/sf
	$(CXX) $(SFFLAGS) -I$(SF) -c -o $@ $<

build/sf:
	mkdir -p build/sf

# regressioni + campione ridotto di G0 (pochi minuti); il campione completo da 10^4 posizioni: make g0full. Budget per
# ricerca: $(NODES) espansioni e $(MOVES) do_move (il primo che si esaurisce; df-pn fa ~35 do_move per espansione).
test: build/wintree-chess
	./build/wintree-chess --regress --core $(CORE) --tb $(TB)
	./build/wintree-chess --g0 $(G0) --core $(CORE) --seed $(SEED) --nodes $(NODES) --domove $(MOVES) --tb $(TB)

g0full: build/wintree-chess
	./build/wintree-chess --g0 10000 --core $(CORE) --seed 1 --nodes $(NODES) --domove $(MOVES) --tb $(TB)

$(SUITE):
	sh $(dir $(SUITE))fetch-mates2000.sh

# G1 rapido (§11): B6 su 100 posizioni (una ogni 20) con il nucleo df-pn (freddo, a caldo), WinTree a caldo, la
# baseline a caldo e Stockfish, tetto $(TIME) s per solver, $(THREADS) posizioni in parallelo. ~10 min.
bench-g1: build/wintree-chess $(SUITE)
	$(BENCH) --step 20 --solvers dfpn,dfpn-warm,warm,base-warm,sf --csv $(RESULTS)/g1-quick-m2c.csv

# G1 completo di M2c: B6 sull'intera suite con il nucleo df-pn (freddo/caldo, con/senza inizializzazione euristica),
# la baseline di M2b (freddo/caldo) e Stockfish; poi ε ∈ {0, 0.25, 0.5} su 200 posizioni (una ogni 10). In background,
# log in $(RESULTS)/bench-g1-m2c.log (~1-2 h). I risultati di M2b (WinTree) restano in g1-full.csv, b4-order.csv, b5-ablation.csv.
bench-g1-full: build/wintree-chess $(SUITE)
	nohup sh -c '$(BENCH) --solvers dfpn,dfpn-warm,dfpn-noinit,dfpn-warm-noinit,base,base-warm,sf --csv $(RESULTS)/g1-full-m2c.csv && \
	             $(BENCH) --step 10 --solvers dfpn-warm,dfpn-warm-eps0,dfpn-warm-eps50 --csv $(RESULTS)/g1-eps.csv' \
	      > $(RESULTS)/bench-g1-m2c.log 2>&1 &
	@echo "avviato in background: tail -f $(RESULTS)/bench-g1-m2c.log"

bench-eps: build/wintree-chess $(SUITE)
	$(BENCH) --step 10 --solvers dfpn-warm,dfpn-warm-eps0,dfpn-warm-eps50 --csv $(RESULTS)/g1-eps.csv

clean:
	rm -rf build
