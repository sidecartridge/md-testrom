#!/usr/bin/env python3
"""Count power-cycled boots from the debug console.

Run it, then power-cycle the ST (which powers the RP too), waiting for the
desktop each time. For every RP boot it prints how long after the banner the
cartridge window was served (`LIVE_MARK`, the first line the main loop prints)
and whether the boot reached it. The Multidevice Test firmware puts no code on
the ST and the ST says nothing to it, so whether the ST reached the desktop is
for the person at the machine to say. Three boots are the default: power
cycles wear a machine this old.

    python3 tools/dev/power_cycles.py [--boots 3]

Needs a debug build and `tools/dev/console.py watch` running.
"""
import argparse
import datetime
import os
import re
import sys
import time

LOG = os.path.join(os.path.dirname(os.path.abspath(__file__)), "logs", "console.log")
LIVE_WINDOW_S = 20.0
# emul.c prints this once the window is served and the main loop starts.
LIVE_MARK = "Start the app loop here"


def stamp(line):
    return datetime.datetime.strptime(line[:23], "%Y-%m-%d %H:%M:%S.%f")


def follow(path):
    with open(path, "r", errors="replace") as f:
        f.seek(0, os.SEEK_END)
        while True:
            line = f.readline()
            if line:
                yield line.rstrip("\n")
            else:
                yield None
                time.sleep(0.05)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--boots", type=int, default=3)
    ap.add_argument("--timeout", type=float, default=1800, help="give up after this many seconds")
    args = ap.parse_args()

    results = []
    boot = None
    end = time.time() + args.timeout
    print(f"watching {LOG}; power-cycle the ST {args.boots} times")
    for line in follow(LOG):
        now = datetime.datetime.now()
        if boot and boot.get("live") is None and (now - boot["banner"]).total_seconds() > LIVE_WINDOW_S:
            boot["live"] = False
            results.append(boot)
            print(f"boot {len(results)}: window NOT served within {LIVE_WINDOW_S:.0f} s")
            boot = None
        if len(results) >= args.boots or time.time() > end:
            break
        if line is None or len(line) < 24:
            continue
        if re.search(r"App\. v\S+ \(", line):
            if boot and boot.get("live") is None:
                boot["live"] = False
                results.append(boot)
                print(f"boot {len(results)}: window NOT served before the next boot")
            boot = {"banner": stamp(line), "live": None}
        elif boot and boot.get("live") is None and LIVE_MARK in line:
            boot["live"] = (stamp(line) - boot["banner"]).total_seconds() * 1000
            results.append(boot)
            print(f"boot {len(results)}: window served +{boot['live']:.0f} ms after the banner")
            boot = None

    served = sum(1 for b in results if b["live"] is not False)
    print(f"\n{served} of {len(results)} power-cycled boots served the window")
    return 0 if results and served == len(results) else 1


if __name__ == "__main__":
    sys.exit(main())
