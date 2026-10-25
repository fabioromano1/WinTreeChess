import argparse
import re
import sys
import subprocess
import pathlib
import os
import fnmatch

from testing import (
    EPD,
    Stockfish as Engine,
    MiniTestFramework,
    OrderedClassMembers,
    Valgrind,
    Syzygy,
)

PATH = pathlib.Path(__file__).parent.resolve()
CWD = os.getcwd()


def get_prefix(expect_failure=False):
    if args.valgrind:
        return Valgrind.get_valgrind_command(expect_failure)
    if args.valgrind_thread:
        return Valgrind.get_valgrind_thread_command()

    return []


def get_threads():
    if args.valgrind_thread or args.sanitizer_thread:
        return 4
    return 1


def get_path():
    return os.path.abspath(os.path.join(CWD, args.stockfish_path))


def postfix_check(output):
    if args.sanitizer_undefined:
        for idx, line in enumerate(output):
            if "runtime error:" in line:
                # print next possible 50 lines
                for i in range(50):
                    debug_idx = idx + i
                    if debug_idx < len(output):
                        print(output[debug_idx])
                return False

    if args.sanitizer_thread:
        for idx, line in enumerate(output):
            if "WARNING: ThreadSanitizer:" in line:
                # print next possible 50 lines
                for i in range(50):
                    debug_idx = idx + i
                    if debug_idx < len(output):
                        print(output[debug_idx])
                return False

    if args.valgrind or args.valgrind_thread:
        for line in output:
            match = re.search(r"ERROR SUMMARY:\s*(\d+) errors", line)
            if match and int(match.group(1)) > 0:
                return False

    return True


def Stockfish(*args, expect_failure=False, **kwargs):
    return Engine(get_prefix(expect_failure), get_path(), *args, expect_failure=expect_failure, **kwargs)


class TestCLI(metaclass=OrderedClassMembers):
    def beforeAll(self):
        pass

    def afterAll(self):
        pass

    def beforeEach(self):
        self.stockfish = None

    def afterEach(self):
        assert postfix_check(self.stockfish.get_output()) == True
        self.stockfish.clear_output()

    def test_eval(self):
        self.stockfish = Stockfish("eval".split(" "), True)
        assert self.stockfish.process.returncode == 0

    def test_go_nodes_1000(self):
        self.stockfish = Stockfish("go nodes 1000".split(" "), True)
        assert self.stockfish.process.returncode == 0

    def test_go_depth_10(self):
        self.stockfish = Stockfish("go depth 10".split(" "), True)
        assert self.stockfish.process.returncode == 0

    def test_go_perft_4(self):
        self.stockfish = Stockfish("go perft 4".split(" "), True)
        assert self.stockfish.process.returncode == 0

    def test_go_movetime_1000(self):
        self.stockfish = Stockfish("go movetime 1000".split(" "), True)
        assert self.stockfish.process.returncode == 0

    def test_go_wtime_8000_btime_8000_winc_500_binc_500(self):
        self.stockfish = Stockfish(
            "go wtime 8000 btime 8000 winc 500 binc 500".split(" "),
            True,
        )
        assert self.stockfish.process.returncode == 0

    def test_go_wtime_1000_btime_1000_winc_0_binc_0(self):
        self.stockfish = Stockfish(
            "go wtime 1000 btime 1000 winc 0 binc 0".split(" "),
            True,
        )
        assert self.stockfish.process.returncode == 0

    def test_go_wtime_1000_btime_1000_winc_0_binc_0_movestogo_5(self):
        self.stockfish = Stockfish(
            "go wtime 1000 btime 1000 winc 0 binc 0 movestogo 5".split(" "),
            True,
        )
        assert self.stockfish.process.returncode == 0

    def test_go_movetime_200(self):
        self.stockfish = Stockfish("go movetime 200".split(" "), True)
        assert self.stockfish.process.returncode == 0

    def test_go_nodes_20000_searchmoves_e2e4_d2d4(self):
        self.stockfish = Stockfish(
            "go nodes 20000 searchmoves e2e4 d2d4".split(" "), True
        )
        assert self.stockfish.process.returncode == 0

    def test_bench_128_threads_8_default_depth(self):
        self.stockfish = Stockfish(
            f"bench 128 {get_threads()} 8 default depth".split(" "),
            True,
        )
        assert self.stockfish.process.returncode == 0

    def test_bench_128_threads_3_bench_tmp_epd_depth(self):
        self.stockfish = Stockfish(
            f"bench 128 {get_threads()} 3 {os.path.join(PATH, 'bench_tmp.epd')} depth".split(
                " "
            ),
            True,
        )
        assert self.stockfish.process.returncode == 0

    def test_d(self):
        self.stockfish = Stockfish("d".split(" "), True)
        assert self.stockfish.process.returncode == 0

    def test_compiler(self):
        self.stockfish = Stockfish("compiler".split(" "), True)
        assert self.stockfish.process.returncode == 0

    def test_license(self):
        self.stockfish = Stockfish("license".split(" "), True)
        assert self.stockfish.process.returncode == 0

    def test_uci(self):
        self.stockfish = Stockfish("uci".split(" "), True)
        assert self.stockfish.process.returncode == 0

    def test_export_net_verify_nnue(self):
        current_path = os.path.abspath(os.getcwd())
        self.stockfish = Stockfish(
            f"export_net {os.path.join(current_path, 'verify.nnue')}".split(" "), True
        )
        assert self.stockfish.process.returncode == 0

    # verify the generated net equals the base net

    def test_network_equals_base(self):
        self.stockfish = Stockfish(
            ["uci"],
            True,
        )

        output = self.stockfish.process.stdout

        # find line
        for line in output.split("\n"):
            if "option name EvalFile type string default" in line:
                network = line.split(" ")[-1]
                break

        # find network file in src dir (layout di Stockfish: tests/../src; copia ibrida M3: tests/..)
        candidates = [os.path.join(PATH.parent.resolve(), "src", network), os.path.join(PATH.parent.resolve(), network)]
        network = next((c for c in candidates if os.path.exists(c)), candidates[0])

        if not os.path.exists(network):
            print(
                f"Network file {network} not found, please download the network file over the make command."
            )
            assert False

        diff = subprocess.run(["diff", network, f"verify.nnue"])

        assert diff.returncode == 0


