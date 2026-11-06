# Stops a flash on a board that runs a lilpet firmware and says it is another one: the image of another board drives the
# wrong pins. A blank board says nothing, and neither does an image stopped by its chip check or a firmware older than
# board=: those go on. FORCE=1 replaces the board's image for good.
# Usage: run by BOARD=<board> mise run firmware:flash (PORT, or the only board plugged in).
import os
import sys

from board_link import ask, board_from_env, find_port, hello_fields, open_port


def before_flash(board):
    name = find_port()
    if name is None:
        return  # esptool tells that no board answers
    port = open_port(name)
    hello = ask(port, "> hello", 1.5) or ask(port, "> hello", 1.5)  # twice: a board still booting misses the first
    port.close()
    running = hello_fields(hello).get("board")
    if running in (None, board):
        return
    if os.environ.get("FORCE") == "1":
        print(f"{name} runs the image of {running}: flashing {board} over it (FORCE=1)")
        return
    sys.exit(
        f"{name} runs the image of {running}, not {board}: the image of another board drives the wrong pins.\n"
        f"Flash BOARD={running}, choose another board with PORT=<port>, or replace it for good with FORCE=1."
    )


before_flash(board_from_env())
