# What the scripts that talk to a board share: which board, its port, and how to ask it things (protocol in
# docs/protocole.md). uv installs pyserial, from scripts/requirements.txt.
import os
import re
import sys
import termios
import time
from pathlib import Path

import serial
from serial.tools import list_ports

ROOT = Path(__file__).resolve().parent.parent
BOARDS = ROOT / "firmware" / "boards"
ESPRESSIF_USB = 0x303A  # the chip's own USB port: firmware or blank, the board shows up with this vendor
# What a failing port raises: SerialException, which is an OSError, plain OSError (DTR, RTS) or termios.error (opening)
SERIAL_ERRORS = (OSError, termios.error)


def board_from_env():
    """BOARD, which must name a folder of firmware/boards"""
    known = sorted(path.parent.name for path in BOARDS.glob("*/board.cmake"))
    board = os.environ.get("BOARD")
    if board not in known:
        sys.exit(f"Set BOARD to one of: {' '.join(known)}")
    return board


def has_motion_sensor(board):
    """Whether board.cmake links a motion sensor: an imu_*.c other than imu_none.c, comments left out"""
    parts = re.sub(r"#.*", "", (BOARDS / board / "board.cmake").read_text())
    return any(part != "imu_none.c" for part in re.findall(r"\bimu_\w+\.c\b", parts))


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


def ask_hello(port, timeout=3):
    """Asks hello until the reply is one, and returns it without the "< " prefix, or None. Any other reply is left from
    a connection that stopped, and says nothing about the board."""
    end = time.time() + timeout
    while (left := end - time.time()) > 0:
        reply = ask(port, "> hello", left)
        if reply and reply.startswith("lilpet "):
            return reply
    return None


def find_port():
    """PORT, or the only board plugged in, or None. Stops when several could be meant: it may be the wrong one."""
    if "PORT" in os.environ:
        return os.environ["PORT"]
    ports = [port.device for port in list_ports.comports() if port.vid == ESPRESSIF_USB]
    if len(ports) > 1:
        sys.exit(f"Several boards are plugged in ({', '.join(ports)}): choose one with PORT=<port>.")
    return ports[0] if ports else None


def open_port(name):
    """Opens the port without rebooting the board; raises one of SERIAL_ERRORS"""
    port = serial.Serial()
    port.port = name
    port.baudrate = 115200
    port.timeout = 0.2
    port.exclusive = True  # the lock esptool and pyserial-miniterm take: a monitor cannot open during a check
    # Opening asserts both lines: keep DTR while releasing RTS, then release DTR, never RTS alone (same as the app)
    port.dtr = True
    port.rts = False
    port.open()
    try:
        port.dtr = False
        port.write(b"\n")  # ends any half line a previous connection left in the board's input
    except SERIAL_ERRORS:
        port.close()
        raise
    return port


def hello_fields(hello):
    return dict(word.split("=", 1) for word in (hello or "").split() if "=" in word)