class TestInteractive(metaclass=OrderedClassMembers):
    def beforeAll(self):
        self.stockfish = Stockfish()

    def afterAll(self):
        self.stockfish.quit()
        assert self.stockfish.close() == 0

    def afterEach(self):
        assert postfix_check(self.stockfish.get_output()) == True
        self.stockfish.clear_output()

    def test_startup_output(self):
        self.stockfish.starts_with("Stockfish")

    def test_uci_command(self):
        self.stockfish.send_command("uci")
        self.stockfish.equals("uciok")

    def test_set_threads_option(self):
        self.stockfish.send_command(f"setoption name Threads value {get_threads()}")

    def test_ucinewgame_and_startpos_nodes_1000(self):
        self.stockfish.send_command("ucinewgame")
        self.stockfish.send_command("position startpos")
        self.stockfish.send_command("go nodes 1000")
        self.stockfish.starts_with("bestmove")

    def test_ucinewgame_and_startpos_moves(self):
        self.stockfish.send_command("ucinewgame")
        self.stockfish.send_command("position startpos moves e2e4 e7e6")
        self.stockfish.send_command("go nodes 1000")
        self.stockfish.starts_with("bestmove")

    def test_fen_position_1(self):
        self.stockfish.send_command("ucinewgame")
        self.stockfish.send_command("position fen 5rk1/1K4p1/8/8/3B4/8/8/8 b - - 0 1")
        self.stockfish.send_command("go nodes 1000")
        self.stockfish.starts_with("bestmove")

    def test_fen_position_2_flip(self):
        self.stockfish.send_command("ucinewgame")
        self.stockfish.send_command("position fen 5rk1/1K4p1/8/8/3B4/8/8/8 b - - 0 1")
        self.stockfish.send_command("flip")
        self.stockfish.send_command("go nodes 1000")
        self.stockfish.starts_with("bestmove")

    def test_depth_5_with_callback(self):
        self.stockfish.send_command("ucinewgame")
        self.stockfish.send_command("position startpos")
        self.stockfish.send_command("go depth 5")

        def callback(output):
            regex = r"info depth \d+ seldepth \d+ multipv \d+ score cp -?\d+ nodes \d+ nps \d+ hashfull \d+ tbhits \d+ time \d+ pv"
            if output.startswith("info depth") and not re.match(regex, output):
                assert False
            if output.startswith("bestmove"):
                return True
            return False

        self.stockfish.check_output(callback)

    def test_ucinewgame_and_go_depth_9(self):
        self.stockfish.send_command("ucinewgame")
        self.stockfish.send_command("setoption name UCI_ShowWDL value true")
        self.stockfish.send_command("position startpos")
        self.stockfish.send_command("go depth 9")

        depth = 1

        def callback(output):
            nonlocal depth

            regex = rf"info depth {depth} seldepth \d+ multipv \d+ score cp -?\d+ wdl \d+ \d+ \d+ nodes \d+ nps \d+ hashfull \d+ tbhits \d+ time \d+ pv"

            if output.startswith("info depth"):
                if not re.match(regex, output):
                    assert False
                depth += 1

            if output.startswith("bestmove"):
                assert depth == 10
                return True

            return False

        self.stockfish.check_output(callback)

    def test_go_depth_3_with_mismatched_clock(self):
        self.stockfish.send_command("ucinewgame")
        self.stockfish.send_command("position startpos")
        self.stockfish.send_command("go depth 3 btime 1000")

        max_depth = 0

        def callback(output):
            nonlocal max_depth
            if output.startswith("info depth"):
                match = re.search(r"info depth (\d+)", output)
                if match:
                    max_depth = max(max_depth, int(match.group(1)))

            if output.startswith("bestmove"):
                assert max_depth == 3
                return True

            return False

        self.stockfish.check_output(callback)

    def test_clear_hash(self):
        self.stockfish.send_command("setoption name Clear Hash")

    def test_fen_position_mate_1(self):
        self.stockfish.send_command("ucinewgame")
        self.stockfish.send_command(
            "position fen 5K2/8/2qk4/2nPp3/3r4/6B1/B7/3R4 w - e6"
        )
        self.stockfish.send_command("go depth 18")

        self.stockfish.expect("* score mate 1 * pv d5e6")
        self.stockfish.equals("bestmove d5e6")

    def test_fen_position_mate_minus_1(self):
        self.stockfish.send_command("ucinewgame")
        self.stockfish.send_command(
            "position fen 2brrb2/8/p7/Q7/1p1kpPp1/1P1pN1K1/3P4/8 b - -"
        )
        self.stockfish.send_command("go depth 18")
        self.stockfish.expect("* score mate -1 *")
        self.stockfish.starts_with("bestmove")

    def test_fen_position_fixed_node(self):
        self.stockfish.send_command("ucinewgame")
        self.stockfish.send_command(
            "position fen 5K2/8/2P1P1Pk/6pP/3p2P1/1P6/3P4/8 w - - 0 1"
        )
        self.stockfish.send_command("go nodes 500000")
        self.stockfish.starts_with("bestmove")

    def test_fen_position_with_mate_go_depth(self):
        self.stockfish.send_command("ucinewgame")
        self.stockfish.send_command(
            "position fen 8/5R2/2K1P3/4k3/8/b1PPpp1B/5p2/8 w - -"
        )
        self.stockfish.send_command("go depth 18 searchmoves c6d7")
        self.stockfish.expect("* score mate 2 * pv c6d7 * f7f5")

        self.stockfish.starts_with("bestmove")

    def test_fen_position_with_mate_go_mate(self):
        self.stockfish.send_command("ucinewgame")
        self.stockfish.send_command(
            "position fen 8/5R2/2K1P3/4k3/8/b1PPpp1B/5p2/8 w - -"
        )
        self.stockfish.send_command("go mate 2 searchmoves c6d7")
        self.stockfish.expect("* score mate 2 * pv c6d7 *")

        self.stockfish.starts_with("bestmove")

    def test_fen_position_with_mate_go_nodes(self):
        self.stockfish.send_command("ucinewgame")
        self.stockfish.send_command(
            "position fen 8/5R2/2K1P3/4k3/8/b1PPpp1B/5p2/8 w - -"
        )
        self.stockfish.send_command("go nodes 500000 searchmoves c6d7")
        self.stockfish.expect("* score mate 2 * pv c6d7 * f7f5")

        self.stockfish.starts_with("bestmove")

    def test_fen_position_depth_27(self):
        self.stockfish.send_command("ucinewgame")
        self.stockfish.send_command(
            "position fen r1b2r1k/pp1p2pp/2p5/2B1q3/8/8/P1PN2PP/R4RK1 w - - 0 18"
        )
        self.stockfish.send_command("go")
        self.stockfish.contains("score mate 1")

        self.stockfish.starts_with("bestmove")

    def test_fen_position_with_mate_go_depth_and_promotion(self):
        self.stockfish.send_command("ucinewgame")
        self.stockfish.send_command(
            "position fen 8/5R2/2K1P3/4k3/8/b1PPpp1B/5p2/8 w - - moves c6d7 f2f1q"
        )
        self.stockfish.send_command("go depth 18")
        self.stockfish.expect("* score mate 1 * pv f7f5")
        self.stockfish.starts_with("bestmove f7f5")

    def test_fen_position_with_mate_go_depth_and_searchmoves(self):
        self.stockfish.send_command("ucinewgame")
        self.stockfish.send_command(
            "position fen 8/5R2/2K1P3/4k3/8/b1PPpp1B/5p2/8 w - -"
        )
        self.stockfish.send_command("go depth 18 searchmoves c6d7")
        self.stockfish.expect("* score mate 2 * pv c6d7 * f7f5")

        self.stockfish.starts_with("bestmove c6d7")

    def test_fen_position_with_moves_with_mate_go_depth_and_searchmoves(self):
        self.stockfish.send_command("ucinewgame")
        self.stockfish.send_command(
            "position fen 8/5R2/2K1P3/4k3/8/b1PPpp1B/5p2/8 w - - moves c6d7"
        )
        self.stockfish.send_command("go depth 18 searchmoves e3e2")
        self.stockfish.expect("* score mate -1 * pv e3e2 f7f5")
        self.stockfish.starts_with("bestmove e3e2")

    def test_verify_nnue_network(self):
        current_path = os.path.abspath(os.getcwd())
        Stockfish(
            f"export_net {os.path.join(current_path, 'verify.nnue')}".split(" "), True
        )

        self.stockfish.send_command("setoption name EvalFile value verify.nnue")
        self.stockfish.send_command("position startpos")
        self.stockfish.send_command("go depth 5")
        self.stockfish.starts_with("bestmove")

    def test_multipv_setting(self):
        self.stockfish.send_command("setoption name MultiPV value 4")
        self.stockfish.send_command("position startpos")
        self.stockfish.send_command("go depth 5")
        self.stockfish.starts_with("bestmove")

    def test_fen_position_with_skill_level(self):
        self.stockfish.send_command("setoption name Skill Level value 10")
        self.stockfish.send_command("position startpos")
        self.stockfish.send_command("go depth 5")
        self.stockfish.starts_with("bestmove")

        self.stockfish.send_command("setoption name Skill Level value 20")


