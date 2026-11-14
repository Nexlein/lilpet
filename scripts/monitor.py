# Shows what the board prints over USB, on the port that firmware:flash and firmware:check-device use: PORT, or the only
# board plugged in. Opening the port asserts both lines, then pyserial sets DTR, then RTS; the chip reboots on RTS
# without DTR, so DTR stays high while RTS goes low, and opening the port does not reboot the board (checked on the
# AtomS3R, where --dtr 0 rebooted it). Quit with Ctrl+]. uv installs pyserial, from scripts/requirements.txt.
# Usage: mise run firmware:monitor (PORT, or the only board plugged in).
import sys

from board_link import find_port
from serial.tools import miniterm


def main():
    port = find_port()
    if port is None:
        sys.exit("No board found on a USB port of the chip: plug one in, or name its port with PORT=<port>.")
    sys.argv = [sys.argv[0], "--raw", "--rts", "0", "--dtr", "1", port, "115200"]
    miniterm.main()


if __name__ == "__main__":
    main()
