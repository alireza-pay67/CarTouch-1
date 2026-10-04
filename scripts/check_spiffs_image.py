#!/usr/bin/env python3
"""Fail if a generated SPIFFS image does not fit its partition.

Usage: python3 scripts/check_spiffs_image.py PARTITION_CSV IMAGE
Example: python3 scripts/check_spiffs_image.py cartouch_4MB.csv .pio/build/esp32-s3-4mb/spiffs.bin
"""
import csv
import sys
from pathlib import Path


def spiffs_partition_size(path):
    with Path(path).open(newline="") as stream:
        for row in csv.reader(stream):
            if row and row[0].strip() == "spiffs":
                return int(row[4].strip(), 0)
    raise SystemExit(f"spiffs partition not found in {path}")


def main(argv):
    if len(argv) != 3:
        print(__doc__)
        return 2
    partition, image_path = argv[1], argv[2]
    expected = spiffs_partition_size(partition)
    image = Path(image_path)
    if not image.is_file():
        raise SystemExit(f"SPIFFS image not found: {image}")
    actual = image.stat().st_size
    print(f"{partition}: partition {expected} bytes; image {actual} bytes")
    if actual > expected:
        raise SystemExit(f"Generated SPIFFS image exceeds {partition}")
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
