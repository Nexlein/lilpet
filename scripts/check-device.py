# Checks a board flashed with the lilpet firmware, over USB, like the PC app talks to it (docs/protocole.md): opening
# the port does not reboot it, it answers hello and tells its board, why it last started and that its flash storage is
# as it left it, keeps the last of two looks sent in a row across a reboot, which it tells came from its USB port,
# logs no fault of its screen, sets its battery's charge current if it charges one, keeps drawing, resets itself and
# tells so when its drawing loop hangs, then starts once on the default look, takes the commands that drive the eyes,
# and rejects bad commands and lines too long, which change nothing. It tells its battery, falls asleep when told, its
# screen off, tells so, and a command wakes it. With a motion sensor, it reads about 1 g at rest, and keeps it across
# reboots in the middle of its reads; a board that has one must have it working. The board's own look and usual mood
# are put back at the end, also after Ctrl+C. What it knows about a board comes from its hello and
# firmware/boards/<board>.
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
    charges_battery,
    find_port,
    has_motion_sensor,
    hello_fields,
    next_line,
    open_port,
    read_reply,
)
from firmware_facts import (
    REPLY_BENCH,
    REPLY_NO_MOTION,
    REPLY_TOO_LONG,
    REPLY_UNKNOWN,
    c_define,
    default_look,
    sdkconfig_default,
)

# The checks need two looks the board does not wear, so three. The first, kite colors with two pupil colors, is one
# nobody keeps by chance.
LOOKS = ("7DB5F4 C10787 8F1716 cat", "AE0002 FF5252 FF5252 dot", "2E7D32 FFD54F FFD54F acorn")
DEFAULT_LOOK = default_look()  # what a start after a crash shows, which the look kept across it must not be
# What the firmware prints first as it starts (main.c, which a test compares), or for firmware 0.8.0 and earlier, which
# did not print it, what ESP-IDF logs just before
BOOT_LINE = "lilpet starts"
BOOT = rf"{BOOT_LINE}|Calling app_main"
BOOT_SECONDS = 3
# Only the log tells them: everything else works with a black screen
SCREEN_FAULTS = ("not recognized", "backlight:", "no TE signal")
# What a board that charges its battery prints at each start once the charge current is set (battery_axp2101.c, which a
# test compares): without it, the battery stays unplugged
CHARGE_LINE = "AXP2101 charges at"
REPORT_LINE = r"^(\d+\.\d) frames/s"
# What the drawing loop prints as it falls asleep and wakes up (main.c, which a test compares)
ASLEEP_LINE = "asleep: screen off"
AWAKE_LINE = "awake: screen on"
SLEEP_SECONDS = 2  # for either: a frame, then the screen's own wait
# Catches a dead or crawling drawing loop, not a slow screen: a board reports every LP_BENCH_FRAMES frames
# (engine/tools/bench_script.h), and REPORT_SECONDS waits for a whole round at this rate (a test checks it)
MIN_FRAMES_PER_SECOND = 20
REPORT_SECONDS = 60
WAIT_SECONDS = 10  # for a board that is rebooting, after a flash
# Without the bus clear of firmware/main/board.c, these reboots left the I2C bus held, and the motion sensor lost until
# the next boot, on 9 boots in 120 (issue #33): 40 of them catch it about 96 times in 100, in about 50 s
REBOOTS_DURING_READS = 40

failures = 0
hung = False  # the drawing loop was told to hang, and the watchdog has yet to reset the chip


@dataclass
class Board:
    name: str
    bench: bool  # plays the benchmark animation, and refuses what would change it
    motion: bool  # has a motion sensor: one that does not answer is a fault
    charges: bool  # sets the charge current of its battery at each start