class TestSyzygy(metaclass=OrderedClassMembers):
    def beforeAll(self):
        self.stockfish = Stockfish()

    def afterAll(self):
        self.stockfish.quit()
        assert self.stockfish.close() == 0

    def afterEach(self):
        assert postfix_check(self.stockfish.get_output()) == True
        self.stockfish.clear_output()

    def test_syzygy_setup(self):
        self.stockfish.starts_with("Stockfish")
        self.stockfish.send_command("uci")
        self.stockfish.send_command(
            f"setoption name SyzygyPath value {os.path.join(PATH, 'syzygy')}"
        )
        self.stockfish.expect(
            "info string Found 35 WDL and 35 DTZ tablebase files (up to 4-man)."
        )

    def test_syzygy_bench(self):
        self.stockfish.send_command("bench 128 1 8 default depth")
        self.stockfish.expect("Nodes searched  :*")

    def test_syzygy_position(self):
        self.stockfish.send_command("ucinewgame")
        self.stockfish.send_command("position fen 4k3/PP6/8/8/8/8/8/4K3 w - - 0 1")
        self.stockfish.send_command("go depth 5")

        def check_output(output):
            if "score cp 20000" in output or "score mate" in output:
                return True

        self.stockfish.check_output(check_output)
        self.stockfish.expect("bestmove *")

    def test_syzygy_position_2(self):
        self.stockfish.send_command("ucinewgame")
        self.stockfish.send_command("position fen 8/1P6/2B5/8/4K3/8/6k1/8 w - - 0 1")
        self.stockfish.send_command("go depth 5")

        def check_output(output):
            if "score cp 20000" in output or "score mate" in output:
                return True

        self.stockfish.check_output(check_output)
        self.stockfish.expect("bestmove *")

    def test_syzygy_position_3(self):
        self.stockfish.send_command("ucinewgame")
        self.stockfish.send_command("position fen 8/1P6/2B5/8/4K3/8/6k1/8 b - - 0 1")
        self.stockfish.send_command("go depth 5")

        def check_output(output):
            if "score cp -20000" in output or "score mate -" in output:
                return True

        self.stockfish.check_output(check_output)
        self.stockfish.expect("bestmove *")

    def test_syzygy_position_4(self):
        self.stockfish.send_command("ucinewgame")
        self.stockfish.send_command("position fen 8/8/7B/3B3P/7k/8/5K2/3r4 w - - 0 1")
        self.stockfish.send_command("go depth 1")
        self.stockfish.expect("bestmove *")

