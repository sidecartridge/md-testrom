#!/usr/bin/env python3
"""Run MDTEST.TOS against the firmware in EmuMD's Hatari, drive it with steps,
and end in PASS or FAIL with the evidence.

  --hatari fork   EmuMD's fork of Hatari with this firmware on the cartridge
                  port (build/md-testrom.mdfw, the firmware's own C code).
  --hatari bare   the same Hatari with nothing on the cartridge port.

The program runs from a floppy image's AUTO folder (built from
app/atarist/dist/MDTEST.TOS unless --disk-a gives one), and prints every line
it shows to Hatari's console through NatFeats ("MDTEST: <line>"), which the
steps read. Never give this Hatari a GEMDOS drive: it takes the first 1 KB of
the cartridge.

Setups (TOS images linked in emu/tos/, gitignored):
  st-104      ST, TOS 1.04, 512 KB          ste-162   STE, TOS 1.62
  mste-206    Mega STE, TOS 2.06            tt-306    TT, TOS 3.06
  falcon-404  Falcon, TOS 4.04

Steps (--step, or --test NAME for tools/dev/emu_tests/NAME.steps):

  wait FRAMES           let FRAMES frames pass (50 a second); "boot" as
                        FRAMES in any step is the setup's time to boot
  key KEY               press and release a key: one character, or an ST
                        scancode in hex (0x1c is Return)
  say REGEX FRAMES      wait until the program has shown a line matching
                        REGEX
  sayn N REGEX FRAMES   wait until the program has shown N lines matching
                        REGEX
  nosay REGEX           FAIL if the program has shown a line matching REGEX
  machine               the program's machine line names the setup's machine
  log REGEX FRAMES      wait until Hatari's output (the firmware's log
                        included) matches REGEX
  request CODE [TEST P0 P1 P2 P3]
                        the bench's next request to the program
                        (MDTEST_REQUEST_* in common/mdtest.h)
  select [MS]           SELECT on the device pressed for MS ms (100)
  card out|in           the microSD card taken out of the device, or put back
  sdsame                at the end: the microSD card's folder holds what it
                        held before the run, file for file
  shot NAME             a screenshot, shots/NAME.png
  floppy NAME REGEX     at the end, with Hatari stopped: the floppy holds
                        the file NAME and its text matches REGEX
  nofloppy NAME         at the end: the floppy has no file NAME
  sdfile NAME REGEX     at the end: the card's folder holds NAME (a report
                        the firmware saved) and its text matches REGEX;
                        sdnofile NAME and sdfilenot NAME REGEX the opposite

A test's lines (or --step values) "@hatari bare", "@monitor mono|vga", "@memsize KB",
"@layout N" (the program built to read window layout N, to show a version
mismatch), "@protect on" (the floppy write-protected) and "@fault KIND A B"
(the firmware serves the window as that faulty bus would, MDTEST_FAULT_* in
common/mdtest.h), "@debug" (the program's debug build, which can override
the gate), "@protocol N" (the program built to speak protocol N), "@drop N"
(every Nth command runs unanswered), "@bench MODE" (the firmware asks the
program for bench mode: line, requests or commands), "@twice" (the program
twice in the AUTO folder), "@chipfail" (the radio chip fails to start, as a dead
one would), "@cardfault N" (the microSD card fails as CARDTEST_FAULT_N in
rp/src/include/cardtest.h), "@sdapps" (Booster's apps folder on the card),
"@wifi HOW" (EmuMD's Wi-Fi: badauth, nonet, fail or off), "@rssi DBM"
(the signal it reports), "@wifinotsetup" (Booster's Wi-Fi read as not set
up to join a network), "@btfail" (the Bluetooth controller does not
start) and "@btnone" (a Bluetooth scan hears nothing) set those options;
"@skip SETUP... -- WHY" reports the test as SKIP on those setups, for what
Hatari cannot show there. The card's folder always holds one file of the user's,
USER.TXT.

The run folder, tools/dev/logs/emu/<date>-<setup>-<name>/, keeps
command.txt, hatari.out, the program's lines (screen.txt), shots/ (end.png
last), the floppy copy, its files after the run (floppy-files/) and
report.json. Standard library and mtools only.

Usage:  emu_harness.py --setup st-104 --test main-screen
        emu_harness.py --all                  # every test on every setup
        emu_harness.py --all --jobs 6         # the same, six runs at once
        emu_harness.py --all --jobs 6 --test card --test card-none
                                              # these tests on every setup
"""
import argparse
import concurrent.futures
import datetime
import json
import os
import re
import select
import shlex
import shutil
import signal
import socket
import subprocess
import sys
import tempfile
import time
from pathlib import Path

