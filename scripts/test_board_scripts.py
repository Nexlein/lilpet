# Tests of the scripts that talk to a board, with no hardware: the fake pyserial below plays a board on a fake clock, so
# no real port is opened and no test waits. Its hello comes from firmware/main/commands.c: a field renamed there fails
# here. Standard library only.
# Usage: npm run check:scripts
import contextlib
import io
import json
import math
import os
import re
import runpy
import sys
import termios
import types
import unittest
from pathlib import Path
from unittest import mock

from firmware_facts import REPLY_BENCH, REPLY_NO_MOTION, REPLY_TOO_LONG, REPLY_UNKNOWN, c_define, pupil_types

SCRIPTS = Path(__file__).resolve().parent
ROOT = SCRIPTS.parent
PORT = "/dev/ttyACM0"

COMMANDS = ROOT / "firmware" / "main" / "commands.c"
COMMANDS_C = COMMANDS.read_text()
HELLO = re.search(r'"< (lilpet [^"]*)\\n"', COMMANDS_C).group(1)
LINE_MAX = c_define(ROOT / "engine" / "include" / "lilpet" / "lilpet.h", "LP_COMMAND_LINE_MAX")
SAVE_GAP = c_define(ROOT / "firmware" / "main" / "look_saving.h", "LOOK_SAVE_GAP_US") / 1e6  # seconds
# Seconds: the firmware may notice that a late look's turn came this much later (read_line in commands.c)
READ_WAIT = c_define(COMMANDS, "READ_WAIT_MS") / 1000
REPORT = "62.5 frames/s | draw mean 2800 max 6100 us | redrawn 96 per mille of the screen"
LOOK = rf"[0-9A-F]{{6}} [0-9A-F]{{6}} [0-9A-F]{{6}} ({'|'.join(pupil_types())})"
DRIVES = {  # what the eyes take, in the fake's few words: enough for what the check sends
    "emotion": r"(neutral|happy|love)( [1-9.]+)?",
    "react": r"tap|gesture_like",
    "face": r"off|-?[01](\.\d+)? -?[01](\.\d+)?",
}


class Clock:
    now = 0.0

    def time(self):
        return self.now

    def sleep(self, seconds):
        self.now += seconds


clock = Clock()
plugged = {}  # port name: Board