class TestEnPassantSanitization(metaclass=OrderedClassMembers):
    def beforeAll(self):
        self.stockfish = Stockfish()

    def afterAll(self):
        self.stockfish.quit()
        assert self.stockfish.close() == 0

    def afterEach(self):
        assert postfix_check(self.stockfish.get_output()) == True
        self.stockfish.clear_output()

    def test_position_1(self):
        self.stockfish.send_command("position fen rnbqkbnr/ppp1p1pp/5p2/3pP3/8/8/PPPP1PPP/RNBQKBNR w kq d6 0 3")
        self.stockfish.send_command("d")

        self.stockfish.expect_for_line_matching("Fen*", "*rnbqkbnr/ppp1p1pp/5p2/3pP3/8/8/PPPP1PPP/RNBQKBNR w kq d6 0 3*")

    def test_position_2(self):
        self.stockfish.send_command("position fen k7/8/8/1pP5/2K5/8/8/8 w - b6 0 1")
        self.stockfish.send_command("d")

        self.stockfish.expect_for_line_matching("Fen*", "*k7/8/8/1pP5/2K5/8/8/8 w - b6 0 1*")

    def test_position_3(self):
        self.stockfish.send_command("position fen k1r5/8/8/1pP5/2K5/8/8/8 w - b6 0 1")
        self.stockfish.send_command("d")

        self.stockfish.expect_for_line_matching("Fen*", "*k1r5/8/8/1pP5/2K5/8/8/8 w - - 0 1*")

    def test_position_4(self):
        self.stockfish.send_command("position fen k1r5/8/8/1pP5/8/2K5/8/8 w - b6 0 1")
        self.stockfish.send_command("d")

        self.stockfish.expect_for_line_matching("Fen*", "*k1r5/8/8/1pP5/8/2K5/8/8 w - - 0 1*")

    def test_position_5(self):
        self.stockfish.send_command("position fen k1r5/8/8/PpP5/8/2K5/8/8 w - b6 0 1")
        self.stockfish.send_command("d")

        self.stockfish.expect_for_line_matching("Fen*", "*k1r5/8/8/PpP5/8/2K5/8/8 w - b6 0 1*")

    def test_position_6(self):
        self.stockfish.send_command("position fen k1r5/8/8/PpP5/2K5/8/8/8 w - b6 0 1")
        self.stockfish.send_command("d")

        self.stockfish.expect_for_line_matching("Fen*", "*k1r5/8/8/PpP5/2K5/8/8/8 w - b6 0 1*")

    def test_position_7(self):
        self.stockfish.send_command("position fen k7/4b3/8/PpP5/1K6/8/8/8 w - b6 0 1")
        self.stockfish.send_command("d")

        self.stockfish.expect_for_line_matching("Fen*", "*k7/4b3/8/PpP5/1K6/8/8/8 w - b6 0 1*")

    def test_position_8(self):
        self.stockfish.send_command("position fen k7/b5b1/8/2PpP3/3K4/8/8/8 w - d6 0 1")
        self.stockfish.send_command("d")

        self.stockfish.expect_for_line_matching("Fen*", "*k7/b5b1/8/2PpP3/3K4/8/8/8 w - - 0 1*")

    def test_position_9(self):
        self.stockfish.send_command("position fen k7/8/8/r2pPK2/8/8/8/8 w - d6 0 1")
        self.stockfish.send_command("d")

        self.stockfish.expect_for_line_matching("Fen*", "*k7/8/8/r2pPK2/8/8/8/8 w - - 0 1*")

    def test_position_10(self):
        self.stockfish.send_command("position fen k7/8/8/r1PpPK2/8/8/8/8 w - d6 0 1")
        self.stockfish.send_command("d")

        self.stockfish.expect_for_line_matching("Fen*", "*k7/8/8/r1PpPK2/8/8/8/8 w - d6 0 1*")

    def test_position_11(self):
        self.stockfish.send_command("position fen kb6/8/8/3pP3/5K2/8/8/8 w - d6 0 1")
        self.stockfish.send_command("d")

        self.stockfish.expect_for_line_matching("Fen*", "*kb6/8/8/3pP3/5K2/8/8/8 w - d6 0 1*")

    def test_position_find_draw(self):
        self.stockfish.send_command("position fen q4kb1/3Q2nq/8/r3PpK1/2n5/7q/8/q7 w - f6 0 1 moves d7c8 f8f7 c8d7 f7f8 d7d8 f8f7")
        self.stockfish.send_command("go nodes 10000")

        def check_output(output):
            if fnmatch.fnmatch(output, "* score cp 0 * pv d8d7*"):
                return True

        self.stockfish.check_output(check_output)
        self.stockfish.expect("bestmove d8d7*")