DEV = Path(__file__).resolve().parent
ROOT = DEV.parent.parent
FORK = DEV / "builds/emumd-cache/hatari-fork/build/src/hatari"
PLUGIN = ROOT / "build/md-testrom.mdfw"
TESTS = DEV / "emu_tests"
PROGRAM = ROOT / "app/atarist/dist/MDTEST.TOS"

# name: (Hatari machine, TOS image, ST RAM in KB, the machine line's name,
#        frames to boot to the program)
SETUPS = {
    "st-104": ("st", "emu/tos/tos104uk.img", 512, "ST", 1500),
    "ste-162": ("ste", "emu/tos/tos162uk.img", 1024, "STE", 2500),
    "mste-206": ("megaste", "emu/tos/tos206us.img", 1024, "Mega STE", 7500),
    "tt-306": ("tt", "emu/tos/tos306uk.img", 2048, "TT", 3000),
    "falcon-404": ("falcon", "emu/tos/tos404.img", 4096, "Falcon", 3000),
}

SAY = re.compile(rb"MDTEST: ([^\r\n]*)")

# Hatari warns of every bus and address error. TOS's start-up makes some on
# purpose, sizing memory and probing hardware from its ROM; any other is a
# crash.
CRASH_RE = re.compile(rb"WARN : (Bus|Address) Error \w+ at address \$([0-9a-f]+), PC=\$([0-9a-f]+)")
TOS_ROM = (0xE00000, 0x1000000)


class StepFailed(Exception):
    pass


def crashes(out_bytes):
    return [m.group(0).decode(errors="replace")[7:] for m in CRASH_RE.finditer(out_bytes)
            if not TOS_ROM[0] <= int(m.group(3), 16) < TOS_ROM[1]]


def stcmd_make(*make_args):
    if os.environ.get("EMU_HARNESS_PROGRAMS_BUILT"):
        return  # a --jobs run: the parent built every program first
    env = dict(os.environ, STCMD_IMAGE_TAG=os.environ.get("STCMD_IMAGE_TAG", "1.5.0"),
               STCMD_NO_TTY="1", STCMD_QUIET="1", ST_WORKING_FOLDER=str(ROOT))
    shutil.copy(ROOT / "version.txt", ROOT / "app/atarist/version.txt")
    subprocess.run(["stcmd", "make", "-C", "app/atarist", *make_args], env=env, check=True,
                   cwd=ROOT, stdout=subprocess.DEVNULL)


def build_program(layout=None, protocol=None, debug=False):
    """MDTEST.TOS, rebuilt when a source is newer; a variant (another window
    layout or protocol, the debug build) in its own folder."""
    if layout is None and protocol is None and not debug:
        stcmd_make()
        return PROGRAM
    name, flags, args = "", [], []
    if layout is not None:
        name += f"-layout{layout}"
        flags.append(f"-DPROGRAM_LAYOUT_VERSION={layout}")
    if protocol is not None:
        name += f"-protocol{protocol}"
        flags.append(f"-DPROGRAM_PROTOCOL_VERSION={protocol}")
    if debug:
        name += "-debug"
        args.append("DEBUG=1")
    stcmd_make(f"DIST=dist{name}", f"BUILD=build{name}", *args,
               f"EXTRA_CFLAGS={' '.join(flags)}")
    return ROOT / "app/atarist" / f"dist{name}" / "MDTEST.TOS"


def make_floppy(program, path, twice=False):
    folder = Path(tempfile.mkdtemp(prefix="mdtest-floppy-"))
    (folder / "AUTO").mkdir()
    shutil.copy(program, folder / "AUTO" / "MDTEST.PRG")
    if twice:
        # A second copy in the AUTO folder: quitting restarts the computer,
        # so the first runs again; the second never does.
        shutil.copy(program, folder / "AUTO" / "MDTEST2.PRG")
    subprocess.run([str(DEV / "make_floppy.sh"), str(folder), str(path)], check=True,
                   stdout=subprocess.DEVNULL)
    shutil.rmtree(folder)