LINE_MAX = c_define(ROOT / "engine" / "include" / "lilpet" / "lilpet.h", "LP_COMMAND_LINE_MAX")  # longest line
# Commands the drawing loop's queue holds between two frames: a burst that size fits without a busy reply
QUEUE_SLOTS = c_define(ROOT / "firmware" / "main" / "commands.c", "QUEUE_SLOTS")
# A look sent sooner than this after the last write to flash is written once this time has passed
SAVE_GAP_SECONDS = c_define(ROOT / "firmware" / "main" / "look_saving.h", "LOOK_SAVE_GAP_US") / 1e6
# A drawing loop hung this long resets the chip
WATCHDOG_SECONDS = sdkconfig_default("ESP_TASK_WDT_TIMEOUT_S")
# Why a start shows the default look and leaves the flash out, once (after_crash in firmware/main/main.c, which a test
# compares)
CRASH_RESETS = ("panic", "int_wdt", "task_wdt", "wdt", "cpu_lockup")


def check(ok, what, detail=""):
    global failures
    failures += 0 if ok else 1
    print(f"{'ok  ' if ok else 'FAIL'} {what}{f' ({detail})' if detail else ''}")


def check_reply(ok, what, reply):
    check(ok, what, reply or "no reply")


def read_lines(port, seconds, until=None):
    """What the board prints for that long, or up to and with the first line that matches `until`"""
    lines, end = [], time.time() + seconds
    while (line := next_line(port, end)) is not None:
        if line:
            lines.append(line)
            if until and re.search(until, line):
                break
    return lines


def reset(port):
    """The ESP32-S3 USB port reboots the chip when RTS is asserted without DTR (what esptool does)"""
    port.dtr = False
    port.rts = True
    time.sleep(0.1)
    port.rts = False


def booted(port, seconds):
    """Returns what the board prints from a boot within `seconds` to its first report, nothing if it does not boot"""
    boot = read_lines(port, seconds, until=BOOT)[-1:]  # what comes before may be a report from before it
    if not (boot and re.search(BOOT, boot[0])):
        return []
    return boot + read_lines(port, REPORT_SECONDS, until=REPORT_LINE)


def reset_reason(port):
    """Why the board last started, as its hello tells it"""
    return hello_fields(ask_hello(port)).get("reset")


def screen_faults(log):
    return "; ".join(line for line in log if any(fault in line for fault in SCREEN_FAULTS))


