"""Reset <PORT>, capture the firmware summary, and compare deterministic replays."""
import argparse
import re
import time

import serial

MARKER = re.compile(rb"LUNA_EMBEDDED_HW_(?:PASS|FAIL).*")

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
            if line.startswith((b"HW_CYCLES", b"HW_CRC_BACKEND", b"HW_FINAL_PATH=", b"LUNA_EMBEDDED_HW_")):
                lines.append(line)
            if line.startswith(b"LUNA_EMBEDDED_HW_PASS") or line.startswith(b"LUNA_EMBEDDED_HW_FAIL"):
                break
    if not lines or not lines[-1].startswith(b"LUNA_EMBEDDED_HW_PASS"):
        raise RuntimeError("firmware did not report LUNA_EMBEDDED_HW_PASS; captured: " + repr(lines))
    final_path = next((line for line in lines if line.startswith(b"HW_FINAL_PATH=")), None)
    if final_path is None:
        raise RuntimeError("firmware did not report final path")
    return lines, (final_path, lines[-1])

def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--port", default="<PORT>")
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
