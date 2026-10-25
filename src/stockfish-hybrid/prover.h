/*
  Stockfish, a UCI chess playing engine derived from Glaurung 2.1
  Copyright (C) 2004-2026 The Stockfish developers (see AUTHORS file)

  Stockfish is free software: you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 3 of the License, or
  (at your option) any later version.

  Stockfish is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

// WinTree × Stockfish, M3 (spec §6, §8.5): prover df-pn sincrono alla radice e probe della tabella delle prove
// nella ricerca. Stato globale come Tablebases (opzioni H1, tabella H7); il nucleo (../wintree/dfpn.h), l'adapter e
// il verificatore (../wintree-chess) sono inclusi da prover.cpp senza copie.

#ifndef PROVER_H_INCLUDED
#define PROVER_H_INCLUDED

#include <vector>

#include "types.h"

namespace Stockfish {

class Position;

namespace Prover {

enum Mode {
    OFF,   // nessun prover, nessun probe: ricerca identica a Stockfish 19
    AUTO,  // probe attivo; prover alla radice su trigger (go mate, punteggio precedente decisivo)
    MATE,  // probe attivo; prover alla radice sempre, in modalità WIN(us)
    DRAW   // probe attivo; prover alla radice sempre, in modalità WIN(opp) (cerca PROVEN_NOWIN(opp): patta certificata)
};

// Impostati dalle opzioni UCI (engine.cpp), letti dalla ricerca: setoption attende la fine della ricerca.
extern Mode mode;
extern int  probeDepth;      // ProverProbeDepth: probe solo con depth >= probeDepth (gating come SyzygyProbeDepth)
extern int  budgetPermille;  // ProverBudget: quota di tm.optimum() (o di movetime) per il prover alla radice
extern int  maxNodes;        // ProverMaxNodes: tetto in do_move del prover (il "nodo" di §9.4); 0 = prover spento
extern bool logRoot;         // ProverLog: `info string prover: start ...` / `no result after N do_move in T ms`

void set_mode(Mode m);
void set_hash(int mib);  // ProverHash in MiB (16 byte per entry)
void clear();            // H7: azzeramento della tabella (search_clear)

// Esito certificato alla radice (H4/H5): PROVEN_WIN(us) passata dal verificatore di §7.9.
struct RootProof {
    bool              proven = false;
    int               d      = 0;           // ply dell'albero verificato (limite superiore della DTM, I6)
    Value             value  = VALUE_NONE;  // mate_in(d), oppure VALUE_TB - d se la prova ha foglie TB (§7.8)
    Move              move   = Move::none();
    std::vector<Move> pv;                   // cammino principale dell'albero di prova
};

// H3: entry della posizione (una per goal) mappate su valore e bound con la tabella di §7.8. Restituisce quante.
struct Hit {
    Value value;
    Bound bound;
};
int probe(const Position& pos, int ply, Hit out[2]);

}  // namespace Prover
}  // namespace Stockfish

#endif  // #ifndef PROVER_H_INCLUDED
