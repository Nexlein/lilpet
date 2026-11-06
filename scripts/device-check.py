# Checks a board flashed with the lilpet firmware, over USB, like the PC app talks to it (protocol in notes/app.md):
# opening the port does not reboot it, it answers hello and tells its board, keeps a look it is sent, keeps it across a
# reboot, logs no fault of its screen, takes the commands that drive the eyes, and rejects bad commands and lines too
# long, which change nothing.
# With a motion sensor, it reads about 1 g at rest; a board that has one must have it working. The board's own look is
# put back at the end, also after Ctrl+C. What it knows about a board comes from its hello and firmware/boards/<board>.
# Usage: BOARD=<board> mise run firmware:check-device (PORT, or the only board plugged in; BOARD must be that board).
# uv installs pyserial, from scripts/requirements.txt.
import re
import sys
import time
from dataclasses import dataclass

from board_link import (
    BOARDS,
    ROOT,
    SERIAL_ERRORS,
    ask,
    ask_hello,
    board_from_env,
    find_port,
    has_motion_sensor,
    hello_fields,
    open_port,
    read_reply,
)

TEST_LOOK = "7DB5F4 C10787 8F1716 cat"  # kite colors, two pupil colors: a look nobody keeps by chance
OTHER_LOOK = "AE0002 FF5252 FF5252 dot"
BOOT_LINE = "Calling app_main"
SCREEN_FAULTS = ("not recognized", "backlight:")  # only the log tells them: everything else works with a black screen
WAIT_SECONDS = 10  # for a board that is rebooting, after a flash

failures = 0


@dataclass
class Board:
    name: str
    bench: bool  # plays the benchmark animation, and refuses what would change it
    motion: bool  # has a motion sensor: one that does not answer is a fault


def define(header, name):
    """The number a C header defines under that name"""
    found = re.search(rf"#define\s+{name}\s+(\d+)\b", header.read_text())
    if found is None:
        sys.exit(f"{header} does not define {name}")
    return int(found.group(1))


LINE_MAX = define(ROOT / "engine" / "include" / "lilpet" / "lilpet.h", "LP_COMMAND_LINE_MAX")  # longest line


def check(ok, what, detail=""):
    global failures
    failures += 0 if ok else 1
    print(f"{'ok  ' if ok else 'FAIL'} {what}{f' ({detail})' if detail else ''}")


def check_reply(ok, what, reply):
    check(ok, what, reply or "no reply")


def read_lines(port, seconds):
    lines, end = [], time.time() + seconds
    while time.time() < end:
        line = port.readline().decode(errors="replace").strip()
        if line:
            lines.append(line)
    return lines


def reboot(port):
    """Returns what the board prints as it starts, nothing if it does not. The ESP32-S3 USB port reboots the chip when
    RTS is asserted without DTR (what esptool does)."""
    port.dtr = False
    port.rts = True
    time.sleep(0.1)
    port.rts = False
    lines = read_lines(port, 3)
    booted = [index for index, line in enumerate(lines) if BOOT_LINE in line]
    return lines[booted[0] :] if booted else []


def open_booted_board():
    """Opens the port of a board that has booted, or one that stays silent for WAIT_SECONDS. After a flash the port
    comes and goes, and a boot in progress must not pass for a reboot caused by opening the port: the port is opened
    again once the board answers, and a boot line after that can only come from the opening."""
    problem = "No board plugged in: plug it in, or name its port with PORT=<port>."
    end = time.time() + WAIT_SECONDS
    while True:
        name = find_port()
        if name is not None:
            try:
                with open_port(name) as port:
                    answered = ask_hello(port, 2)  # not sooner: a board not reading yet only takes a few bytes
                if answered or time.time() >= end:
                    return open_port(name)
            except SERIAL_ERRORS as err:
                problem = f"cannot open {name}: {err}. Plug the board in, or close the serial monitor."
        if time.time() >= end:
            sys.exit(problem)
        time.sleep(0.2)


def finish(port):
    port.close()
    print(f"\n{port.port}: {'all checks passed' if failures == 0 else f'{failures} check(s) failed'}")
    sys.exit(1 if failures else 0)