class Board:
    """What the firmware does on its USB port, as far as the scripts can tell"""

    def __init__(
        self,
        name="atoms3r",
        bench=False,
        motion=True,
        reads_after=0.0,  # still booting: what it is sent until then is lost
        stale=(),  # lines on their way to the port when it is opened
        boot_log=(),  # what it prints when it starts, past the boot line
        report=REPORT,  # the timing report it prints after a round of the animation, if any
        report_after=12.5,  # seconds after booting
        open_error=None,
        dtr_error=None,
        monitor_open=False,  # a program holds the port with the exclusive lock of pyserial
        stop_at=None,  # Ctrl+C when the board gets this line, or at "rts" when RTS is asserted
        cut=None,  # bytes: each line reaches readline in two parts, cut there (its timeout ended in the middle)
        drops_late_looks=False,  # a look that waits for its turn answers ok but is never written
        comes_back=True,  # starts again after a reset
        lost_at=None,  # the port goes away when the board gets this line (unplugged)
    ):
        self.name, self.bench, self.motion = name, bench, motion
        self.boot_log, self.report, self.report_after = boot_log, report, report_after
        self.reads_from = clock.now + reads_after
        self.output = [(clock.now, line) for line in stale]  # when each line reaches the port, and the line
        self.open_error, self.dtr_error, self.monitor_open, self.stop_at = open_error, dtr_error, monitor_open, stop_at
        self.cut, self.drops_late_looks, self.comes_back, self.lost_at = cut, drops_late_looks, comes_back, lost_at
        self.lost = False
        self.look = "AE0002 FF5252 FF5252 dot"
        # What the flash holds, and when it was last written (firmware/main/look_saving.h)
        self.kept, self.tried_at = self.look, -SAVE_GAP
        self.mood = "neutral"
        self.in_reset = False
        self.input = b""
        self.opened = 0

    def boot(self):
        self.look, self.mood = self.kept, "neutral"
        self.tried_at = clock.now - SAVE_GAP  # as commands_start: the first new look is written at once
        self.reads_from = clock.now + 1
        for line in ["I (250) main_task: Calling app_main()", *self.boot_log, "lilpet 0.13.0, board ..."]:
            self.output.append((clock.now + 0.3, line))
        if self.report:
            self.output.append((clock.now + 0.3 + self.report_after, self.report))

    def write_late_look(self):
        """What the firmware does while it runs, by its one rule: the shown look is written once it is not the kept one
        and the gap has passed since the last write, at the latest when its read gives up waiting for a byte"""
        turn = self.tried_at + SAVE_GAP + READ_WAIT
        if self.look != self.kept and clock.now >= turn and not self.drops_late_looks:
            self.kept, self.tried_at = self.look, turn

    def set_reset(self, held):
        if held and not self.in_reset:
            self.write_late_look()  # what is not written by then is lost: the board boots on the kept look
        if self.in_reset and not held:
            if self.comes_back:
                self.boot()
            else:
                self.reads_from = math.inf
        self.in_reset = held

    def receive(self, data):
        if self.in_reset or clock.now < self.reads_from:
            return
        *lines, self.input = (self.input + data).split(b"\n")
        for line in lines:
            reply = self.reply(line.decode())
            if reply:
                self.output.append((clock.now, f"< {reply}"))
            self.stop_if(line.decode())
            self.lost = self.lost or line.decode() == self.lost_at

    def stop_if(self, event):
        if event == self.stop_at:
            self.stop_at = None
            raise KeyboardInterrupt

    def reply(self, text):
        if not text.startswith("> "):
            return None
        if len(text) > LINE_MAX:
            return REPLY_TOO_LONG
        if text == "> hello":
            return HELLO % ("0.5.0", "0.13.0", self.name, self.motion)
        if text == "> look?":
            return f"look {self.look}"
        if text == "> imu?":
            return "imu 0.000 1.000 0.000" if self.motion else REPLY_NO_MOTION
        word, _, rest = text[2:].partition(" ")
        if word == "look":
            if not re.fullmatch(LOOK, rest):
                return "err malformed look"
            self.write_late_look()  # the one shown before, if its turn came while it waited
            self.look = rest
            if rest != self.kept and clock.now - self.tried_at >= SAVE_GAP:  # its turn has come: written at once
                self.kept, self.tried_at = rest, clock.now
            return "ok"
        if word not in DRIVES:
            return REPLY_UNKNOWN
        if not re.fullmatch(DRIVES[word], rest):
            return f"err malformed {word}"
        if self.bench:
            return REPLY_BENCH
        if word == "emotion" and " " not in rest:
            self.mood = rest
        return "ok"

    def next_line(self):
        for item in self.output:
            if item[0] <= clock.now:
                self.output.remove(item)
                return item[1]
        return None


class Serial:
    """The part of pyserial the scripts use: the chip resets while RTS is asserted without DTR"""

    def __init__(self):
        self.port, self.baudrate, self.timeout, self.exclusive = None, None, None, None
        self.board = None  # once open
        self._dtr = self._rts = True
        self._rest = b""  # the second part of a cut line

    def open(self):
        board = plugged[self.port]
        if board.open_error:
            raise board.open_error
        if board.monitor_open and self.exclusive:
            raise OSError(f"Could not exclusively lock port {self.port}")
        board.opened += 1
        self.board = board
        self.lines_changed()

    def close(self):
        if self.board:
            self.board.set_reset(False)  # closing drops both lines
            self.board = None

    def __enter__(self):
        return self

    def __exit__(self, *_):
        self.close()

    def lines_changed(self):
        if self.board:
            self.board.set_reset(self._rts and not self._dtr)

    @property
    def dtr(self):
        return self._dtr

    @dtr.setter
    def dtr(self, value):
        if self.board and self.board.dtr_error:
            raise self.board.dtr_error
        self._dtr = value
        self.lines_changed()

    @property
    def rts(self):
        return self._rts

    @rts.setter
    def rts(self, value):
        self._rts = value
        self.lines_changed()
        if value:
            self.board.stop_if("rts")

    def write(self, data):
        if self.board.lost:
            raise OSError("device disconnected")
        clock.now += 0.001
        self.board.receive(data)

    def readline(self):
        if self.board.lost:
            raise OSError("device disconnected")
        if self._rest:
            rest, self._rest = self._rest, b""
            return rest
        line = self.board.next_line()
        if line is None:
            clock.now += self.timeout
            return b""
        data = f"{line}\r\n".encode()
        if self.board.cut:
            clock.now += self.timeout  # readline gave up in the middle of the line
            data, self._rest = data[: self.board.cut], data[self.board.cut :]
        return data


