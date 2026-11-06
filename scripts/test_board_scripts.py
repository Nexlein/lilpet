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
REPORT = "62.5 frames/s | draw mean 2800 max 6100 us | redrawn 96 per mille of the screen"
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
        boot_log=(),  # what it prints when it starts, past the boot line
        report=REPORT,  # the timing report it prints after a round of the animation, if any
        report_after=12.5,  # seconds after booting
        open_error=None,
        dtr_error=None,
        monitor_open=False,  # a program holds the port with the exclusive lock of pyserial
        stop_at=None,  # Ctrl+C when the board gets this line, or at "rts" when RTS is asserted
    ):
        self.name, self.bench, self.motion = name, bench, motion
        self.boot_log, self.report, self.report_after = boot_log, report, report_after
        self.reads_from = clock.now + reads_after
        self.output = [(clock.now, line) for line in stale]  # when each line reaches the port, and the line
        self.open_error, self.dtr_error, self.monitor_open, self.stop_at = open_error, dtr_error, monitor_open, stop_at
        self.look = "AE0002 FF5252 FF5252 dot"
        self.mood = "neutral"
        self.in_reset = False
        self.input = b""
        self.opened = 0

    def boot(self):
        self.mood = "neutral"
        self.reads_from = clock.now + 1
        for line in ["I (250) main_task: Calling app_main()", *self.boot_log, "lilpet 0.13.0, board ..."]:
            self.output.append((clock.now + 0.3, line))
        if self.report:
            self.output.append((clock.now + 0.3 + self.report_after, self.report))

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
            self.stop_if(line.decode())

    def stop_if(self, event):
        if event == self.stop_at:
            self.stop_at = None
            raise KeyboardInterrupt

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
        self.port, self.baudrate, self.timeout, self.exclusive = None, None, None, None
        self.board = None  # once open
        self._dtr = self._rts = True

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
        self.assertEqual((board.look, board.mood), (look, "neutral"))

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

    def test_keeps_a_monitor_off_the_port(self):
        status, _, _ = check_device(Board("atoms3r", monitor_open=True))
        self.assertIn("close the serial monitor", status)

    def test_fails_when_the_motion_sensor_does_not_answer(self):
        status, out, _ = check_device(Board("atoms3r", motion=False))
        self.assertEqual(status, 1)
        self.assertIn("FAIL has its motion sensor", out)


if __name__ == "__main__":
    unittest.main()