def reboot(port):
    """Returns what the board prints from its boot to its first report, nothing if it does not boot"""
    reset(port)
    return booted(port, BOOT_SECONDS)


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
    # The second look comes too soon after the first to be written at once: it must be written in its turn
    check(ask(port, f"> look {other_look}") == "ok", "takes a new look")
    check(ask(port, f"> look {test_look}") == "ok", "takes a second look right after")
    check(ask(port, "> look?") == f"look {test_look}", "tells the last look back")
    time.sleep(SAVE_GAP_SECONDS + 0.5)  # its turn, and the write

    # The eyes freeze while the commands still get answers: only the watchdog of the drawing loop can tell. The start
    # that follows resets the CPU only, not the display or the I2C bus: the screen and the sensor are checked again.
    global hung
    hung = True
    reply = ask(port, "> test hang")
    hung = reply == "ok"
    check_reply(hung, "takes the test command that hangs its drawing loop", reply)
    seconds = WATCHDOG_SECONDS + BOOT_SECONDS
    log = booted(port, seconds)
    check(bool(log), "resets itself when its drawing loop hangs", "" if log else f"no reboot in {seconds} s")
    if not log:
        log = reboot(port)  # its drawing loop stays hung until then
    hung = False
    faults = screen_faults(log)
    check(bool(log) and not faults, "starts its screen again after the reset", faults)
    check(bool(log) and re.search(REPORT_LINE, log[-1]), "draws again after the reset", "" if log else "no report")
    fields = hello_fields(ask_hello(port))
    check(fields.get("reset") == "task_wdt", "tells its watchdog reset it", f"reset={fields.get('reset')}")
    if board.motion:
        check(fields.get("motion") == "1", "keeps its motion sensor after the reset", f"motion={fields.get('motion')}")
    shown = ask(port, "> look?")
    check_reply(shown == f"look {DEFAULT_LOOK}", "starts on the default look after a crash", shown)

    log = reboot(port)
    check(bool(log), "reboots on request")
    faults = screen_faults(log)
    check(bool(log) and not faults, "starts its screen", faults)
    if board.charges:
        charging = any(CHARGE_LINE in line for line in log)
        detail = "" if charging else "keep the battery unplugged" if log else "no boot log"
        check(charging, "sets its battery's charge current", detail)
    report = re.search(REPORT_LINE, log[-1]) if log else None
    rate = float(report.group(1)) if report else 0.0
    check(
        rate >= MIN_FRAMES_PER_SECOND,
        f"draws at least {MIN_FRAMES_PER_SECOND} frames/s",
        f"{rate} frames/s" if report else f"no report in {REPORT_SECONDS} s",
    )
    after = ask(port, "> look?")  # the start after the crash showed the default look once, and kept this one
    check_reply(after == f"look {test_look}", "still has the last look after the reboot", after)
    reason = reset_reason(port)
    check(reason == "usb", "tells it was reset by its USB port", f"reset={reason}")

    bad = ask(port, "> look 7DB5F4 C10787 slit")
    check_reply(bool(bad) and bad.startswith("err "), "rejects a malformed look", bad)
    kept = ask(port, "> look?")
    check_reply(kept == f"look {test_look}", "keeps its look after a refused one", kept)
    unknown = ask(port, "> wag")
    check_reply(unknown == REPLY_UNKNOWN, "rejects an unknown command", unknown)

    too_long = ask(port, "> " + "x" * LINE_MAX)
    check_reply(too_long == REPLY_TOO_LONG, "rejects a line too long", too_long)
    hello = ask(port, "> hello")
    check_reply(bool(hello) and hello.startswith("lilpet fw="), "answers after a line too long", hello)
    port.write(b"x" * (LINE_MAX + 1) + f"> look {other_look}\n".encode())  # right where the line stops fitting
    kept = ask(port, "> look?")
    check_reply(kept == f"look {test_look}", "does not run the end of a line too long", kept)

    accepted = REPLY_BENCH if board.bench else "ok"
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

    time.sleep(1 / MIN_FRAMES_PER_SECOND)  # a frame at the slowest rate allowed: the last command accepted is drawn
    burst = QUEUE_SLOTS  # sent at once: the queue holds them all, with no wait that would drop a packet
    port.write(b"> react tap\n" * burst)
    replies = [read_reply(port) for _ in range(burst)]
    expected = f"{replies.count(accepted)} as expected"
    check(replies == [accepted] * burst, f"answers a burst of {burst} commands", expected)

    check_sleep(port)

    imu = ask(port, "> imu?")
    if imu == REPLY_NO_MOTION and not board.motion:
        print("-    no motion sensor on this board")
    else:
        try:
            g = sum(float(v) ** 2 for v in imu.split()[1:4]) ** 0.5 if imu and imu.startswith("imu ") else 0.0
        except ValueError:
            g = 0.0
        check_reply(0.8 < g < 1.2, "reads gravity, about 1 g at rest", imu)
        if 0.8 < g < 1.2:  # a sensor that does not work at rest says nothing more after a reboot
            lost, silent = reboots_during_reads(port)
            check(
                lost == silent == 0,
                f"keeps its motion sensor across {REBOOTS_DURING_READS} reboots during its reads",
                f"lost on {lost} of {REBOOTS_DURING_READS} boots, no hello on {silent}" if lost or silent else "",
            )


def check_sleep(port):
    """Asleep, the screen is off and hello tells it; a command, that hello, wakes it, and the screen comes back"""
    reply = ask(port, "> test sleep")
    check_reply(reply == "ok", "takes the test command that puts it to sleep", reply)
    log = read_lines(port, SLEEP_SECONDS, until=ASLEEP_LINE)
    asleep = any(ASLEEP_LINE in line for line in log)
    check(asleep, "falls asleep, its screen off", "" if asleep else f"no {ASLEEP_LINE!r} in {SLEEP_SECONDS} s")
    state = hello_fields(ask_hello(port)).get("state")
    check(state == "asleep", "tells it sleeps", f"state={state}")
    woken = read_lines(port, SLEEP_SECONDS, until=AWAKE_LINE)
    awake = any(AWAKE_LINE in line for line in woken)
    check(awake, "wakes up on a command, its screen on", "" if awake else f"no {AWAKE_LINE!r} in {SLEEP_SECONDS} s")
    faults = screen_faults(log + woken)
    check(not faults, "turns its screen off and on without a fault", faults)
    state = hello_fields(ask_hello(port)).get("state")
    check(state == "awake", "tells it is awake again", f"state={state}")


