# Stops a flash on a board that runs a lilpet firmware and says it is another one: the image of another board drives the
# wrong pins. A blank board says nothing, and neither does an image stopped by its chip check, a firmware older than
# board= or a board that keeps rebooting, whose port fails: those go on. FORCE=1 asks nothing: it replaces the board's
# image for good, and it is the way out when nothing else answers.
# Usage: run by BOARD=<board> mise run firmware:flash (PORT, or the only board plugged in).
import os
import sys

from board_link import SERIAL_ERRORS, ask, board_from_env, find_port, hello_fields, open_port


def running_board(name):
    """The board that the firmware on the port says it is, or None: a port that fails counts as a silent board"""
    try:
        with open_port(name) as port:
            # Twice: a board still booting misses the first
            hello = ask(port, "> hello", 1.5) or ask(port, "> hello", 1.5)
    except SERIAL_ERRORS:
        return None
    return hello_fields(hello).get("board")


def before_flash(board):
    name = find_port()
    if name is None:
        return  # esptool tells that no board answers
    if os.environ.get("FORCE") == "1":
        print(f"{name}: flashing {board} over whatever it runs (FORCE=1)")
        return
    running = running_board(name)
    if running in (None, board):
        return
    sys.exit(
        f"{name} runs the image of {running}, not {board}: the image of another board drives the wrong pins.\n"
        f"Flash BOARD={running}, choose another board with PORT=<port>, or replace it for good with FORCE=1."
    )


before_flash(board_from_env())
