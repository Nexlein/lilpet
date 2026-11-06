# /// script
# dependencies = ["pyserial==3.5"]
# ///
# Checks a board flashed with the lilpet firmware, over USB, like the PC app talks to it (protocol in notes/app.md):
# opening the port does not reboot it, it answers hello and tells its board, keeps a look it is sent, keeps it across a
# reboot, takes the commands that drive the eyes, and rejects bad commands and lines too long, which change nothing.
# With a motion sensor, it reads about 1 g at rest; a board that has one must have it working. The board's own look is
# put back at the end, also after Ctrl+C. What it knows about a board comes from its hello and firmware/boards/<board>.
# Usage: BOARD=<board> mise run firmware:check-device (PORT, or the only board plugged in; BOARD must be that board).
# uv installs pyserial, from the header above.
import os
import re
import sys
import time
from pathlib import Path

import serial
from serial.tools import list_ports

BOARDS = Path(__file__).resolve().parent.parent / "firmware" / "boards"
ESPRESSIF_USB = 0x303A  # the chip's own USB port: firmware or blank, the board shows up with this vendor
TEST_LOOK = "7DB5F4 C10787 8F1716 cat"  # kite colors, two pupil colors: a look nobody keeps by chance
OTHER_LOOK = "AE0002 FF5252 FF5252 dot"
BOOT_LINE = "Calling app_main"
LINE_SIZE = 96  # of the firmware (commands.c)

failures = 0


def check(ok, what, detail=""):
    global failures
    failures += 0 if ok else 1
    print(f"{'ok  ' if ok else 'FAIL'} {what}{f' ({detail})' if detail else ''}")


def read_lines(port, seconds):
    lines, end = [], time.time() + seconds
    while time.time() < end:
        line = port.readline().decode(errors="replace").strip()
        if line:
            lines.append(line)
    return lines


def read_reply(port, timeout=3):
    """Returns the next reply without the "< " prefix, or None"""
    end = time.time() + timeout
    while time.time() < end:
        line = port.readline().decode(errors="replace").strip()
        if line.startswith("< "):
            return line[2:]
    return None


def ask(port, command, timeout=3):
    """Sends a command and returns its reply without the "< " prefix, or None"""
    port.write(f"{command}\n".encode())
    return read_reply(port, timeout)


def reboot(port):
    """The ESP32-S3 USB port reboots the chip when RTS is asserted without DTR (what esptool does)"""
    port.dtr = False
    port.rts = True
    time.sleep(0.1)
    port.rts = False
    return any(BOOT_LINE in line for line in read_lines(port, 3))


def find_port():
    """PORT, or the only board plugged in, or None. Stops when several could be meant: the wrong one would be flashed."""
    if "PORT" in os.environ:
        return os.environ["PORT"]
    ports = [port.device for port in list_ports.comports() if port.vid == ESPRESSIF_USB]
    if len(ports) > 1:
        sys.exit(f"Several boards are plugged in ({', '.join(ports)}): choose one with PORT=<port>.")
    return ports[0] if ports else None


def open_port(name):
    port = serial.Serial()
    port.port = name
    port.baudrate = 115200
    port.timeout = 0.2
    # Opening asserts both lines: keep DTR while releasing RTS, then release DTR, never RTS alone (same as the app)
    port.dtr = True
    port.rts = False
    try:
        port.open()
    except serial.SerialException as err:
        sys.exit(f"cannot open {name}: {err}. Plug the board in, or close the serial monitor.")
    port.dtr = False
    port.write(b"\n")  # ends any half line a previous connection left in the board's input
    return port


def hello_fields(hello):
    return dict(word.split("=", 1) for word in (hello or "").split() if "=" in word)


def finish(port):
    port.close()
    print(f"\n{port.port}: {'all checks passed' if failures == 0 else f'{failures} check(s) failed'}")
    sys.exit(1 if failures else 0)


