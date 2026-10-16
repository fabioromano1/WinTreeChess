# M2 (prima metà): wintree-chess = nucleo WinTree (../wintree) + adapter scacchi, verificatore, CLI, regressioni e G0,
# linkato con i sorgenti di Stockfish 19 non modificati (tutti tranne main.cpp: tt.cpp e position.cpp trascinano
# ThreadPool e UCIEngine, quindi quasi tutto il motore). Opzioni di compilazione: le stesse che il Makefile di
# Stockfish sceglie per ARCH=native (lette da `make config-sanity`), senza LTO (link in secondi) e senza la rete
# NNUE incorporata (-DNNUE_EMBEDDING_OFF: 98 MB che il prover non usa; la rete non viene mai caricata).
SF      := ../../Stockfish-sf_19/src
SFFLAGS := $(filter-out -flto%,$(shell $(MAKE) -s --no-print-directory -C $(SF) config-sanity ARCH=native 2>/dev/null | sed -n 's/^CXXFLAGS: *//p')) -DNNUE_EMBEDDING_OFF
LDLIBS  := -lpthread -lrt
SFSRCS  := $(filter-out $(SF)/main.cpp,$(wildcard $(SF)/*.cpp $(SF)/nnue/*.cpp $(SF)/nnue/features/*.cpp $(SF)/syzygy/*.cpp))
SFOBJS  := $(patsubst %.cpp,build/sf/%.o,$(notdir $(SFSRCS)))
TB      ?= $(HOME)/syzygy/5man
G0      ?= 1000
SEED    ?= 1
NODES   ?= 200000

vpath %.cpp $(SF) $(SF)/nnue $(SF)/nnue/features $(SF)/syzygy

.PHONY: all test g0full clean

all: build/wintree-chess

build/wintree-chess: build/main.o $(SFOBJS)
	$(CXX) $(SFFLAGS) -o $@ $^ $(LDLIBS)

build/main.o: main.cpp chess_game.h verifier.h ../wintree/wintree.h | build/sf
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

clean:
	rm -rf build
