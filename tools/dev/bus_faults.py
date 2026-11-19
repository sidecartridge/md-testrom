#!/usr/bin/env python3
"""Check on the device that the line test names each fault a debug firmware
can serve.

MDTEST.TOS runs in bench mode on the ST (an MDTEST.BNC file next to it): the
line test in a loop, each verdict sent to the RP as a frame of ROM3 reads.
For each fault this tool asks the firmware to serve the window as that
faulty bus would (`swd.py app fault`), waits for the next two verdicts (the
first may have started before the fault), compares the class with the one
expected, and serves the window whole again.

    python3 tools/dev/bus_faults.py             # every fault
    python3 tools/dev/bus_faults.py data-low    # one

Needs a debug build (the fault hook is debug-only)
and tools/dev/swd.py's set-up (OPENOCD). The faults act on the served copy
of the window, never on the engine's PIO or DMA: a fault in the PIO sampling
itself stays a hands-on test.
"""
import argparse
import os
import subprocess
import sys
import time

DEV = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, DEV)
import mdtest_window  # noqa: E402
import swd  # noqa: E402

# name: (fault kind, a, b, the class the diagnosis must give), the names
# MDTEST_FAULT_* and MDTEST_CLASS_* of common/mdtest.h.
FAULTS = {
    "data-low": ("FAULT_DATA_LOW", 3, 0, "DATA_LOW"),
    "data-high": ("FAULT_DATA_HIGH", 12, 0, "DATA_HIGH"),
    "data-pair": ("FAULT_DATA_SWAP", 1, 14, "DATA_PAIR"),
    "addr-low": ("FAULT_ADDR_LOW", 8, 0, "ADDR_LOW"),
    "addr-tied": ("FAULT_ADDR_TIED", 2, 7, "ADDR_TIED"),
    "scatter": ("FAULT_SCATTER", 97, 0, "INTERMITTENT"),
    "rom3-is-rom4": ("FAULT_ROM3_IS_ROM4", 0, 0, "BANK_SWAP"),
    "pin-dead": ("FAULT_PIN_DEAD", 6, 0, "PIN_DEAD"),
}
VERDICT_TIMEOUT = 300  # seconds: a failing quick pass on an 8 MHz ST


def swd_cli(*args):
    return subprocess.run([sys.executable, os.path.join(DEV, "swd.py"), *args],
                          capture_output=True, text=True)


def wait_verdicts(elf, after, count):
    end = time.time() + VERDICT_TIMEOUT
    while time.time() < end:
        v = swd.line_verdict(elf)
        if v["count"] >= after + count:
            return v
        time.sleep(5)
    raise SystemExit(f"no verdict in {VERDICT_TIMEOUT} s: is MDTEST.TOS in bench mode?")


def run(elf, name):
    kind, a, b, expected = FAULTS[name]
    layout = mdtest_window.layout()
    start = swd.line_verdict(elf)["count"]
    r = swd_cli("app", "fault", str(layout[kind]), str(a), str(b))
    if r.returncode:
        raise SystemExit(f"{name}: the firmware refused the fault: {r.stdout}{r.stderr}")
    try:
        v = wait_verdicts(elf, start, 2)
    finally:
        swd_cli("app", "fault", "0", "0", "0")
    want = layout["CLASS_" + expected]
    ok = v["verdict"] == "FAIL" and v["class"] == want
    print(f"{'PASS' if ok else 'FAIL'}  {name}: verdict {v['verdict']}, class "
          f"{v['class']} (expected {want} {expected}), first "
          f"${v['first']:06X}, {v['errors']} errors")
    return ok


def main():
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument("faults", nargs="*", choices=[[]] + list(FAULTS), help="default: all")
    ap.add_argument("--elf")
    args = ap.parse_args()
    elf = swd.matching_elf(args.elf)
    # The window whole first: the next verdict must be a PASS.
    swd_cli("app", "fault", "0", "0", "0")
    start = swd.line_verdict(elf)["count"]
    v = wait_verdicts(elf, start, 2)
    if v["verdict"] != "PASS":
        raise SystemExit(f"the line test fails without a fault ({v}): fix that first")
    results = [run(elf, f) for f in (args.faults or FAULTS)]
    return 0 if all(results) else 1


if __name__ == "__main__":
    sys.exit(main())