def run_checks(port, board, test_look, other_look):
    """Everything that changes the board's look or mood"""
    check(ask(port, f"> look {test_look}") == "ok", "keeps a new look")
    check(ask(port, "> look?") == f"look {test_look}", "tells the new look back")

    log = reboot(port)
    check(bool(log), "reboots on request")
    faults = [line for line in log if any(fault in line for fault in SCREEN_FAULTS)]
    check(bool(log) and not faults, "starts its screen", "; ".join(faults))
    after = ask(port, "> look?")
    check_reply(after == f"look {test_look}", "still has the look after the reboot", after)

    bad = ask(port, "> look 7DB5F4 C10787 slit")
    check_reply(bool(bad) and bad.startswith("err "), "rejects a malformed look", bad)
    kept = ask(port, "> look?")
    check_reply(kept == f"look {test_look}", "keeps its look after a refused one", kept)
    unknown = ask(port, "> wag")
    check_reply(unknown == "err unknown command", "rejects an unknown command", unknown)

    too_long = ask(port, "> " + "x" * LINE_MAX)
    check_reply(too_long == "err line too long", "rejects a line too long", too_long)
    hello = ask(port, "> hello")
    check_reply(bool(hello) and hello.startswith("lilpet fw="), "answers after a line too long", hello)
    port.write(b"x" * (LINE_MAX + 1) + f"> look {other_look}\n".encode())  # right where the line stops fitting
    kept = ask(port, "> look?")
    check_reply(kept == f"look {test_look}", "does not run the end of a line too long", kept)

    accepted = "err benchmark animation running" if board.bench else "ok"
    for command in ["> emotion happy", "> emotion love 1.5", "> react tap", "> react gesture_like", "> face 0.5 -0.25"]:
        reply = ask(port, command)
        check_reply(reply == accepted, f'answers "{command}" as it should', reply)
    reply = ask(port, "> face off")
    check_reply(reply == accepted, 'answers "> face off" as it should', reply)

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
        "> face 2 0",
        "> face 0 nan",
        "> face 0.5",
        "> face 0 0 0",
        "> face on",
    ]:
        reply = ask(port, command)
        check_reply(bool(reply) and reply.startswith("err "), f'rejects "{command}"', reply)

    burst = 30  # more than the queue to the drawing loop holds, which must lose none
    port.write(b"> react tap\n" * burst)
    replies = [read_reply(port) for _ in range(burst)]
    expected = f"{replies.count(accepted)} as expected"
    check(replies == [accepted] * burst, f"answers a burst of {burst} commands", expected)

    if not board.bench:
        check(ask(port, "> emotion neutral") == "ok", "gets back to its usual mood")

    imu = ask(port, "> imu?")
    if imu == "err no motion sensor" and not board.motion:
        print("-    no motion sensor on this board")
    else:
        try:
            g = sum(float(v) ** 2 for v in imu.split()[1:4]) ** 0.5 if imu and imu.startswith("imu ") else 0.0
        except ValueError:
            g = 0.0
        check_reply(0.8 < g < 1.2, "reads gravity, about 1 g at rest", imu)


def put_back(port, before):
    """Gives the board its own look back, whatever stopped the checks"""
    try:
        port.write(b"\n")  # ends the half line a stop may have left
        read_lines(port, 0.3)  # and lets the replies still on their way go by
        check(ask(port, f"> {before}") == "ok", "gets its own look back", before[5:])
    except SERIAL_ERRORS:
        check(False, "gets its own look back", f"port lost, send it again: > {before}")


def check_board(board_name):
    config = BOARDS / board_name / "board_config.h"
    board = Board(board_name, define(config, "BOARD_BENCH") == 1, has_motion_sensor(board_name))

    port = open_booted_board()
    check(not any(BOOT_LINE in line for line in read_lines(port, 1.5)), "opening the port does not reboot the board")

    hello = ask_hello(port)
    check_reply(bool(hello) and hello.startswith("lilpet fw="), "answers hello", hello)
    fields = hello_fields(hello)
    check(fields.get("board") == board.name, f"runs the image of {board.name}", fields.get("board", "no board"))
    if fields.get("board") != board.name:
        finish(port)  # not the board asked for, or no firmware: nothing else would mean anything
    check(fields.get("camera") in ("0", "1"), "tells if it has a camera", fields.get("camera", "no field"))
    check(fields.get("motion") in ("0", "1"), "tells if it has a motion sensor", fields.get("motion", "no field"))
    if board.motion:
        check(fields.get("motion") == "1", "has its motion sensor", f"motion={fields.get('motion')}")

    before = ask(port, "> look?")
    check_reply(bool(before) and before.startswith("look "), "tells its look", before)
    if not (before and before.startswith("look ")):
        finish(port)  # its look could not be put back

    # A run stopped by a power cut leaves the test look on the board: the checks need one it does not wear
    test_look, other_look = (OTHER_LOOK, TEST_LOOK) if before == f"look {TEST_LOOK}" else (TEST_LOOK, OTHER_LOOK)
    try:
        run_checks(port, board, test_look, other_look)
    except KeyboardInterrupt:
        check(False, "runs to the end", "interrupted")
    except SERIAL_ERRORS as err:
        check(False, "runs to the end", f"port lost: {err}")
    finally:
        put_back(port, before)
    finish(port)


try:
    check_board(board_from_env())
except SERIAL_ERRORS as err:
    sys.exit(f"port lost: {err}")
