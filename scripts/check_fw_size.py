#!/usr/bin/env python3
"""Print firmware size vs. OTA slot for every built environment and fail if
the free space in the slot gets too small.

OTA needs the new image to fit in one app slot, so the free space is the
real limit for adding features. Usage:
    python3 scripts/check_fw_size.py [min_free_bytes]
"""
import csv
import sys
from pathlib import Path

ENVS = (
    ("esp32-s3-devkitc-1", "cartouch_16MB.csv"),
    ("esp32-s3-headless", "cartouch_16MB.csv"),
    ("esp32-s3-4mb", "cartouch_4MB.csv"),
    ("esp32-s3-4mb-psram", "cartouch_4MB.csv"),
)


def app_slot(csv_path):
    with Path(csv_path).open(newline="") as f:
        for row in csv.reader(f):
            if row and row[0].strip() == "app0":
                return int(row[4].strip(), 0)
    raise SystemExit(f"app0 not found in {csv_path}")


def main():
    min_free = int(sys.argv[1]) if len(sys.argv) > 1 else 8192
    failed = False
    print(f"{'environment':22} {'firmware':>10} {'slot':>10} {'free':>9}")
    for env, table in ENVS:
        image = Path(".pio/build") / env / "firmware.bin"
        if not image.is_file():
            print(f"{env:22} (not built)")
            continue
        size, slot = image.stat().st_size, app_slot(table)
        free = slot - size
        flag = ""
        if free < min_free:
            flag, failed = "  <-- TOO SMALL", True
        print(f"{env:22} {size:>10} {slot:>10} {free:>9}{flag}")
    if failed:
        print(f"Free space in an OTA slot is below {min_free} bytes.")
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
