#!/usr/bin/env python3
"""Static guard: only approved modules may transmit on the CAN bus.

Rule (project principle 4): transmission happens only from an explicit user
action through the guarded paths. Learn Mode, the recorder, the web server and
the UI must never call sendMessage() themselves (no automatic replay).

Approved callers: the drivers (can_manager.cpp, mcp2515_can_interface.cpp),
the router (can_service.cpp), OBD-II (obd2_reader.cpp) and vehicle commands
(vehicle_control.cpp). Anything else fails the check.
Exit code 0 = ok, 1 = a forbidden file calls sendMessage().
"""
import re
import sys
from pathlib import Path

ALLOWED = {
    "can_manager.cpp", "mcp2515_can_interface.cpp", "can_service.cpp",
    "obd2_reader.cpp", "vehicle_control.cpp",
}


def strip_comments(text):
    text = re.sub(r"/\*.*?\*/", "", text, flags=re.S)
    return re.sub(r"//[^\n]*", "", text)


def main():
    root = Path(sys.argv[1] if len(sys.argv) > 1 else "src")
    bad = []
    for path in sorted(root.glob("*.cpp")):
        if path.name in ALLOWED:
            continue
        code = strip_comments(path.read_text(errors="replace"))
        for m in re.finditer(r"\bsendMessage\s*\(", code):
            line = code.count("\n", 0, m.start()) + 1
            bad.append("%s (approx. line %d)" % (path.name, line))
    if bad:
        print("Forbidden CAN transmit call(s):")
        for b in bad:
            print("  " + b)
        return 1
    print("OK: only approved modules call sendMessage()")
    return 0


if __name__ == "__main__":
    sys.exit(main())
