#!/usr/bin/env python3
"""Check what the SELECT button does, on the running RP, without a finger.

Each case presses SELECT through swd.py's GPIO input override, then reads the
RP's state over SWD and says whether it matches the behaviour of the
Multidevice Test firmware, which has no setup menu:

  bounce           15 ms press: ignored (the debounce is 30 ms); the firmware
                   keeps running
  during-test      short press while the SELECT test runs (a job, started
                   over SWD as job.py does): reported with its length, not
                   acted on; the firmware keeps running
  short            short press: the firmware leaves for Booster (the RP no
                   longer runs this ELF: the reset handler in the live vector
                   table is not this ELF's). This ends the probe session:
                   never probe Booster
  long             a press held past SELECT_LONG_RESET: factory reset (the
                   global settings are erased and the RP restarts into
                   Booster, which then clears every app's settings). Needs
                   --force and saves the settings flash first (put it back
                   with `restore`)

  status           print the state the cases read
  backup FILE      save the settings flash (app configs, lookup, global config)
  restore FILE     write a backup back and reset the RP

Usage: python3 tools/dev/select_harness.py <case> [FILE] [--elf ELF] [--force]
"""

import argparse
import os
import subprocess
import sys
import time

import swd

TIMERAWL = 0x40054028
FLASH_END = 0x10200000  # 2 MB flash on the SidecarTridge Multi-device
BOUNCE_MS = 15
SELECT_LONG_RESET_MS = 10000  # until the firmware's select.h defines it

SYMBOLS = ("_config_flash_start",)


def elf_word(elf: str, address: int) -> int:
    """A little-endian 32-bit word of the ELF's loaded image, read from the
    file (never from the device)."""
    out = subprocess.run(["arm-none-eabi-objdump", "-s",
                          f"--start-address=0x{address:x}",
                          f"--stop-address=0x{address + 4:x}", elf],
                         capture_output=True, text=True, check=True).stdout
    for line in out.splitlines():
        parts = line.split()
        if parts and parts[0] == f"{address:08x}"[-len(parts[0]):] and len(parts) > 1:
            return int.from_bytes(bytes.fromhex(parts[1]), "little")
    raise swd.SwdError(f"{elf} has no data at 0x{address:08x}")


class Harness:
    def __init__(self, elf: str | None):
        self.elf = swd.matching_elf(elf)
        self.sym = swd.elf_symbols(self.elf, *SYMBOLS, "ram_vector_table",
                                   "__vectors")
        missing = [s for s in SYMBOLS if s not in self.sym]
        if missing:
            raise swd.SwdError(f"{self.elf} lacks {', '.join(missing)}")
        table = self.sym.get("ram_vector_table") or self.sym.get("__vectors")
        if not table:
            raise swd.SwdError(f"{self.elf} has no vector table symbol")
        self.table = table[0]
        # Every SDK firmware, Booster included, installs its RAM vector table
        # at the same address, so VTOR alone cannot tell them apart. The
        # reset handler (the table's second word) can: each firmware's points
        # into its own code. The ELF's flash table holds this firmware's.
        flash_table = swd.elf_symbols(self.elf, "__vectors").get("__vectors")
        if not flash_table:
            raise swd.SwdError(f"{self.elf} has no __vectors symbol")
        self.reset_handler = elf_word(self.elf, flash_table[0] + 4)
        self.long_ms = swd.include_defines().get("SELECT_LONG_RESET",
                                                 SELECT_LONG_RESET_MS)

    def state(self) -> dict:
        return {
            "uptime_us": swd.read_word(TIMERAWL),
            "ours": (swd.read_word(swd.VTOR) == self.table and
                     swd.read_word(self.table + 4) == self.reset_handler),
        }

    def press(self, hold_ms: int, force: bool = False) -> None:
        kind = "long" if hold_ms >= self.long_ms else "short"
        swd.cmd_select(argparse.Namespace(press=kind, hold_ms=hold_ms,
                                          force=force))

    def wait_reset(self, before_us: int, timeout: float = 15.0) -> bool:
        """True once the RP has restarted (its microsecond timer went back)."""
        end = time.time() + timeout
        while time.time() < end:
            time.sleep(1.0)
            try:
                if swd.read_word(TIMERAWL) < before_us:
                    return True
            except swd.SwdError:
                pass  # the debug port drops while the chip restarts
        return False

    def wait_left(self, timeout: float = 15.0) -> bool:
        """True once the RP no longer runs this ELF (it jumped to Booster)."""
        end = time.time() + timeout
        while time.time() < end:
            try:
                if not self.state()["ours"]:
                    return True
            except swd.SwdError:
                pass
            time.sleep(0.5)
        return False


def show(label: str, st: dict) -> None:
    print(f"{label}: {'this firmware runs' if st['ours'] else 'another firmware runs (Booster?)'}; "
          f"uptime {st['uptime_us'] / 1e6:.1f} s")


