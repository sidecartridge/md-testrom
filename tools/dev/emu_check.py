#!/usr/bin/env python3
"""Boot the firmware under EmuMD on each TOS and say PASS or FAIL.

Builds build/md-testrom.mdfw (tools/dev/mdfw.sh build), then for each setup
boots EmuMD's Hatari headless with no user configuration and reads the
firmware's own log: a setup passes when the test image was verified in the
window and the main loop started, with no panic. A screenshot of the last
frame and the log are kept in tools/dev/logs/emu/<setup>/.

    python3 tools/dev/emu_check.py                 # every setup
    python3 tools/dev/emu_check.py st-104 mste-206
    python3 tools/dev/emu_check.py --disk-a DISK.ST --frames 3000 st-104

With --disk-a the floppy goes in drive A (a program in its AUTO folder runs
at boot); look at the screenshot for what it printed. Never give EmuMD's
Hatari a GEMDOS drive: it takes the first 1 KB of the cartridge.

TOS images are the links in emu/tos/ (gitignored).
"""

import argparse
import os
import subprocess
import sys

DEV = os.path.dirname(os.path.abspath(__file__))
REPO = os.path.abspath(os.path.join(DEV, "..", ".."))

# name: (TOS image in emu/tos, Hatari machine, frames to boot to the desktop)
SETUPS = {
    "st-104": ("tos104uk.img", "st", 1200),
    "ste-162": ("tos162uk.img", "ste", 2500),
    "mste-206": ("tos206us.img", "megaste", 2500),
}

EXPECT = ("Test image in RAM verified", "Start the app loop here")
FORBID = ("panic", "ERROR:")


def run(setup: str, frames: int | None, disk: str | None) -> bool:
    tos, machine, boot_frames = SETUPS[setup]
    tos_path = os.path.join(REPO, "emu", "tos", tos)
    if not os.path.exists(tos_path):
        print(f"SKIP  {setup}: no {tos_path}")
        return True
    out = os.path.join(DEV, "logs", "emu", setup)
    os.makedirs(out, exist_ok=True)
    log = os.path.join(out, "run.log")
    cmd = [os.path.join(DEV, "mdfw.sh"), "run", "--headless", "--no-user-config",
           "--frames", str(frames or boot_frames), "--tos", tos_path,
           "--machine", machine, "--screenshot", os.path.join(out, "last.png")]
    if disk:
        cmd += ["--", "--disk-a", os.path.abspath(disk)]
    with open(log, "w") as f:
        p = subprocess.run(cmd, cwd=REPO, stdout=f, stderr=subprocess.STDOUT,
                           timeout=1800)
    text = open(log, errors="replace").read()
    missing = [e for e in EXPECT if e not in text]
    found = [b for b in FORBID if b in text]
    ok = p.returncode == 0 and not missing and not found
    detail = []
    if p.returncode:
        detail.append(f"exit {p.returncode}")
    detail += [f"no '{m}'" for m in missing] + [f"'{b}' in the log" for b in found]
    print(f"{'PASS' if ok else 'FAIL'}  {setup}" +
          (f": {', '.join(detail)}" if detail else "") + f"  ({log})")
    return ok


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument("setups", nargs="*", choices=[[]] + list(SETUPS),
                    help="default: all")
    ap.add_argument("--frames", type=int, help="frames to run (default: the "
                    "setup's boot time)")
    ap.add_argument("--disk-a", help="a floppy image for drive A")
    args = ap.parse_args()
    build = subprocess.run([os.path.join(DEV, "mdfw.sh"), "build"], cwd=REPO,
                           capture_output=True, text=True)
    if build.returncode:
        print(build.stdout[-2000:] + build.stderr[-2000:])
        print("FAIL  build")
        return 1
    results = [run(s, args.frames, args.disk_a) for s in (args.setups or SETUPS)]
    return 0 if all(results) else 1


if __name__ == "__main__":
    sys.exit(main())
