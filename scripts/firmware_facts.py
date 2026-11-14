# What the board scripts know of the firmware and the engine, read from their sources where they can be: a define, the
# pupil types, and the replies the checks expect, which scripts/test_board_scripts.py finds in the C sources. Standard
# library only, so that the tests can read it before their fake pyserial is in place.
import json
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent

# Replies of firmware/main/commands.c, and the errors of engine/src/command.c it prints after "err "
REPLY_UNKNOWN = "err unknown command"
REPLY_TOO_LONG = "err line too long"
REPLY_BENCH = "err benchmark animation running"
REPLY_NO_MOTION = "err no motion sensor"


def c_define(path, name):
    """The number a C file defines under that name; stops when it has none"""
    found = re.search(rf"#define\s+{name}\s+(\d+)\b", Path(path).read_text())
    if found is None:
        sys.exit(f"{path} does not define {name}")
    return int(found.group(1))


def pupil_types():
    """The pupil types of eyes/looks.json, which the engine's table is generated from"""
    return list(json.loads((ROOT / "eyes" / "looks.json").read_text())["pupils"])