serial = types.ModuleType("serial")
serial.Serial = Serial
serial.tools = types.ModuleType("serial.tools")
serial.tools.list_ports = types.ModuleType("serial.tools.list_ports")
serial.tools.list_ports.comports = lambda: [types.SimpleNamespace(device=port, vid=0x303A) for port in plugged]
sys.modules.update({"serial": serial, "serial.tools": serial.tools, "serial.tools.list_ports": serial.tools.list_ports})

import board_link  # noqa: E402  after the fake, so that it can never reach a real port


def run(script, *boards, **env):
    """Runs a script with these boards plugged in. Returns its exit status, what it wrote on stdout and on stderr."""
    plugged.clear()
    plugged.update({f"/dev/ttyACM{index}": board for index, board in enumerate(boards)})
    out, err = io.StringIO(), io.StringIO()
    with (
        mock.patch.dict(os.environ, env, clear=True),
        mock.patch("time.time", clock.time),
        mock.patch("time.sleep", clock.sleep),
        contextlib.redirect_stdout(out),
        contextlib.redirect_stderr(err),
    ):
        try:
            runpy.run_path(str(SCRIPTS / script), run_name="__main__")
            status = 0
        except SystemExit as stop:
            status = stop.code
    return status, out.getvalue(), err.getvalue()


def guard(*boards, **env):
    return run("flash-guard.py", *boards, **{"BOARD": "atoms3r", **env})


def check_device(board, **env):
    return run("check-device.py", board, **{"BOARD": board.name, **env})


class FirmwareFacts(unittest.TestCase):
    def test_the_replies_the_checks_expect_are_the_firmware_s(self):
        for reply in (REPLY_BENCH, REPLY_NO_MOTION):
            self.assertIn(f'printf("< {reply}\\n");', COMMANDS_C)
        # The engine's errors, which commands.c prints after "err "
        self.assertIn('printf("< err %s\\n", error);', COMMANDS_C)
        command_c = (ROOT / "engine" / "src" / "command.c").read_text()
        for reply in (REPLY_UNKNOWN, REPLY_TOO_LONG):
            self.assertIn(f'return "{reply.removeprefix("err ")}";', command_c)

    def test_the_report_window_holds_a_round_at_the_lowest_rate(self):
        # LP_BENCH_FRAMES, from the enum of bench_script.h (sums of products of names and numbers), with the emotion
        # count of eyes/emotions.json
        known = {"LP_EMOTION_COUNT": len(json.loads((ROOT / "eyes" / "emotions.json").read_text())["emotions"])}

        def term(word):
            if word in known:
                return known[word]
            if not word.isdigit():
                self.fail(f"bench_script.h: {word} is neither a number nor a name defined before")
            return int(word)

        def value(expression):
            return sum(math.prod(term(word) for word in product.split("*")) for product in expression.split("+"))

        bench = (ROOT / "engine" / "tools" / "bench_script.h").read_text()
        for name, expression in re.findall(r"^\s*(LP_BENCH_\w+) = ([\w\s+*]+?),", bench, re.M):
            known[name] = value("".join(expression.split()))  # without spaces or line breaks
        check = (SCRIPTS / "check-device.py").read_text()
        rate = int(re.search(r"^MIN_FRAMES_PER_SECOND = (\d+)", check, re.M).group(1))
        window = int(re.search(r"^REPORT_SECONDS = (\d+)", check, re.M).group(1))
        self.assertGreater(known["LP_BENCH_FRAMES"], 0)
        self.assertLessEqual(known["LP_BENCH_FRAMES"] / rate, window)

    def test_c_define_stops_on_a_missing_define(self):
        with self.assertRaises(SystemExit):
            c_define(COMMANDS, "NO_SUCH_DEFINE")


class HelloFields(unittest.TestCase):
    def test_reads_the_hello_of_the_firmware(self):
        fields = board_link.hello_fields(Board("atoms3r").reply("> hello"))
        self.assertEqual(fields["board"], "atoms3r")
        self.assertEqual(fields["camera"], "0")
        self.assertEqual(fields["motion"], "1")

    def test_no_hello_has_no_fields(self):
        self.assertEqual(board_link.hello_fields(None), {})


