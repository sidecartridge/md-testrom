#!/usr/bin/env python3
"""Run a test of the firmware's as a job over SWD, with no ST attached.

The probe selects ROM3 (a debug-only hook), injects the job's commands
through the debug mailbox, reads the answers the firmware writes into the
window (the answer record at $FA2100, the result at $FA4000), and gives ROM3
back. Injected commands are never answered through the token: the ST did not
send them.

    python3 tools/dev/job.py run dummy 2                   # its result
    python3 tools/dev/job.py run dummy 2 --expect seconds=2 --json out.json
    python3 tools/dev/job.py status

`run` exits 1 when the job does not end done or an --expect does not match,
and writes a JSON report (tools/dev/logs/job-TEST.json by default). Needs a
debug build running and tools/dev/swd.py's set-up (OPENOCD); the RP can be
powered over USB alone.
"""
import argparse
import json
import os
import struct
import sys
import time

DEV = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, DEV)
import mdtest_window  # noqa: E402
import swd  # noqa: E402

L = mdtest_window.layout()
JOB_STATES = {L[k]: k[4:].lower() for k in L if k.startswith("JOB_")}
STATUSES = {L[k]: k[7:].lower() for k in L if k.startswith("STATUS_")}
TESTS = {k[5:].lower(): L[k] for k in L if k.startswith("TEST_")}
JOB_TIMEOUT = 120  # seconds


def app(elf, name, *words):
    defs = swd.include_defines()
    return swd.mailbox_request(elf, swd.KIND_APP, defs["DEVHOOKS_APP_" + name],
                               list(words))


def window_base(elf):
    return swd.elf_symbols(elf, "__rom_in_ram_start__")["__rom_in_ram_start__"][0]


def answer(elf):
    """The answer record: the words the ST would read at $FA2100."""
    base = window_base(elf) + 2 * L["ANSWER_INDEX"]
    words = struct.unpack(f"<{L['ANSWER_WORDS']}H",
                          swd.read_memory(base, 2 * L["ANSWER_WORDS"]))
    data = list(words[L["ANS_DATA"]:])
    return {"command": words[L["ANS_COMMAND"]],
            "status": STATUSES.get(words[L["ANS_STATUS"]], words[L["ANS_STATUS"]]),
            "boot": (words[L["ANS_BOOT"]] << 16) | words[L["ANS_BOOT"] + 1],
            "count": words[L["ANS_COUNT"]], "length": words[L["ANS_LENGTH"]],
            "data": data}


def result_text(elf, length):
    """The result buffer's text: two characters a word, the first in the
    high byte, as the ST reads it."""
    base = window_base(elf) + 2 * L["RESULT_BUFFER_INDEX"]
    raw = swd.read_memory(base, (length + 1) & ~1)
    st = b"".join(raw[i + 1:i + 2] + raw[i:i + 1] for i in range(0, len(raw), 2))
    return st[:length].decode("ascii", errors="replace")


def command(elf, name, *words):
    """Inject a command and return the answer it wrote."""
    before = answer(elf)["count"]
    swd.send_protocol(elf, L["CMD_" + name], list(words))
    deadline = time.monotonic() + 5
    while time.monotonic() < deadline:
        a = answer(elf)
        if a["count"] != before and a["command"] == L["CMD_" + name]:
            return a
        time.sleep(0.05)
    raise swd.SwdError(f"no answer to {name}")


def status_line(a):
    number, state, progress, test = a["data"][:4]
    return (f"job {number}: {JOB_STATES.get(state, state)}, {progress}%, test "
            f"{test}, result {a['length']} bytes")


def run(elf, test, params, expects, report_path):
    report = {"test": test, "params": params, "expect": expects, "result": "FAIL"}
    app(elf, "SELECT", 1)
    try:
        seq = int(time.time()) & 0xFFFF
        a = command(elf, "JOB_START", TESTS[test], seq, *(params + [0] * 4)[:4])
        if a["status"] != "ok":
            raise swd.SwdError(f"start refused: {a['status']}")
        deadline = time.monotonic() + JOB_TIMEOUT
        while True:
            time.sleep(0.5)
            a = command(elf, "JOB_STATUS")
            if JOB_STATES.get(a["data"][1]) != "running":
                break
            if time.monotonic() > deadline:
                command(elf, "JOB_CANCEL")
                raise swd.SwdError(f"still running after {JOB_TIMEOUT} s")
        text = result_text(elf, a["length"])
    finally:
        app(elf, "SELECT", 0)
    lines = dict(l.split("=", 1) for l in text.splitlines() if "=" in l)
    report.update(state=JOB_STATES.get(a["data"][1]), lines=lines)
    print(status_line(a))
    for line in text.splitlines():
        print(f"  {line}")
    ok = report["state"] == "done"
    for e in expects:
        key, _, value = e.partition("=")
        if lines.get(key) != value:
            print(f"FAIL  {key}: {lines.get(key)!r}, expected {value!r}")
            ok = False
    report["result"] = "PASS" if ok else "FAIL"
    os.makedirs(os.path.dirname(report_path), exist_ok=True)
    with open(report_path, "w") as f:
        json.dump(report, f, indent=2)
    print(f"{report['result']}  {test} ({report_path})")
    return 0 if ok else 1


def main():
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument("--elf")
    sub = ap.add_subparsers(dest="cmd", required=True)
    r = sub.add_parser("run", help="run a test to its result")
    r.add_argument("test", choices=sorted(TESTS))
    r.add_argument("params", nargs="*", type=lambda v: int(v, 0))
    r.add_argument("--expect", action="append", default=[], metavar="KEY=VALUE")
    r.add_argument("--json")
    sub.add_parser("status", help="the last answer the firmware wrote")
    args = ap.parse_args()
    elf = swd.matching_elf(args.elf)
    if args.cmd == "status":
        a = answer(elf)
        print(json.dumps(a))
        return 0
    path = args.json or os.path.join(DEV, "logs", f"job-{args.test}.json")
    with swd.held_openocd():
        return run(elf, args.test, args.params, args.expect, path)


if __name__ == "__main__":
    try:
        sys.exit(main())
    except swd.SwdError as e:
        sys.exit(f"job: {e}")