def run_checks(port, test_look, other_look, bench, motion, fields):
    """Everything that changes the board's look or mood"""
    check(ask(port, f"> look {test_look}") == "ok", "keeps a new look")
    check(ask(port, "> look?") == f"look {test_look}", "tells the new look back")

    check(reboot(port), "reboots on request")
    after = ask(port, "> look?")
    check(after == f"look {test_look}", "still has the look after the reboot", after or "no reply")

    bad = ask(port, "> look 7DB5F4 C10787 slit")
    check(bool(bad) and bad.startswith("err "), "rejects a malformed look", bad or "no reply")
    kept = ask(port, "> look?")
    check(kept == f"look {test_look}", "keeps its look after a refused one", kept or "no reply")
    unknown = ask(port, "> wag")
    check(unknown == "err unknown command", "rejects an unknown command", unknown or "no reply")

    too_long = ask(port, "> " + "x" * 200)
    check(too_long == "err line too long", "rejects a line too long", too_long or "no reply")
    hello = ask(port, "> hello")
    check(bool(hello) and hello.startswith("lilpet fw="), "answers after a line too long", hello or "no reply")
    port.write(b"x" * (LINE_SIZE - 1) + f"> look {other_look}\n".encode())  # right where the firmware's buffer ends
    kept = ask(port, "> look?")
    check(kept == f"look {test_look}", "does not run the end of a line too long", kept or "no reply")

    accepted = "err benchmark animation running" if bench else "ok"
    for command in ["> emotion happy", "> emotion love 1.5", "> react tap", "> react gesture_like", "> gaze 0.5 -0.25"]:
        reply = ask(port, command)
        check(reply == accepted, f'answers "{command}" as it should', reply or "no reply")
    reply = ask(port, "> gaze off")
    check(reply == accepted, 'answers "> gaze off" as it should', reply or "no reply")

    for command in [
        "> emotion furious",
        "> emotion",
        "> emotion happy 0",
        "> emotion happy -1",
        "> emotion happy nan",
        "> emotion happy inf",
        "> emotion happy 2 now",
        "> react wag",
        "> react",
        "> react tap now",
        "> gaze 2 0",
        "> gaze 0 nan",
        "> gaze 0.5",
        "> gaze 0 0 0",
        "> gaze on",
    ]:
        reply = ask(port, command)
        check(bool(reply) and reply.startswith("err "), f'rejects "{command}"', reply or "no reply")

    burst = 30  # more than the queue to the drawing loop holds, which must lose none
    port.write(b"> react tap\n" * burst)
    replies = [read_reply(port) for _ in range(burst)]
    check(replies == [accepted] * burst, f"answers a burst of {burst} commands", f"{replies.count(accepted)} as expected")

    if not bench:
        check(ask(port, "> emotion neutral") == "ok", "gets back to its usual mood")

    imu = ask(port, "> imu?")
    if motion:
        check(fields.get("motion") == "1", "has its motion sensor", f"motion={fields.get('motion')}")
    if imu == "err no motion sensor" and not motion:
        print("-    no motion sensor on this board")
    else:
        try:
            g = sum(float(v) ** 2 for v in imu.split()[1:4]) ** 0.5 if imu and imu.startswith("imu ") else 0.0
        except ValueError:
            g = 0.0
        check(0.8 < g < 1.2, "reads gravity, about 1 g at rest", imu or "no reply")


def put_back(port, before):
    """Gives the board its own look back, whatever stopped the checks"""
    try:
        port.write(b"\n")  # ends the half line a stop may have left
        read_lines(port, 0.3)  # and lets the replies still on their way go by
        check(ask(port, f"> {before}") == "ok", "gets its own look back", before[5:])
    except serial.SerialException:
        check(False, "gets its own look back", f"port lost, send it again: > {before}")


def check_board(board):
    name = find_port()
    if name is None:
        sys.exit("No board plugged in: plug it in, or name its port with PORT=<port>.")
    # They play the benchmark animation, and refuse what would change it
    bench = re.search(r"#define\s+BOARD_BENCH\s+1\b", (BOARDS / board / "board_config.h").read_text()) is not None
    motion = "imu_none.c" not in (BOARDS / board / "board.cmake").read_text()  # a sensor that does not answer is a fault

    port = open_port(name)
    check(not any(BOOT_LINE in line for line in read_lines(port, 1.5)), "opening the port does not reboot the board")

    hello = ask(port, "> hello")
    check(bool(hello) and hello.startswith("lilpet fw="), "answers hello", hello or "no reply")
    fields = hello_fields(hello)
    check(fields.get("board") == board, f"runs the image of {board}", fields.get("board", "no board"))
    if fields.get("board") != board:
        finish(port)  # not the board asked for, or no firmware: nothing else would mean anything
    check(fields.get("camera") in ("0", "1"), "tells if it has a camera", fields.get("camera", "no field"))
    check(fields.get("motion") in ("0", "1"), "tells if it has a motion sensor", fields.get("motion", "no field"))

    before = ask(port, "> look?")
    check(bool(before) and before.startswith("look "), "tells its look", before or "no reply")
    if not (before and before.startswith("look ")):
        finish(port)  # its look could not be put back

    # A run stopped by a power cut leaves the test look on the board: the checks need one it does not wear
    test_look, other_look = (OTHER_LOOK, TEST_LOOK) if before == f"look {TEST_LOOK}" else (TEST_LOOK, OTHER_LOOK)
    try:
        run_checks(port, test_look, other_look, bench, motion, fields)
    except KeyboardInterrupt:
        check(False, "runs to the end", "interrupted")
    except serial.SerialException as err:
        check(False, "runs to the end", f"port lost: {err}")
    finally:
        put_back(port, before)
    finish(port)


known = sorted(path.parent.name for path in BOARDS.glob("*/board.cmake"))
BOARD = os.environ.get("BOARD")
if BOARD not in known:
    sys.exit(f"Set BOARD to one of: {' '.join(known)}")
check_board(BOARD)