class MotionSensor(unittest.TestCase):
    def test_reads_the_parts_of_each_board(self):
        self.assertTrue(board_link.has_motion_sensor("atoms3r"))
        self.assertFalse(board_link.has_motion_sensor("waveshare_132"))
        self.assertFalse(board_link.has_motion_sensor("stamp_s3a"))

    def test_any_sensor_counts(self):
        with mock.patch.object(Path, "read_text", return_value="list(APPEND srcs display.c imu_other.c)\n"):
            self.assertTrue(board_link.has_motion_sensor("atoms3r"))

    def test_leaves_comments_out(self):
        cmake = "# imu_bmi270.c once soldered\nlist(APPEND srcs imu_none.c) # not imu_bmi270.c\n"
        with mock.patch.object(Path, "read_text", return_value=cmake):
            self.assertFalse(board_link.has_motion_sensor("atoms3r"))


class FlashGuard(unittest.TestCase):
    def test_allows_the_same_board(self):
        self.assertEqual(guard(Board("atoms3r"))[:2], (0, f"{PORT}\n"))

    def test_refuses_another_board(self):
        status, out, _ = guard(Board("waveshare_132"))
        self.assertIn(f"{PORT} runs the image of waveshare_132", status)
        self.assertEqual(out, "")

    def test_refuses_a_board_that_missed_the_first_hello(self):
        status, out, _ = guard(Board("waveshare_132", reads_after=1))
        self.assertIn("runs the image of waveshare_132", status)
        self.assertEqual(out, "")

    def test_a_stale_reply_does_not_pass_for_hello(self):
        status, out, _ = guard(Board("waveshare_132", stale=["< ok", "< ok"]))
        self.assertIn("runs the image of waveshare_132", status)
        self.assertEqual(out, "")

    def test_goes_on_with_a_silent_board(self):
        self.assertEqual(guard(Board("waveshare_132", reads_after=math.inf))[:2], (0, f"{PORT}\n"))

    def test_goes_on_when_the_port_fails(self):
        for board in (
            Board("waveshare_132", open_error=OSError("no such port")),
            Board("waveshare_132", open_error=termios.error(5, "Input/output error")),
            Board("waveshare_132", dtr_error=OSError("broken pipe")),
            Board("waveshare_132", monitor_open=True),  # esptool stops on the same lock
        ):
            with self.subTest(open_error=board.open_error, dtr_error=board.dtr_error, monitor=board.monitor_open):
                self.assertEqual(guard(board)[:2], (0, f"{PORT}\n"))

    def test_force_asks_nothing(self):
        board = Board("waveshare_132")
        status, out, err = guard(board, FORCE="1")
        self.assertEqual((status, out), (0, f"{PORT}\n"))
        self.assertIn("FORCE=1", err)
        self.assertEqual(board.opened, 0)

    def test_stops_with_no_board(self):
        # esptool would try every serial port and flash whatever answers, with no hello asked
        status, out, _ = guard()
        self.assertIn("No board found", status)
        self.assertEqual(out, "")

    def test_force_lets_esptool_look_for_a_board(self):
        status, out, err = guard(FORCE="1")
        self.assertEqual((status, out), (0, ""))
        self.assertIn("esptool looks for one", err)

    def test_an_empty_port_means_the_only_board(self):
        status, out, _ = guard(Board("waveshare_132"), PORT="")
        self.assertIn(f"{PORT} runs the image of waveshare_132", status)
        self.assertEqual(out, "")

    def test_reads_a_hello_cut_in_two(self):
        # Its first half has no board=: read alone, it would pass for a silent board
        status, out, _ = guard(Board("waveshare_132", cut=20))
        self.assertIn("runs the image of waveshare_132", status)
        self.assertEqual(out, "")

    def test_stops_when_several_boards_could_be_meant(self):
        status, out, _ = guard(Board("atoms3r"), Board("atoms3r"))
        self.assertIn("Several boards", status)
        self.assertEqual(out, "")

    def test_port_picks_the_board(self):
        status, out, _ = guard(Board("waveshare_132"), Board("atoms3r"), PORT="/dev/ttyACM1")
        self.assertEqual((status, out), (0, "/dev/ttyACM1\n"))

    def test_needs_a_known_board(self):
        status, out, _ = guard(Board("atoms3r"), BOARD="../atoms3r")
        self.assertIn("Set BOARD to one of", status)
        self.assertEqual(out, "")


