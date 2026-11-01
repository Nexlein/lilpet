# Checks a board flashed with the lilpet firmware, over USB, like the PC app talks to it (protocol in notes/app.md):
# opening the port does not reboot it, it answers hello, keeps a look it is sent, keeps it across a reboot and rejects
# a bad command; with a motion sensor, it reads about 1 g at rest. The board's own look is put back at the end.
# Usage: mise run firmware:check-device (PORT=/dev/ttyACM0 by default). Runs pyserial through uvx, no install.
import os
import sys
import time

import serial

PORT = os.environ.get("PORT", "/dev/ttyACM0")
TEST_LOOK = "7DB5F4 C10787 8F1716 cat"  # kite colors, two pupil colors: a look nobody keeps by chance
BOOT_LINE = "Calling app_main"

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


def ask(port, command, timeout=3):
    """Sends a command and returns its reply without the "< " prefix, or None"""
    port.write(f"{command}\n".encode())
    end = time.time() + timeout
    while time.time() < end:
        line = port.readline().decode(errors="replace").strip()
        if line.startswith("< "):
            return line[2:]
    return None


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

before = ask(port, "> look?")
check(bool(before) and before.startswith("look "), "tells its look", before or "no reply")

check(ask(port, f"> look {TEST_LOOK}") == "ok", "keeps a new look")
check(ask(port, "> look?") == f"look {TEST_LOOK}", "tells the new look back")

check(reboot(port), "reboots on request")
after = ask(port, "> look?")
check(after == f"look {TEST_LOOK}", "still has the look after the reboot", after or "no reply")

bad = ask(port, "> look 7DB5F4 C10787 slit")
check(bool(bad) and bad.startswith("err "), "rejects a malformed look", bad or "no reply")

imu = ask(port, "> imu?")
if imu == "err no motion sensor":
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