def verdict(ok: bool, what: str) -> int:
    print(("PASS  " if ok else "FAIL  ") + what)
    return 0 if ok else 1


def case_bounce(h: Harness) -> int:
    before = h.state()
    show("before", before)
    h.press(BOUNCE_MS)
    time.sleep(1.5)
    after = h.state()
    show("after ", after)
    return verdict(after["uptime_us"] > before["uptime_us"] and after["ours"],
                   f"a {BOUNCE_MS} ms press is ignored")


def case_short(h: Harness) -> int:
    before = h.state()
    show("before", before)
    if not before["ours"]:
        raise swd.SwdError("the RP does not run this ELF")
    h.press(swd.SELECT_SHORT_MS)
    left = h.wait_left()
    try:
        show("after ", h.state())
    except swd.SwdError:
        pass
    return verdict(left, "a short press leaves for Booster (the probe session "
                   "ends here: confirm Booster on its page, then launch the "
                   "test microfirmware again)")


def case_during_test(h: Harness) -> int:
    import job
    before = h.state()
    show("before", before)
    if not before["ours"]:
        raise swd.SwdError("the RP does not run this ELF")
    job.app(h.elf, "SELECT", 1)
    try:
        seq = int(time.time()) & 0xFFFF
        a = job.command(h.elf, "JOB_START", job.TESTS["select"], seq, 10, 0, 0, 0)
        if a["status"] != "ok":
            raise swd.SwdError(f"start refused: {a['status']}")
        time.sleep(0.5)
        h.press(swd.SELECT_SHORT_MS)
        deadline = time.monotonic() + 15
        while True:
            time.sleep(0.3)
            a = job.command(h.elf, "JOB_STATUS")
            if job.JOB_STATES.get(a["data"][1]) != "running":
                break
            if time.monotonic() > deadline:
                job.command(h.elf, "JOB_CANCEL")
                raise swd.SwdError("the SELECT test is still running")
        text = job.result_text(h.elf, a["length"])
    finally:
        job.app(h.elf, "SELECT", 0)
    for line in text.splitlines():
        print(f"  {line}")
    time.sleep(1.0)
    after = h.state()
    show("after ", after)
    done = job.JOB_STATES.get(a["data"][1]) == "done" and "press_ms=" in text
    return verdict(done and after["ours"],
                   "a short press during the SELECT test is reported, not "
                   "acted on")


def settings_region(h: Harness) -> tuple[int, int]:
    start = h.sym["_config_flash_start"][0]
    return start, FLASH_END - start


def case_backup(h: Harness, path: str) -> int:
    start, length = settings_region(h)
    with open(path, "wb") as f:
        f.write(swd.read_memory(start, length))
    print(f"saved {length} bytes of settings flash from 0x{start:08x} to {path}")
    return 0


def case_restore(h: Harness, path: str) -> int:
    start, length = settings_region(h)
    if os.path.getsize(path) != length:
        raise swd.SwdError(f"{path} is not a {length}-byte settings backup")
    out = swd.openocd(*swd.quiesce_commands(),
                      f"flash write_image erase {path} 0x{start:08x} bin",
                      f"verify_image {path} 0x{start:08x} bin", check=False,
                      work_area=True)
    swd.chip_reset()
    ok = "verified" in out
    return verdict(ok, f"settings flash restored from {path} and RP reset")


def case_long(h: Harness, force: bool, backup: str) -> int:
    if not force:
        raise swd.SwdError("a long press is a factory reset: add --force "
                           "(the settings flash is backed up first)")
    case_backup(h, backup)
    before = h.state()
    show("before", before)
    h.press(h.long_ms + 1000, force=True)
    reset = h.wait_reset(before["uptime_us"], timeout=20.0)
    print(f"      restore with: python3 tools/dev/select_harness.py restore "
          f"{backup}")
    return verdict(reset, f"a press held {h.long_ms / 1000:.0f} s factory-resets "
                   "and reboots the RP")


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument("case", choices=["status", "bounce", "during-test", "short",
                                     "long", "backup", "restore"])
    ap.add_argument("file", nargs="?", help="backup file (backup/restore/long)")
    ap.add_argument("--elf")
    ap.add_argument("--force", action="store_true")
    args = ap.parse_args()
    try:
        h = Harness(args.elf)
        if args.case == "status":
            show("now", h.state())
            return 0
        if args.case in ("backup", "restore"):
            if not args.file:
                raise swd.SwdError(f"{args.case} needs a FILE")
            return (case_backup if args.case == "backup"
                    else case_restore)(h, args.file)
        if args.case == "bounce":
            return case_bounce(h)
        if args.case == "during-test":
            return case_during_test(h)
        if args.case == "short":
            return case_short(h)
        return case_long(h, args.force, args.file or os.path.join(
            swd.HERE, "logs", time.strftime("settings-%Y%m%d-%H%M%S.bin")))
    except swd.SwdError as e:
        print(f"error: {e}", file=sys.stderr)
        return 2


if __name__ == "__main__":
    sys.exit(main())