def reboots_during_reads(port):
    """Reboots the board while it answers a flood of motion sensor reads, so that resets land in the middle of an I2C
    transfer; returns how many boots came back without the sensor, and how many did not answer hello"""
    lost = silent = 0
    for i in range(REBOOTS_DURING_READS):
        port.write(b"> imu?\n" * 20)
        time.sleep(i % 20 / 1000)  # from 0 to 19 ms: the reset lands while the board reads, at another point each time
        reset(port)
        read_lines(port, BOOT_SECONDS, until=BOOT)
        read_lines(port, 1)  # until it reads: a board still booting keeps only a few bytes of what it is sent
        hello = ask_hello(port)
        silent += hello is None
        lost += hello is not None and hello_fields(hello).get("motion") != "1"
    return lost, silent


def put_back(port, board, before):
    """Gives the board its own look and usual mood back, whatever stopped the checks"""
    try:
        if port.rts:  # stopped during a reboot: the chip stays in reset until RTS is released
            port.rts = False
            read_lines(port, BOOT_SECONDS)
        if hung:  # stopped while its drawing loop hangs: a look sent now would give way to the default one at the reset
            booted(port, WATCHDOG_SECONDS + BOOT_SECONDS)
        port.write(b"\n")  # ends the half line a stop may have left
        read_lines(port, 0.3)  # and lets the replies still on their way go by
        check(ask(port, f"> {before}") == "ok", "gets its own look back", before[5:])
        if not board.bench:
            check(ask(port, "> emotion neutral") == "ok", "gets back to its usual mood")
    except SERIAL_ERRORS:
        check(False, "gets its own look back", f"port lost, send it again: > {before}")


def check_board(board_name):
    config = BOARDS / board_name / "board_config.h"
    board = Board(
        board_name, c_define(config, "BOARD_BENCH") == 1, has_motion_sensor(board_name), charges_battery(board_name)
    )

    port = open_booted_board()
    rebooted = any(re.search(BOOT, line) for line in read_lines(port, 1.5))
    check(not rebooted, "opening the port does not reboot the board")

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
    check(bool(fields.get("reset")), "tells why it last started", fields.get("reset", "no field"))
    storage = fields.get("storage")
    check(storage == "ok", "finds its flash storage as it left it", f"storage={storage}")
    battery = fields.get("battery")  # millivolts, or none: without a battery, or on a board that cannot read one
    check(battery == "none" or str(battery).isdigit(), "tells its battery", f"battery={battery}")
    if fields.get("reset") in CRASH_RESETS:  # it shows the default look this once: its own is in flash, back at a reset
        reboot(port)

    before = ask(port, "> look?")
    check_reply(bool(before) and before.startswith("look "), "tells its look", before)
    if not (before and before.startswith("look ")):
        finish(port)  # its look could not be put back

    # Two looks the board does not wear: a run stopped by a power cut leaves one on it. The one kept is not the default
    looks = [look for look in LOOKS if f"look {look}" != before]
    test_look = next(look for look in looks if look != DEFAULT_LOOK)
    other_look = next(look for look in looks if look != test_look)
    try:
        run_checks(port, board, test_look, other_look)
    except KeyboardInterrupt:
        check(False, "runs to the end", "interrupted")
    except SERIAL_ERRORS as err:
        check(False, "runs to the end", f"port lost: {err}")
    finally:
        put_back(port, board, before)
    finish(port)


try:
    check_board(board_from_env())
except SERIAL_ERRORS as err:
    sys.exit(f"port lost: {err}")