def floppy_files(image, out):
    out.mkdir()
    subprocess.run(["mcopy", "-s", "-n", "-m", "-i", str(image), "::/*", str(out)],
                   env=dict(os.environ, MTOOLS_SKIP_CHECK="1"), capture_output=True)
    return sorted(p.name for p in out.iterdir())


class Hatari:
    """A running Hatari, its output (through a pseudo-terminal: through a file
    it arrives late) and its control socket."""

    def __init__(self, cmd, env, run_dir):
        self.out_path = run_dir / "hatari.out"
        self.shots = run_dir / "shots"
        self.sock_dir = tempfile.mkdtemp(prefix="eh", dir="/tmp")  # macOS: 104-byte paths
        sock_path = os.path.join(self.sock_dir, "ctl.sock")
        server = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
        server.bind(sock_path)
        server.listen(1)
        server.settimeout(30)
        master, slave = os.openpty()
        self.proc = subprocess.Popen(cmd + ["--control-socket", sock_path], env=env,
                                     cwd=run_dir, stdin=subprocess.DEVNULL,
                                     stdout=slave, stderr=slave)
        os.close(slave)
        self.master = master
        self.out = open(self.out_path, "wb")
        try:
            self.conn, _ = server.accept()
        except socket.timeout:
            self.stop()
            raise RuntimeError("Hatari never opened its control socket")
        finally:
            server.close()
        self.deadline = float("inf")

    def pump(self):
        while select.select([self.master], [], [], 0)[0]:
            try:
                data = os.read(self.master, 65536)
            except OSError:
                return
            if not data:
                return
            self.out.write(data)
        self.out.flush()

    def output(self):
        self.pump()
        return self.out_path.read_bytes()

    def send(self, text):
        # Hatari takes one command per read of its socket: two sent together
        # lose the second.
        self.conn.sendall((text + "\n").encode())
        time.sleep(0.05)

    def vbl(self, timeout=10):
        """The frame count, from the debugger's VBL variable."""
        if time.time() > self.deadline:
            raise StepFailed("the run's time limit (--timeout) is over")
        self.pump()
        mark = self.out_path.stat().st_size
        self.send("hatari-debug e VBL")
        end = time.time() + timeout
        while time.time() < end:
            time.sleep(0.05)
            self.pump()
            with open(self.out_path, "rb") as f:
                f.seek(mark)
                m = re.search(rb"#(\d+) \(dec\)", f.read())
            if m:
                return int(m.group(1))
            if self.proc.poll() is not None:
                raise StepFailed("Hatari stopped")
        raise StepFailed("no answer from Hatari's debugger")

    def shot(self, name):
        before = set(os.listdir(self.shots))
        self.send("hatari-shortcut screenshot")
        end = time.time() + 10
        while time.time() < end:
            self.pump()
            new = [f for f in os.listdir(self.shots) if f not in before]
            if new:
                size = -1
                while time.time() < end:  # complete: non-empty, no longer growing
                    now = (self.shots / new[0]).stat().st_size
                    if now and now == size:
                        os.replace(self.shots / new[0], self.shots / f"{name}.png")
                        return
                    size = now
                    time.sleep(0.2)
            time.sleep(0.1)
        raise StepFailed(f"no screenshot {name}")

    def stop(self):
        self.pump()
        # Quit through Hatari's own shortcut: it writes the floppy image
        # back only when it quits cleanly.
        if self.proc.poll() is None:
            try:
                self.send("hatari-shortcut quit")
                self.proc.wait(10)
            except (OSError, subprocess.TimeoutExpired):
                pass
        if self.proc.poll() is None:
            self.proc.send_signal(signal.SIGTERM)
            try:
                self.proc.wait(10)
            except subprocess.TimeoutExpired:
                self.proc.kill()
                self.proc.wait()
        self.pump()
        self.out.close()
        os.close(self.master)
        shutil.rmtree(self.sock_dir, ignore_errors=True)