class TestProver(metaclass=OrderedClassMembers):
    # M3 (spec §9.1, §12.1 punto 6): prover WinTree/df-pn alla radice. Posizioni: le 4 di matto/stallo del bench di
    # Stockfish (benchmark.cpp) e un campione di mates2000 (vondele/matetrack, ../../tests/suites/mates2000.epd:
    # id, FEN, N = "il tratto matta in N"). Il prover trova un matto, non il piu' corto: la N provata e' >= quella
    # dichiarata (limite superiore della DTM, I6); mossa dell'info string, bestmove e punteggio devono coincidere.
    MATES2000 = [
        (1, "7n/BBP2P1P/8/P1PpK3/P5RR/5k2/Pn2NPN1/3Q2b1 w - d6 0 1", 1),
        (7, "3R4/8/8/2p3K1/2p5/5B2/R1pPNB2/1b1k4 w - - 0 1", 2),
        (19, "1N3B2/5p2/2R2p2/1p1kpp2/1P2rp2/2P1pB2/2P1P1K1/8 w - - 0 1", 3),
        (51, "1B5b/1p1N4/1Pp5/2P3p1/K1pk3p/2N5/2nP1p2/1b3B2 w - - 0 1", 4),
        (120, "1R5B/2p1K3/8/5p1p/B7/P1N2p2/1P3P2/r1k2N1r w - - 0 1", 5),
        (249, "1bB5/1P2p2r/8/2RRp3/1p2k1pB/3pP1K1/n2PpP2/2N1rb2 w - - 0 1", 6),
        (660, "1K4n1/1N6/k2pp3/8/3N1p2/1R5b/pp6/2b5 w - - 0 1", 7),
        (1144, "1B6/8/8/7p/8/8/P4K2/7k w - - 0 1", 8),
        (1391, "1N6/8/1B6/Pk6/8/1K3p2/3P4/8 w - - 0 1", 9),
        (1579, "1K2b3/3k1p2/3B1p2/3R3P/3P1P2/2P2p2/p7/7r w - - 0 1", 10),
        (1691, "1b5k/6pP/R5P1/p7/p5b1/p7/P7/4K3 w - - 0 1", 11),
        (1773, "1b3r1k/5pp1/6p1/5p1B/4p3/2pp3R/2r5/KQ6 w - - 0 1", 12),
        (1920, "1R2B1b1/5p2/1b2p3/2pp4/8/8/1p1K4/k6n w - - 0 1", 15),
        (1983, "1r6/5q1r/1n4p1/2N2p2/p3p3/p1KpR1R1/b1B5/k7 w - - 0 1", 20),
    ]

    def beforeAll(self):
        self.stockfish = Stockfish()
        self.stockfish.send_command("uci")
        self.stockfish.equals("uciok")
        self.stockfish.setoption("Prover", "mate")

    def afterAll(self):
        self.stockfish.quit()
        assert self.stockfish.close() == 0

    def afterEach(self):
        assert postfix_check(self.stockfish.get_output()) == True
        self.stockfish.clear_output()

    def _go(self, fen, go="go depth 5"):
        """Esegue `go` sulla posizione; restituisce (N provata, mossa provata) o None, l'ultimo `score mate` e la bestmove."""
        self.stockfish.send_command("ucinewgame")
        self.stockfish.send_command(f"position fen {fen}")
        self.stockfish.send_command(go)
        proven, mate, best = None, None, None
        for line in self.stockfish.readline():
            m = re.match(r"info string prover: proven mate in (\d+) move (\S+)", line)
            if m:
                proven = (int(m.group(1)), m.group(2))
            m = re.search(r" score mate (-?\d+) ", line)
            if m:
                mate = int(m.group(1))
            if line.startswith("bestmove"):
                best = line.split()[1]
                break
        return proven, mate, best

    def _check_mate(self, fen, n, go="go depth 5"):
        proven, mate, best = self._go(fen, go)
        assert proven is not None, f"nessun 'info string prover: proven mate' per {fen}"
        assert proven[1] == best, f"bestmove {best} diversa dalla mossa provata {proven[1]}"
        assert mate == proven[0], f"score mate {mate} diverso dalla N provata {proven[0]}"
        assert proven[0] >= n, f"N provata {proven[0]} minore del matto dichiarato in {n}"

    def test_bench_mate_in_5_black(self):
        self._check_mate("6k1/3b3r/1p1p4/p1n2p2/1PPNpP1q/P3Q1p1/1R1RB1P1/5K2 b - - 0 1", 5)

    def test_bench_mate_in_4_white(self):
        proven, mate, best = self._go("r2r1n2/pp2bk2/2p1p2p/3q4/3PN1QP/2P3R1/P4PP1/5RK1 w - - 0 1")
        assert proven == (4, "g4g7") and best == "g4g7" and mate == 4

    def test_bench_stalemate_no_prover(self):
        proven, mate, best = self._go("8/8/8/8/8/6k1/6p1/6K1 w - - 0 1")
        assert proven is None and best == "(none)"

    def test_bench_mated_no_prover(self):
        proven, mate, best = self._go("7k/7P/6K1/8/3B4/8/8/8 b - - 0 1")
        assert proven is None and best == "(none)"

    def test_mates2000_sample(self):
        for _, fen, n in self.MATES2000:
            self._check_mate(fen, n)

    def test_go_mate_stops_on_proof(self):
        proven, mate, best = self._go("r2r1n2/pp2bk2/2p1p2p/3q4/3PN1QP/2P3R1/P4PP1/5RK1 w - - 0 1", "go mate 4")
        assert proven == (4, "g4g7") and best == "g4g7" and mate == 4

    def test_prover_off_no_output(self):
        self.stockfish.setoption("Prover", "off")
        proven, mate, best = self._go("r2r1n2/pp2bk2/2p1p2p/3q4/3PN1QP/2P3R1/P4PP1/5RK1 w - - 0 1")
        assert proven is None and best is not None
        self.stockfish.setoption("Prover", "mate")

    def test_prover_auto_startpos_no_trigger(self):
        self.stockfish.setoption("Prover", "auto")
        self.stockfish.send_command("ucinewgame")
        self.stockfish.send_command("position startpos")
        self.stockfish.send_command("go nodes 2000")
        for line in self.stockfish.readline():
            assert not line.startswith("info string prover")
            if line.startswith("bestmove"):
                break

    def test_prover_auto_go_mate_triggers(self):
        proven, mate, best = self._go("r2r1n2/pp2bk2/2p1p2p/3q4/3PN1QP/2P3R1/P4PP1/5RK1 w - - 0 1", "go mate 4")
        assert proven == (4, "g4g7") and best == "g4g7"

    def test_prover_draw_dead_position(self):
        self.stockfish.setoption("Prover", "draw")
        self.stockfish.send_command("ucinewgame")
        self.stockfish.send_command("position fen 8/8/8/8/8/4k3/8/4KB2 w - - 0 1")
        self.stockfish.send_command("go depth 5")
        self.stockfish.starts_with("info string prover: proven draw-or-better")
        self.stockfish.starts_with("bestmove")
        self.stockfish.setoption("Prover", "mate")

    def test_prover_invalid_value(self):
        self.stockfish.send_command("setoption name Prover value zzz")
        self.stockfish.expect("*Prover: invalid value 'zzz'*")
        self.stockfish.setoption("Prover", "mate")

    def _go_clock(self, go="go wtime 1000 btime 1000 movestogo 1"):
        """`go` con l'orologio sulla posizione corrente: (ms di parete fino a bestmove, righe del prover, bestmove)."""
        import time
        self.stockfish.send_command(go)
        t0, prover, best = time.perf_counter(), [], None
        for line in self.stockfish.readline():
            if line.startswith("info string prover"):
                prover.append(line)
            if line.startswith("bestmove"):
                best = line.split()[1]
                break
        return (time.perf_counter() - t0) * 1000, prover, best

    def test_prover_auto_clock_within_limit(self):
        # G3 tuning: con Prover=auto il prover parte solo con punteggio precedente decisivo (seconda `go`), la sua
        # quota e' sommata all'orologio e il tempo totale resta sotto il limite (wtime 1000).
        self.stockfish.setoption("Prover", "auto")
        self.stockfish.setoption("ProverLog", "true")
        self.stockfish.send_command("ucinewgame")
        self.stockfish.send_command("position fen r2r1n2/pp2bk2/2p1p2p/3q4/3PN1QP/2P3R1/P4PP1/5RK1 w - - 0 1")
        ms, prover, best = self._go_clock()
        assert ms < 1000 and prover == [] and best == "g4g7", (ms, prover, best)
        ms, prover, best = self._go_clock()
        assert ms < 1000 and best == "g4g7", (ms, best)
        assert prover[0].startswith("info string prover: start win budget") and "proven mate in 4 move g4g7" in prover[1], prover
        self.stockfish.setoption("ProverLog", "false")
        self.stockfish.setoption("Prover", "mate")

    def test_prover_auto_failed_root_not_retried(self):
        # G3 tuning: dopo una ricerca senza esito (tetto 1 do_move) la stessa radice non riparte finche' il budget
        # non e' almeno il doppio di quello speso; ucinewgame azzera la memoria.
        self.stockfish.setoption("Prover", "auto")
        self.stockfish.setoption("ProverLog", "true")
        self.stockfish.setoption("ProverMaxNodes", "1")
        fen = "position fen r2r1n2/pp2bk2/2p1p2p/3q4/3PN1QP/2P3R1/P4PP1/5RK1 w - - 0 1"
        self.stockfish.send_command("ucinewgame")
        self.stockfish.send_command(fen)
        assert self._go_clock()[1] == []                          # prima mossa: nessun punteggio precedente
        prover = self._go_clock()[1]
        assert len(prover) == 2 and re.search(r"no result after \d+ do_move in \d+ ms", prover[1]), prover
        assert self._go_clock()[1] == [], "radice fallita ripartita con lo stesso budget"
        self.stockfish.setoption("ProverMaxNodes", "1000")       # >= 2x le do_move spese: riparte e prova
        prover = self._go_clock()[1]
        assert len(prover) == 2 and "proven mate in 4" in prover[1], prover
        self.stockfish.setoption("ProverMaxNodes", "1000000")
        self.stockfish.setoption("ProverLog", "false")
        self.stockfish.setoption("Prover", "mate")


