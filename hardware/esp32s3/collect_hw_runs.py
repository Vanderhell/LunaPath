# SPDX-License-Identifier: Apache-2.0
"""Reset the selected ESP32-S3 port and compare deterministic firmware replays."""
import argparse
import time

import serial

def one_run(port, timeout):
    lines = []
    with serial.Serial(port, 115200, timeout=0.2) as device:
        device.dtr = False
        device.rts = True
        time.sleep(0.12)
        device.rts = False
        deadline = time.monotonic() + timeout
        while time.monotonic() < deadline:
            raw = device.readline()
            if not raw:
                continue
            line = raw.strip()
            if line.startswith((b"HW_CYCLES", b"HW_CRC_BACKEND", b"HW_FINAL_PATH=", b"LUNAPATH_HW_")):
                lines.append(line)
            if line.startswith(b"LUNAPATH_HW_PASS") or line.startswith(b"LUNAPATH_HW_FAIL"):
                break
    if not lines or not lines[-1].startswith(b"LUNAPATH_HW_PASS"):
        raise RuntimeError("firmware did not report LUNAPATH_HW_PASS; captured: " + repr(lines))
    final_path = next((line for line in lines if line.startswith(b"HW_FINAL_PATH=")), None)
    if final_path is None:
        raise RuntimeError("firmware did not report final path")
    return lines, (final_path, lines[-1])

def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--port", required=True, help="serial port for the identified ESP32-S3")
    parser.add_argument("--runs", type=int, default=3)
    parser.add_argument("--timeout", type=float, default=45.0)
    args = parser.parse_args()
    summaries = []
    for run in range(1, args.runs + 1):
        lines, summary = one_run(args.port, args.timeout)
        summaries.append(summary)
        print(f"REPLAY {run}/{args.runs}")
        for line in lines:
            print(line.decode("ascii", errors="replace"))
    if any(summary != summaries[0] for summary in summaries[1:]):
        raise SystemExit("FAIL: deterministic path/state/fault summaries differ")
    print(f"PASS: {args.runs}/{args.runs} deterministic summaries identical")

if __name__ == "__main__":
    main()
