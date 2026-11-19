#!/usr/bin/env python3
"""Have MDTEST.TOS in bench mode run a test, with nobody at the ST.

The program, started with an MDTEST.BNC holding "requests" next to it, runs
the line test, selects ROM3 and then polls the RP for requests. This tool
posts one (run TEST with its parameters), waits for the program's result
lines in the screen it mirrors to the RP, and compares them with --expect.

    python3 tools/dev/st_loop.py run dummy 2 --expect seconds=2 --times 10
    python3 tools/dev/st_loop.py run dummy 2 --expect seconds=2 --emu st-104
    python3 tools/dev/st_loop.py stop

On the device it goes through SWD (swd.py request and log: a debug or
release build, the program already looping). With --emu SETUP it boots
EmuMD's Hatari with the program's debug build in bench mode (EmuMD's ROM3
cannot pass the line test, so the debug bench skips it) and checks the same
lines through emu_harness.py.
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

L = mdtest_window.layout()
TESTS = {k[5:].lower(): L[k] for k in L if k.startswith("TEST_")}
RUN_TIMEOUT = 120  # seconds


def device_run(elf, test, params, expects):
    """One request on the device; True when every expect matched."""
    start, _ = swd.channel_log(elf)
    swd.post_request(elf, L["REQUEST_RUN"], [TESTS[test]] + params)
    deadline = time.monotonic() + RUN_TIMEOUT
    seen = []
    while time.monotonic() < deadline:
        time.sleep(0.5)
        count, lines = swd.channel_log(elf)
        new = lines[len(lines) - min(len(lines), count - start):]
        if any(l.startswith("Bench: done") for l in new):
            seen = new
            break
    else:
        print(f"FAIL  no 'Bench: done' in {RUN_TIMEOUT} s: is MDTEST.TOS in "
              "bench mode with 'requests'?")
        return False
    values = dict(l.strip().split("=", 1) for l in seen if "=" in l)
    ok = True
    for e in expects:
        key, _, value = e.partition("=")
        if values.get(key) != value:
            print(f"FAIL  {key}: {values.get(key)!r}, expected {value!r}")
            ok = False
    for line in seen:
        print(f"  | {line}")
    return ok


def emu_run(setup, test, params, expects, times):
    steps = ["@debug", "@bench commands",
             'say "^PASS  The device takes commands" 500']
    words = " ".join(str(w) for w in [TESTS[test]] + params)
    for n in range(1, times + 1):
        steps += [f"request {L['REQUEST_RUN']} {words}", f'sayn {n} "^Bench: test" 500',
                  f'sayn {n} "^Bench: done" 6000']
        steps += [f'sayn {n} "^  {e}$" 100' for e in expects]
    steps += [f"request {L['REQUEST_STOP']}", 'say "^Bench mode: stopped by the RP" 500']
    cmd = [sys.executable, os.path.join(DEV, "emu_harness.py"), "--setup", setup]
    for step in steps:
        cmd += ["--step", step]
    return subprocess.run(cmd).returncode == 0


def main():
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument("--elf")
    sub = ap.add_subparsers(dest="cmd", required=True)
    r = sub.add_parser("run", help="run a test through the program")
    r.add_argument("test", choices=sorted(TESTS))
    r.add_argument("params", nargs="*", type=lambda v: int(v, 0))
    r.add_argument("--expect", action="append", default=[], metavar="KEY=VALUE")
    r.add_argument("--times", type=int, default=1)
    r.add_argument("--emu", metavar="SETUP", help="in EmuMD (emu_harness.py's setups)")
    sub.add_parser("stop", help="end the program's bench loop")
    args = ap.parse_args()

    if args.cmd == "run" and args.emu:
        ok = emu_run(args.emu, args.test, args.params, args.expect, args.times)
        return 0 if ok else 1
    elf = swd.matching_elf(args.elf)
    if args.cmd == "stop":
        swd.post_request(elf, L["REQUEST_STOP"], [])
        return 0
    passed = 0
    with swd.held_openocd():
        for n in range(1, args.times + 1):
            ok = device_run(elf, args.test, args.params, args.expect)
            print(f"{'PASS' if ok else 'FAIL'}  run {n} of {args.times}: {args.test}")
            passed += ok
    return 0 if passed == args.times else 1


if __name__ == "__main__":
    try:
        sys.exit(main())
    except swd.SwdError as e:
        sys.exit(f"st_loop: {e}")