class Run:
    def __init__(self, hatari, setup):
        self.hatari, self.setup = hatari, setup
        self.start = time.time()
        self.after = []  # floppy checks, run once Hatari has stopped

    def said(self):
        return [m.group(1).decode(errors="replace") for m in SAY.finditer(self.hatari.output())]

    def wait_frames(self, frames):
        target = self.hatari.vbl() + int(frames)
        while self.hatari.vbl() < target:
            time.sleep(0.05)

    def wait_for(self, frames, check, what):
        deadline = self.hatari.vbl() + int(frames)
        while True:
            found = check()
            if found:
                return found
            if self.hatari.vbl() > deadline:
                raise StepFailed(f"{what} not seen after {frames} frames")
            time.sleep(0.1)

    # -- steps --------------------------------------------------------------

    def step_wait(self, frames):
        self.wait_frames(frames)

    def step_key(self, key):
        self.hatari.send(f"hatari-event keypress {key}")
        self.wait_frames(2)

    def step_say(self, regex, frames):
        pattern = re.compile(regex)
        self.wait_for(frames, lambda: any(pattern.search(l) for l in self.said()),
                      f"the line /{regex}/")

    def step_sayn(self, count, regex, frames):
        pattern = re.compile(regex)
        self.wait_for(frames,
                      lambda: sum(bool(pattern.search(l)) for l in self.said()) >= int(count),
                      f"{count} lines /{regex}/")

    def step_nosay(self, regex):
        pattern = re.compile(regex)
        found = [l for l in self.said() if pattern.search(l)]
        if found:
            raise StepFailed(f"the program said {found[0]!r}")

    def step_machine(self):
        name = SETUPS[self.setup][3]
        self.step_say(rf"^Machine: {re.escape(name)}, 680\d0, TOS \d", 100)

    def step_log(self, regex, frames):
        pattern = re.compile(regex.encode())
        self.wait_for(frames, lambda: pattern.search(self.hatari.output()), f"log /{regex}/")

    def step_request(self, *words):
        # The bench's next request (MDTEST_REQUEST_*): the firmware reads the
        # file MDTEST_REQUEST_FILE names, then deletes it.
        path = self.hatari.out_path.parent / "request.txt"
        path.write_text(" ".join(words) + "\n")

    def step_select(self, ms="100"):
        # SELECT pressed for MS ms: the firmware reads the file
        # EMU_SELECT_FILE names, then deletes it (emu/standins.c).
        path = self.hatari.out_path.parent / "select.txt"
        path.write_text(f"{int(ms)}\n")

    def step_card(self, where):
        # The card taken out or put back: the stand-in card is out while the
        # file EMU_CARD_OUT_FILE names exists (emu/standins.c).
        if where not in ("out", "in"):
            raise StepFailed(f"card {where}: out or in")
        path = self.hatari.out_path.parent / "card-out"
        if where == "out":
            path.write_text("out\n")
        elif path.exists():
            path.unlink()

    def step_sdsame(self):
        self.after.append(("sdsame", None, None))

    def step_shot(self, name):
        self.hatari.shot(name)

    def step_floppy(self, name, regex):
        self.after.append(("floppy", name, regex))

    def step_nofloppy(self, name):
        self.after.append(("nofloppy", name, None))

    def step_floppynot(self, name, regex):
        self.after.append(("floppynot", name, regex))

    def step_sdfile(self, name, regex):
        self.after.append(("sdfile", name, regex))

    def step_sdnofile(self, name):
        self.after.append(("sdnofile", name, None))

    def step_sdfilenot(self, name, regex):
        self.after.append(("sdfilenot", name, regex))


def sd_listing(sd_dir):
    """Every file and folder under the card's folder, with its contents."""
    listing = {}
    for path in sorted(sd_dir.rglob("*")):
        rel = str(path.relative_to(sd_dir))
        listing[rel] = None if path.is_dir() else path.read_bytes()
    return listing


