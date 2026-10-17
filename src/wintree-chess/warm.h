// Ricerca breve di Stockfish in-process (§8.4, ordinamento a caldo) e `go mate N` (B6) con un'istanza di Engine
// (engine.h) senza output su stdout. `tt` e `threads` sono membri privati di Engine: si leggono con l'istanziazione
// esplicita di un template su puntatori a membro ([temp.explicit]/12: niente controllo degli accessi), senza
// toccare i sorgenti di Stockfish. Il worker principale è letto solo a ricerca conclusa (wait_for_search_finished).
#ifndef WINTREE_CHESS_WARM_H
#define WINTREE_CHESS_WARM_H

#include "chess_game.h"

#include "engine.h"
#include "misc.h"
#include "score.h"

#include <iostream>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

namespace WinTree {

template<class Tag, typename Tag::type M> struct Rob { friend typename Tag::type get(Tag) { return M; } };
struct EngineTT { using type = Stockfish::TranspositionTable Stockfish::Engine::*; friend type get(EngineTT); };
struct EngineThreads { using type = Stockfish::ThreadPool Stockfish::Engine::*; friend type get(EngineThreads); };
template struct Rob<EngineTT, &Stockfish::Engine::tt>;
template struct Rob<EngineThreads, &Stockfish::Engine::threads>;

class SfEngine {
    Stockfish::Engine e;
    Stockfish::Score last{}; Stockfish::TimePoint lastMs = 0; u64 lastNodes = 0;
    Stockfish::TranspositionTable& tt() { return e.*get(EngineTT{}); }
    Stockfish::ThreadPool& threads() { return e.*get(EngineThreads{}); }

  public:
    // `nnue`: percorso della rete (Engine::verify_network termina il processo se non è caricata: l'errore va su stderr).
    explicit SfEngine(const std::string& nnue, int hashMiB = 16) {
        e.set_on_iter([](const auto&) {}); e.set_on_update_no_moves([](const auto&) {}); e.set_on_start([] {});
        e.set_on_bestmove([](const auto&, const auto&) {});
        e.set_on_verify_network([](std::string_view s) { if (s.substr(0, 5) == "ERROR") std::cerr << s << "\n"; });
        e.set_on_update_full([this](const Stockfish::Engine::InfoFull& i) { last = i.score; lastMs = i.timeMs; lastNodes = i.nodes; });
        set("EvalFile", nnue); set("Hash", std::to_string(hashMiB));
    }
    void set(const std::string& name, const std::string& value) { std::istringstream is("name " + name + " value " + value); e.get_options().setoption(is); }
    WarmSources sources() { return {&tt(), threads().main_thread()->worker.get()}; }
    void clear() { tt().clear(threads()); threads().clear(); }     // Engine::search_clear senza Tablebases::init (che azzererebbe le TB del prover)
    // `go movetime ms [mate n]` bloccante. Restituisce i ply del matto dell'ultimo punteggio (> 0 il tratto matta,
    // < 0 è mattato, 0 nessun matto); ms() e nodes() dell'ultima info ricevuta.
    int go(const std::string& fen, const std::vector<std::string>& moves, int movetimeMs, int mate = 0) {
        if (auto err = e.set_position(fen, moves)) { std::cerr << "SfEngine: posizione non valida: " << err->what() << "\n"; return 0; }
        Stockfish::Search::LimitsType l; l.movetime = movetimeMs; l.mate = mate; l.startTime = Stockfish::now();
        last = {}; lastMs = 0; lastNodes = 0;
        e.go(l); e.wait_for_search_finished();
        return last.is<Stockfish::Score::Mate>() ? last.get<Stockfish::Score::Mate>().plies : 0;
    }
    // Punteggio dell'ultima info in centipawn per il tratto (matto = ±10000; nessuna info = 0).
    int cp() const { return last.is<Stockfish::Score::InternalUnits>() ? last.get<Stockfish::Score::InternalUnits>().value : last.is<Stockfish::Score::Mate>() ? (last.get<Stockfish::Score::Mate>().plies > 0 ? 10000 : -10000) : 0; }
    Stockfish::TimePoint ms() const { return lastMs; }
    u64 nodes() const { return lastNodes; }
};

}  // namespace WinTree

#endif