class TestInvalidFEN(metaclass=OrderedClassMembers):
    def beforeEach(self):
        self.stockfish = None

    def afterEach(self):
        assert postfix_check(self.stockfish.get_output()) == True
        self.stockfish.clear_output()

    def _expect_critical(self, fen):
        self.stockfish = Stockfish(f"position fen {fen}".split(" "), True, expect_failure=True)
        assert self.stockfish.process.returncode != 0
        assert "CRITICAL ERROR" in self.stockfish.process.stdout

    def test_no_kings(self):
        self._expect_critical("8/8/8/8/8/8/8/8 w - - 0 1")

    def test_invalid_piece(self):
        self._expect_critical("rnbqkbnr/pppXpppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1")

    def test_invalid_side_to_move(self):
        self._expect_critical("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR x KQkq - 0 1")

    def test_pawns_on_back_rank(self):
        self._expect_critical("pppppppp/8/8/8/8/8/8/4K2k w - - 0 1")

    def test_invalid_skip_count(self):
        self._expect_critical("9/8/8/8/8/8/8/8 w - - 0 1")


class TestInvalidOptions(metaclass=OrderedClassMembers):
    def beforeAll(self):
        self.stockfish = Stockfish()

    def afterAll(self):
        self.stockfish.quit()
        assert self.stockfish.close() == 0

    def afterEach(self):
        assert postfix_check(self.stockfish.get_output()) == True
        self.stockfish.clear_output()

    # Ignore bogus spin values
    def test_spin_non_numeric(self):
        self.stockfish.send_command("setoption name Threads value abc")
        self.stockfish.send_command("isready")
        self.stockfish.equals("readyok")

    def test_spin_out_of_range(self):
        self.stockfish.send_command("setoption name Threads value 999999999999")
        self.stockfish.send_command("isready")
        self.stockfish.equals("readyok")

    def test_spin_negative(self):
        self.stockfish.send_command("setoption name Threads value -5")
        self.stockfish.send_command("isready")
        self.stockfish.equals("readyok")

    # Warn on bogus NUMA configs
    def test_numa_garbage(self):
        self.stockfish.send_command("setoption name NumaPolicy value zzz")
        self.stockfish.expect("*NumaPolicy: invalid value 'zzz', keeping previous config.*")

    def test_numa_malformed_range(self):
        self.stockfish.send_command("setoption name NumaPolicy value 0-")
        self.stockfish.expect("*NumaPolicy: invalid value '0-', keeping previous config.*")

    def test_numa_overflow(self):
        self.stockfish.send_command(
            "setoption name NumaPolicy value 99999999999999999999999"
        )
        self.stockfish.expect("*NumaPolicy: invalid value*keeping previous config.*")
        self.stockfish.send_command("isready")
        self.stockfish.equals("readyok")