def check_floppy(checks, files_dir, sd_dir=None, sd_before=None):
    results = []
    for kind, name, regex in checks:
        if kind == "sdsame":
            after = sd_listing(sd_dir)
            changed = sorted(set(after) ^ set(sd_before) |
                             {k for k in after if k in sd_before and after[k] != sd_before[k]})
            results.append(("sdsame", not changed,
                            f"changed on the card: {', '.join(changed)}" if changed else None))
            continue
        on_card = kind.startswith("sd")
        where = "the card" if on_card else "the floppy"
        path = (sd_dir if on_card else files_dir) / name
        if kind in ("nofloppy", "sdnofile"):
            ok = not path.exists()
            detail = None if ok else f"{name} is on {where}"
        elif not path.exists():
            ok, detail = False, f"no {name} on {where}"
        else:
            text = path.read_bytes().decode("ascii", "replace")
            found = re.search(regex, text, re.M) is not None
            wanted = kind in ("floppy", "sdfile")
            ok = found if wanted else not found
            detail = None if ok else (f"{name} does not match /{regex}/" if wanted
                                      else f"{name} matches /{regex}/")
        results.append((f"{kind} {name}" + (f" {regex}" if regex else ""), ok, detail))
    return results


OPTIONS = ("hatari", "monitor", "memsize", "layout", "protect", "fault", "debug",
           "protocol", "drop", "bench", "twice", "chipfail", "cardfault", "sdapps",
           "wifi", "rssi", "btfail", "btnone", "wifinotsetup", "skip")


def parse_steps(name, lines, opts):
    """Steps, and "@option value" lines into opts."""
    steps = []
    for line in lines:
        line = line.strip()
        if line.startswith("@"):
            option, _, value = line[1:].partition(" ")
            if option not in OPTIONS:
                raise SystemExit(f"{name}: unknown option {line}")
            opts[option] = value.strip()
        elif line and not line.startswith("#"):
            steps.append(line)
    return steps


def load_test(name, opts):
    return parse_steps(f"{name}.steps", (TESTS / f"{name}.steps").read_text().splitlines(),
                       opts)