class DeviceCheck(unittest.TestCase):
    def assert_passes(self, board):
        look = board.look
        status, out, _ = check_device(board)
        self.assertEqual(status, 0, out)
        self.assertIn("all checks passed", out)
        self.assertEqual((board.look, board.mood), (look, "neutral"))

    def test_passes_a_healthy_board(self):
        self.assert_passes(Board("atoms3r"))

    def test_passes_a_board_whose_lines_come_in_two_parts(self):
        self.assert_passes(Board("atoms3r", cut=12))

    def test_passes_a_board_with_no_motion_sensor(self):
        self.assert_passes(Board("waveshare_132", motion=False))

    def test_passes_a_benchmark_board(self):
        self.assert_passes(Board("stamp_s3a", bench=True, motion=False))

    def test_stops_on_another_board(self):
        board = Board("waveshare_132", motion=False)
        look = board.look
        status, out, _ = check_device(board, BOARD="atoms3r")
        self.assertEqual(status, 1)
        self.assertIn("FAIL runs the image of atoms3r", out)
        self.assertEqual(board.look, look)

    def test_a_stale_reply_does_not_pass_for_hello(self):
        self.assert_passes(Board("atoms3r", stale=["< ok"], reads_after=3))

    def test_fails_on_a_screen_fault_in_the_boot_log(self):
        for fault in (
            "display ID not recognized (00 00 00 00 00 00): trying the GC9107 settings",
            "backlight: no answer from the driver at I2C 0x30",
        ):
            with self.subTest(fault):
                status, out, _ = check_device(Board("atoms3r", boot_log=[fault]))
                self.assertEqual(status, 1)
                self.assertIn(f"FAIL starts its screen ({fault})", out)

    def test_fails_a_drawing_loop_that_does_not_report(self):
        status, out, _ = check_device(Board("atoms3r", report=None))
        self.assertEqual(status, 1)
        self.assertIn("FAIL draws at least 20 frames/s (no report", out)

    def test_fails_a_drawing_loop_that_crawls(self):
        status, out, _ = check_device(Board("atoms3r", report="19.9 frames/s | draw mean", report_after=39.2))
        self.assertEqual(status, 1)
        self.assertIn("FAIL draws at least 20 frames/s (19.9 frames/s)", out)

    def test_a_slow_screen_is_not_a_fault(self):
        self.assert_passes(Board("atoms3r", report="20.0 frames/s | draw mean", report_after=39))

    def test_ctrl_c_gives_the_look_and_the_mood_back(self):
        board = Board("atoms3r", stop_at="> emotion happy")
        look = board.look
        status, out, _ = check_device(board)
        self.assertEqual(status, 1)
        self.assertIn("FAIL runs to the end (interrupted)", out)
        self.assertEqual((board.look, board.mood), (look, "neutral"))

    def test_ctrl_c_during_the_reboot_lets_the_chip_go(self):
        board = Board("atoms3r", stop_at="rts")
        look = board.look
        _, out, _ = check_device(board)
        self.assertIn("FAIL runs to the end (interrupted)", out)
        self.assertIn("ok   gets its own look back", out)
        self.assertEqual((board.look, board.mood), (look, "neutral"))

    def test_fails_a_board_that_loses_the_last_of_two_quick_looks(self):
        status, out, _ = check_device(Board("atoms3r", drops_late_looks=True))
        self.assertEqual(status, 1)
        self.assertIn("FAIL still has the last look after the reboot", out)

    def test_fails_a_board_that_does_not_come_back_after_the_reboot(self):
        status, out, _ = check_device(Board("atoms3r", comes_back=False))
        self.assertEqual(status, 1)
        self.assertIn("FAIL reboots on request", out)

    def test_a_port_lost_midway_says_how_to_give_the_look_back(self):
        status, out, _ = check_device(Board("atoms3r", lost_at="> react tap"))
        self.assertEqual(status, 1)
        self.assertIn("FAIL runs to the end (port lost: device disconnected)", out)
        self.assertIn("FAIL gets its own look back (port lost, send it again: > look AE0002 FF5252 FF5252 dot)", out)

    def test_keeps_a_monitor_off_the_port(self):
        status, _, _ = check_device(Board("atoms3r", monitor_open=True))
        self.assertIn("close the serial monitor", status)

    def test_fails_when_the_motion_sensor_does_not_answer(self):
        status, out, _ = check_device(Board("atoms3r", motion=False))
        self.assertEqual(status, 1)
        self.assertIn("FAIL has its motion sensor", out)


if __name__ == "__main__":
    unittest.main()
