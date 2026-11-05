# /// script
# dependencies = ["pyserial==3.5"]
# ///
# Checks a board flashed with the lilpet firmware, over USB, like the PC app talks to it (protocol in notes/app.md):
# opening the port does not reboot it, it answers hello and tells its board, keeps a look it is sent, keeps it across a
# reboot, takes the commands that drive the eyes, and rejects bad commands and lines too long, which change nothing.
# With a motion sensor, it reads about 1 g at rest; a board that has one must have it working. The board's own look is
# put back at the end.
# Usage: mise run firmware:check-device (PORT=/dev/ttyACM0 by default; BOARD, if set, must be the board plugged in).
# uv installs pyserial, from the header above.
import os
import sys
import time

import serial

PORT = os.environ.get("PORT", "/dev/ttyACM0")
TEST_LOOK = "7DB5F4 C10787 8F1716 cat"  # kite colors, two pupil colors: a look nobody keeps by chance
OTHER_LOOK = "AE0002 FF5252 FF5252 dot"
BOOT_LINE = "Calling app_main"
BOARDS = {"atoms3r", "waveshare_132", "stamp_s3a"}
MOTION_BOARDS = {"atoms3r"}  # a sensor that does not answer on these is a fault
BENCH_BOARDS = {"stamp_s3a"}  # they play the benchmark animation, and refuse what would change it
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


port = serial.Serial()
port.port = PORT
port.baudrate = 115200
port.timeout = 0.2
# Opening asserts both lines: keep DTR while releasing RTS, then release DTR, never RTS alone (same as the app)
port.dtr = True
port.rts = False
try:
    port.open()
except serial.SerialException as err:
    sys.exit(f"cannot open {PORT}: {err}. Plug the board in, or close the serial monitor.")
port.dtr = False
port.write(b"\n")  # ends any half line a previous connection left in the board's input

check(not any(BOOT_LINE in line for line in read_lines(port, 1.5)), "opening the port does not reboot the board")

hello = ask(port, "> hello")
check(bool(hello) and hello.startswith("lilpet fw="), "answers hello", hello or "no reply")
fields = dict(word.split("=", 1) for word in (hello or "").split() if "=" in word)
board = fields.get("board")
check(board in BOARDS, "tells its board", board or "no board")
if "BOARD" in os.environ:
    check(board == os.environ["BOARD"], f"runs the image of {os.environ['BOARD']}", board or "no board")
check(fields.get("camera") in ("0", "1"), "tells if it has a camera", fields.get("camera", "no field"))
check(fields.get("motion") in ("0", "1"), "tells if it has a motion sensor", fields.get("motion", "no field"))

before = ask(port, "> look?")
check(bool(before) and before.startswith("look "), "tells its look", before or "no reply")

check(ask(port, f"> look {TEST_LOOK}") == "ok", "keeps a new look")
check(ask(port, "> look?") == f"look {TEST_LOOK}", "tells the new look back")

check(reboot(port), "reboots on request")
after = ask(port, "> look?")
check(after == f"look {TEST_LOOK}", "still has the look after the reboot", after or "no reply")

bad = ask(port, "> look 7DB5F4 C10787 slit")
check(bool(bad) and bad.startswith("err "), "rejects a malformed look", bad or "no reply")
kept = ask(port, "> look?")
check(kept == f"look {TEST_LOOK}", "keeps its look after a refused one", kept or "no reply")
unknown = ask(port, "> wag")
check(unknown == "err unknown command", "rejects an unknown command", unknown or "no reply")

too_long = ask(port, "> " + "x" * 200)
check(too_long == "err line too long", "rejects a line too long", too_long or "no reply")
hello = ask(port, "> hello")
check(bool(hello) and hello.startswith("lilpet fw="), "answers after a line too long", hello or "no reply")
port.write(b"x" * (LINE_SIZE - 1) + f"> look {OTHER_LOOK}\n".encode())  # right where the firmware's buffer ends
kept = ask(port, "> look?")
check(kept == f"look {TEST_LOOK}", "does not run the end of a line too long", kept or "no reply")

accepted = "err benchmark animation running" if board in BENCH_BOARDS else "ok"
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

BURST = 30  # more than the queue to the drawing loop holds, which must lose none
port.write(b"> react tap\n" * BURST)
replies = [read_reply(port) for _ in range(BURST)]
check(replies == [accepted] * BURST, f"answers a burst of {BURST} commands", f"{replies.count(accepted)} as expected")

if board not in BENCH_BOARDS:
    check(ask(port, "> emotion neutral") == "ok", "gets back to its usual mood")

imu = ask(port, "> imu?")
if board in MOTION_BOARDS:
    check(fields.get("motion") == "1", "has its motion sensor", f"motion={fields.get('motion')}")
if imu == "err no motion sensor" and board not in MOTION_BOARDS:
    print("-    no motion sensor on this board")
else:
    try:
        g = sum(float(v) ** 2 for v in imu.split()[1:4]) ** 0.5 if imu and imu.startswith("imu ") else 0.0
    except ValueError:
        g = 0.0
    check(0.8 < g < 1.2, "reads gravity, about 1 g at rest", imu or "no reply")

if before and before.startswith("look "):
    check(ask(port, f"> {before}") == "ok", "gets its own look back", before[5:])

port.close()
print(f"\n{PORT}: {'all checks passed' if failures == 0 else f'{failures} check(s) failed'}")
sys.exit(1 if failures else 0)