def hatari_command(setup, opts, run_dir, floppy):
    machine, tos, ram_kb, _, _ = SETUPS[setup]
    ram_kb = int(opts.get("memsize", ram_kb))
    cmd = [str(FORK), "--tos", str(ROOT / tos), "--machine", machine,
           "--memsize", str(0 if ram_kb < 1024 else ram_kb // 1024),
           "--sound", "off", "--statusbar", "off", "--drive-led", "off",
           "--harddrive", "none", "--drive-b", "off", "--natfeats", "yes",
           "--confirm-quit", "off",
           "--screenshot-dir", str(run_dir / "shots"), "--fast-forward", "off",
           "--disk-a", str(floppy), "--protect-floppy", opts.get("protect", "off")]
    if machine in ("tt", "falcon"):
        cmd += ["--cpuclock", "16" if machine == "falcon" else "32"]
    if opts.get("monitor"):
        cmd += ["--monitor", opts["monitor"]]
    if opts.get("hatari", "fork") == "fork":
        shutil.copytree(ROOT / "emu/sd", run_dir / "sd")
        (run_dir / "sd/USER.TXT").write_text("The user's own file.\n")
        if "sdapps" in opts:
            (run_dir / "sd/apps").mkdir()
        cmd += ["--md-sd", str(run_dir / "sd"), "--md-verbose", "on",
                "--md-firmware", str(PLUGIN)]
        if opts.get("wifi"):
            cmd += ["--md-option", f"wifi={opts['wifi']}"]
        if opts.get("rssi"):
            cmd += ["--md-option", f"wifi_rssi={opts['rssi']}"]
    return cmd


def run_one(setup, tests, extra_steps, disk, timeout):
    opts = {}
    steps = []
    for t in tests:
        steps += load_test(t, opts)
    steps += parse_steps("--step", extra_steps, opts)
    if not steps:
        raise SystemExit("no steps: --test or --step")
    name = "+".join(tests) or "steps"
    skip, _, why = opts.get("skip", "").partition(" -- ")
    if setup in skip.split():
        print(f"emu_harness: {setup} {name}: SKIP ({why})", flush=True)
        return "SKIP"
    stamp = datetime.datetime.now().strftime("%Y%m%d-%H%M%S")
    run_dir = DEV / "logs/emu" / f"{stamp}-{setup}-{name}"
    (run_dir / "shots").mkdir(parents=True)
    (run_dir / "home").mkdir()  # Hatari reads no saved configuration
    floppy = run_dir / "MDTEST.ST"
    if disk:
        shutil.copy(disk, floppy)
    else:
        layout = opts.get("layout")
        protocol = opts.get("protocol")
        program = build_program(int(layout) if layout else None,
                                int(protocol) if protocol else None,
                                "debug" in opts)
        make_floppy(program, floppy, "twice" in opts)
    cmd = hatari_command(setup, opts, run_dir, floppy)
    env = dict(os.environ, HOME=str(run_dir / "home"), SDL_VIDEODRIVER="dummy",
               SDL_AUDIODRIVER="dummy")
    if opts.get("fault"):
        env["MDTEST_FAULT"] = opts["fault"]
    if opts.get("drop"):
        env["MDTEST_DROP"] = opts["drop"]
    if "chipfail" in opts:
        env["MDTEST_CHIP_FAIL"] = "1"
    if opts.get("cardfault"):
        env["MDTEST_CARD_FAULT"] = opts["cardfault"]
    env["EMU_CARD_OUT_FILE"] = str(run_dir / "card-out")
    if "wifinotsetup" in opts:
        env["MDTEST_WIFI_NOT_SET_UP"] = "1"
    if opts.get("bench"):
        # The firmware asks the program for bench mode in the window's header.
        env["MDTEST_BENCH"] = opts["bench"].split()[0]
    if "btfail" in opts:
        env["EMU_BT_FAIL"] = "1"
    if "btnone" in opts:
        env["EMU_BT_NONE"] = "1"
    sd_before = sd_listing(run_dir / "sd") if (run_dir / "sd").exists() else {}
    env["MDTEST_REQUEST_FILE"] = str(run_dir / "request.txt")
    env["EMU_SELECT_FILE"] = str(run_dir / "select.txt")
    (run_dir / "command.txt").write_text(shlex.join(cmd) + "\n")
    print(f"emu_harness: {setup} {name}: {run_dir.relative_to(ROOT)}", flush=True)

    report = {"setup": setup, "tests": tests, "command": cmd, "steps": [], "result": "FAIL"}
    hatari = Hatari(cmd, env, run_dir)
    hatari.deadline = time.time() + timeout
    run = Run(hatari, setup)
    failed = False
    try:
        # The program in the AUTO folder: its title line first.
        # A monochrome monitor runs at 71 Hz, VGA at 60: as many seconds take
        # more frames.
        boot = SETUPS[setup][4] * (3 if opts.get("monitor") in ("mono", "vga") else 2) // 2
        steps = [f"say ^SidecarTridge {boot}"] + steps
        for step in steps:
            # "boot" stands for the setup's frames to boot (a restart).
            words = [str(boot) if w == "boot" else w for w in shlex.split(step)]
            entry = {"step": step}
            try:
                handler = getattr(run, "step_" + words[0], None)
                if handler is None:
                    raise StepFailed(f"unknown step {words[0]}")
                handler(*words[1:])
                entry["status"] = "PASS"
            except (StepFailed, TypeError, ValueError) as e:
                entry["status"] = "FAIL"
                entry["detail"] = str(e)
                failed = True
            report["steps"].append(entry)
            print(f"  {entry['status']}  {step}" +
                  (f"  ({entry['detail']})" if "detail" in entry else ""), flush=True)
            if failed:
                break
        try:
            hatari.shot("end")
        except StepFailed:
            pass
        (run_dir / "screen.txt").write_text("\n".join(run.said()) + "\n")
    finally:
        hatari.stop()
    report["floppy_files"] = floppy_files(floppy, run_dir / "floppy-files")
    if not failed:
        for step, ok, detail in check_floppy(run.after, run_dir / "floppy-files",
                                             run_dir / "sd", sd_before):
            entry = {"step": step, "status": "PASS" if ok else "FAIL"}
            if detail:
                entry["detail"] = detail
            report["steps"].append(entry)
            print(f"  {entry['status']}  {step}" + (f"  ({detail})" if detail else ""))
            failed = failed or not ok
    report["crashes"] = crashes(hatari.out_path.read_bytes())
    for c in report["crashes"]:
        print(f"  CRASH {c}")
    report["result"] = "PASS" if not failed and not report["crashes"] else "FAIL"
    report["seconds"] = round(time.time() - run.start, 1)
    (run_dir / "report.json").write_text(json.dumps(report, indent=2) + "\n")
    print(f"emu_harness: {report['result']} in {report['seconds']} s", flush=True)
    return report["result"]


def run_parallel(setups, tests, args):
    """--all with --jobs N: each run a child harness (its own run folder, Hatari
    and control socket), N at a time; each run's lines printed when it ends.
    The programs are built first, once, so no two runs build at once."""
    for t in [] if args.disk_a else tests:
        opts = {}
        load_test(t, opts)
        layout, protocol = opts.get("layout"), opts.get("protocol")
        build_program(int(layout) if layout else None,
                      int(protocol) if protocol else None, "debug" in opts)
    env = dict(os.environ, EMU_HARNESS_PROGRAMS_BUILT="1")
    pairs = [(s, t) for s in setups if (ROOT / SETUPS[s][1]).exists() for t in tests]

    def child(pair):
        setup, test = pair
        cmd = [sys.executable, str(Path(__file__).resolve()), "--no-build", "--setup",
               setup, "--test", test, "--timeout", str(args.timeout)]
        if args.disk_a:
            cmd += ["--disk-a", args.disk_a]
        done = subprocess.run(cmd, env=env, cwd=ROOT, capture_output=True, text=True)
        out = done.stdout + done.stderr
        status = "PASS" if done.returncode == 0 else "FAIL"
        if done.returncode == 0 and f"{setup} {test}: SKIP" in out:
            status = "SKIP"
        return setup, test, status, out

    start = time.time()
    results = []
    with concurrent.futures.ThreadPoolExecutor(args.jobs) as pool:
        for setup, test, status, out in pool.map(child, pairs):
            print(out, end="", flush=True)
            results.append((setup, test, status))
    print(f"\nemu_harness summary ({len(results)} runs, {args.jobs} at once, "
          f"{time.time() - start:.0f} s):")
    for setup, name, status in results:
        print(f"  {status}  {setup} {name}")
    return 0 if all(status != "FAIL" for _, _, status in results) else 1


def main():
    ap = argparse.ArgumentParser(description=__doc__.split("\n\n")[0],
                                 formatter_class=argparse.RawDescriptionHelpFormatter,
                                 epilog=__doc__.split("\n\n", 1)[1])
    ap.add_argument("--setup", action="append", choices=SETUPS)
    ap.add_argument("--test", action="append", default=[])
    ap.add_argument("--step", action="append", default=[])
    ap.add_argument("--all", action="store_true", help="every test on every setup")
    ap.add_argument("--disk-a", help="this floppy instead of one built with MDTEST.TOS")
    ap.add_argument("--no-build", action="store_true", help="use build/md-testrom.mdfw as it is")
    ap.add_argument("--timeout", type=float, default=300, help="host seconds a run")
    ap.add_argument("--jobs", type=int, default=1,
                    help="with --all: this many runs at once, each its own Hatari")
    args = ap.parse_args()

    if not args.no_build:
        subprocess.run([str(DEV / "mdfw.sh"), "build"], check=True, stdout=subprocess.DEVNULL)
    setups = args.setup or (list(SETUPS) if args.all else ["st-104"])
    tests = args.test or ([p.stem for p in sorted(TESTS.glob("*.steps"))] if args.all else [])
    results = []
    if args.all and args.jobs > 1:
        return run_parallel(setups, tests, args)
    for setup in setups:
        if not (ROOT / SETUPS[setup][1]).exists():
            print(f"emu_harness: SKIP {setup}: no {SETUPS[setup][1]}")
            continue
        if args.all:
            for t in tests:
                results.append((setup, t, run_one(setup, [t], [], args.disk_a, args.timeout)))
        else:
            results.append((setup, "+".join(tests) or "steps",
                            run_one(setup, tests, args.step, args.disk_a, args.timeout)))
    if len(results) > 1:
        print("\nemu_harness summary:")
        for setup, name, status in results:
            print(f"  {status}  {setup} {name}")
    return 0 if all(status != "FAIL" for _, _, status in results) else 1


if __name__ == "__main__":
    sys.exit(main())