class TestBenchFile(metaclass=OrderedClassMembers):
    def beforeEach(self):
        self.stockfish = None

    def afterEach(self):
        assert postfix_check(self.stockfish.get_output()) == True
        self.stockfish.clear_output()

    def _bench(self, name, content, expect_failure=False):
        with open(name, "w") as f:
            f.write(content)
        self.stockfish = Stockfish(f"bench 16 1 4 {name} depth".split(" "), True, expect_failure=expect_failure)

    def test_valid_file(self):
        self._bench("good.epd", "4k3/8/4K3/8/8/8/8/8 w - - 0 1\n")
        assert self.stockfish.process.returncode == 0
        assert "Nodes searched" in self.stockfish.process.stderr

    def test_empty_file(self):
        self._bench("empty.epd", "")
        assert self.stockfish.process.returncode == 0

    def test_malformed_fen(self):
        self._bench("bad.epd", "not a valid fen\n", expect_failure=True)
        assert self.stockfish.process.returncode != 0
        assert "CRITICAL ERROR" in self.stockfish.process.stdout

    def test_missing_file(self):
        self.stockfish = Stockfish(
            "bench 16 1 4 does_not_exist.epd depth".split(" "), True, expect_failure=True
        )
        assert self.stockfish.process.returncode != 0


