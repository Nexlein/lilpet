# Tests of the scripts that talk to a board, with no hardware: the fake pyserial below plays a board on a fake clock, so
# no real port is opened and no test waits. Its hello comes from firmware/main/commands.c: a field renamed there fails
# here. Standard library only.
# Usage: npm run check:scripts
import contextlib
import io
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

SCRIPTS = Path(__file__).resolve().parent
ROOT = SCRIPTS.parent
PORT = "/dev/ttyACM0"

HELLO = re.search(r'"< (lilpet [^"]*)\\n"', (ROOT / "firmware" / "main" / "commands.c").read_text()).group(1)
LINE_MAX = int(
    re.search(
        r"LP_COMMAND_LINE_MAX\s+(\d+)", (ROOT / "engine" / "include" / "lilpet" / "lilpet.h").read_text()
    ).group(1)
)
LOOK = r"[0-9A-F]{6} [0-9A-F]{6} [0-9A-F]{6} (dot|cat|acorn|circle)"
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
        open_error=None,
        dtr_error=None,
    ):
        self.name, self.bench, self.motion = name, bench, motion
        self.reads_from = clock.now + reads_after
        self.output = [(clock.now, line) for line in stale]  # when each line reaches the port, and the line
        self.open_error, self.dtr_error = open_error, dtr_error
        self.look = "AE0002 FF5252 FF5252 dot"
        self.mood = "neutral"
        self.in_reset = False
        self.input = b""
        self.opened = 0

    def boot(self):
        self.mood = "neutral"
        self.reads_from = clock.now + 1
        self.output.append((clock.now + 0.3, "I (250) main_task: Calling app_main()"))

    def set_reset(self, held):
        if self.in_reset and not held:
            self.boot()
        self.in_reset = held

    def receive(self, data):
        if self.in_reset or clock.now < self.reads_from:
            return
        *lines, self.input = (self.input + data).split(b"\n")
        for line in lines:
            reply = self.reply(line.decode())
            if reply:
                self.output.append((clock.now, f"< {reply}"))

    def reply(self, text):
        if not text.startswith("> "):
            return None
        if len(text) > LINE_MAX:
            return "err line too long"
        if text == "> hello":
            return HELLO % ("0.5.0", "0.13.0", self.name, 0, self.motion)
        if text == "> look?":
            return f"look {self.look}"
        if text == "> imu?":
            return "imu 0.000 1.000 0.000" if self.motion else "err no motion sensor"
        word, _, rest = text[2:].partition(" ")
        if word == "look":
            if not re.fullmatch(LOOK, rest):
                return "err malformed look"
            self.look = rest
            return "ok"
        if word not in DRIVES:
            return "err unknown command"
        if not re.fullmatch(DRIVES[word], rest):
            return f"err malformed {word}"
        if self.bench:
            return "err benchmark animation running"
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
        self.port, self.baudrate, self.timeout = None, None, None
        self.board = None  # once open
        self._dtr = self._rts = True

    def open(self):
        board = plugged[self.port]
        if board.open_error:
            raise board.open_error
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

    def write(self, data):
        clock.now += 0.001
        self.board.receive(data)

    def readline(self):
        line = self.board.next_line()
        if line is None:
            clock.now += self.timeout
            return b""
        return f"{line}\r\n".encode()


serial = types.ModuleType("serial")
serial.Serial = Serial
serial.tools = types.ModuleType("serial.tools")
serial.tools.list_ports = types.ModuleType("serial.tools.list_ports")
serial.tools.list_ports.comports = lambda: [types.SimpleNamespace(device=port, vid=0x303A) for port in plugged]
sys.modules.update(
    {"serial": serial, "serial.tools": serial.tools, "serial.tools.list_ports": serial.tools.list_ports}
)

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
    return run("device-check.py", board, **{"BOARD": board.name, **env})


class HelloFields(unittest.TestCase):
    def test_reads_the_hello_of_the_firmware(self):
        fields = board_link.hello_fields(Board("atoms3r").reply("> hello"))
        self.assertEqual(fields["board"], "atoms3r")
        self.assertEqual(fields["camera"], "0")
        self.assertEqual(fields["motion"], "1")

    def test_no_hello_has_no_fields(self):
        self.assertEqual(board_link.hello_fields(None), {})


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

    def test_goes_on_with_a_silent_board(self):
        self.assertEqual(guard(Board("waveshare_132", reads_after=math.inf))[:2], (0, f"{PORT}\n"))

    def test_goes_on_when_the_port_fails(self):
        for board in (
            Board("waveshare_132", open_error=OSError("no such port")),
            Board("waveshare_132", open_error=termios.error(5, "Input/output error")),
            Board("waveshare_132", dtr_error=OSError("broken pipe")),
        ):
            with self.subTest(open_error=board.open_error, dtr_error=board.dtr_error):
                self.assertEqual(guard(board)[:2], (0, f"{PORT}\n"))

    def test_force_asks_nothing(self):
        board = Board("waveshare_132")
        status, out, err = guard(board, FORCE="1")
        self.assertEqual((status, out), (0, f"{PORT}\n"))
        self.assertIn("FORCE=1", err)
        self.assertEqual(board.opened, 0)

    def test_prints_nothing_with_no_board(self):
        self.assertEqual(guard()[:2], (0, ""))

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
        self.assertEqual(board.look, look)

    def test_passes_a_healthy_board(self):
        self.assert_passes(Board("atoms3r"))

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

    def test_fails_when_the_motion_sensor_does_not_answer(self):
        status, out, _ = check_device(Board("atoms3r", motion=False))
        self.assertEqual(status, 1)
        self.assertIn("FAIL has its motion sensor", out)


if __name__ == "__main__":
    unittest.main()