def parse_args():
    parser = argparse.ArgumentParser(description="Run Stockfish with testing options")
    parser.add_argument("--valgrind", action="store_true", help="Run valgrind testing")
    parser.add_argument(
        "--valgrind-thread", action="store_true", help="Run valgrind-thread testing"
    )
    parser.add_argument(
        "--sanitizer-undefined",
        action="store_true",
        help="Run sanitizer-undefined testing",
    )
    parser.add_argument(
        "--sanitizer-thread", action="store_true", help="Run sanitizer-thread testing"
    )

    parser.add_argument(
        "--none", action="store_true", help="Run without any testing options"
    )
    parser.add_argument("stockfish_path", type=str, help="Path to Stockfish binary")

    return parser.parse_args()


if __name__ == "__main__":
    args = parse_args()

    EPD.create_bench_epd()
    Syzygy.download_syzygy()

    framework = MiniTestFramework()

    # Each test suite will be run inside a temporary directory
    framework.run(
        [
            TestCLI,
            TestInteractive,
            TestSyzygy,
            TestProver,
            TestEnPassantSanitization,
            TestInvalidFEN,
            TestInvalidOptions,
            TestBenchFile,
        ]
    )

    EPD.delete_bench_epd()

    if framework.has_failed():
        sys.exit(1)

    sys.exit(0)
