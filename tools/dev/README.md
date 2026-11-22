# Developer tools

Host-side tools for the Multidevice Test microfirmware. Two benches:

- **EmuMD** (`emu/emumd`, see below): the firmware's own C code built for this computer and run
  in EmuMD's Hatari, with a Multi-device on the cartridge port. Use it first, and for everything
  it can show: logic, commands, the ST program, the card (a folder), Wi-Fi (this computer's
  network), on every machine and TOS Hatari emulates.
- **The device**: a SidecarTridge Multi-device on an Atari, with a Raspberry Pi Debug Probe wired
  to the RP2040's SWD pins and to its debug UART (GPIO 0/1). Only for what EmuMD does not
  emulate: the bus and its timing, the real card and radios, SELECT, the LED, flash and memory
  numbers.

Python tools use the standard library only.

## In this repository

These tools come from the sibling repositories that built and verified them, and are kept as close
to their source as this firmware allows, so a fix travels both ways with a plain diff:

| File | Source | Commit |
| --- | --- | --- |
| `console.py`, `flash.sh`, `swd.py`, `stackdepth.py`, `measure_builds.sh`, `tools_harness.py`, `select_harness.py`, `gdb/`, this README's sections from "Firmware support" on | md-rom-emulator (the template's tools, adapted there to a 128 KB two-bank window) | `3cd573d` |
| `build_hatari_fork.sh`, `hatari_fork.cmake`, `mdfw.sh`, `emu_harness.py` (rewritten since) | md-printer-emulator | `e81bcbc` |
| `make_floppy.sh` | md-printer-emulator | `687affb` |
| `emu_check.py`, `bus_faults.py`, `job.py`, `st_loop.py`, `mdtest_window.py` | this repository | |
| `make_floppy_image.py` | md-drives-emulator | `002fbc8` |
| `../../rp/src/build_id.cmake` | md-rom-emulator | `3cd573d` |

Not imported, because they drive things this firmware does not have: `smoke.py`, `st_harness.py`
and `sttest.s` (a setup menu and cartridge code), `testserver.py`, `make_catalog.py`,
`download_harness.py` and `fill_card.py` (a ROM catalog and downloads), `selfcheck/` (a
cartridge image that runs its own check), `make_rom_images.py` and `hatari_check.py` (ROM
images and the ROM Emulator's Hatari runs).

What differs here:

- **The firmware is headless.** It draws nothing and puts no code on the ST, so `swd.py screen`,
  `text`, `key`, `type` and `st-reset` say they do not apply and exit 1.
- **The command channel runs once the program selects ROM3.** `swd.py counters` and `ring` show
  what the capture and the parser made of the ST's ROM3 reads; `inject` and `app` (`heap_hold`,
  `health`, and this app's `fault`, `drop`, `select`, `restart`, `chip_fail`, `card_fault`,
  `wifi_not_set_up`, `bench`) work on debug builds, and
  injected commands answer only while ROM3 is selected (`app select 1`). `app restart` is an RP
  restart as the ST sees it (ROM3 test data again, the job gone, a new boot ID) with no reset:
  `swd.py reset` while a program reads the cartridge crashes the ST, as a flash does. `swd.py log` shows the program's screen
  as it mirrors it to the RP, `swd.py request` asks the program in bench mode to do something.
- **`swd.py window FILE`** checks only the cartridge window by default (this firmware does not use
  `ROM_TEMP`; `--flash` checks it too), and says how many bytes differ.
- **`select_harness.py`**: the firmware has no setup menu. "Running" means the RP's VTOR holds
  this ELF's vector table; a short press must make the RP leave for Booster, which ends the probe
  session.
- **No `power_cycles.py`**: the sibling's tool asked for the ST to be power-cycled, which wears a
  machine this old; a restart of the RP alone is `swd.py reset` (the ST at the desktop) or, as the
  ST sees it, `swd.py app restart`. A short SELECT press leaves for Booster.
- **`flash.sh`** is the sibling's, without its HTTPS profile: pins pico-sdk 2.2.0, pico-extras
  sdk-2.2.0 and fatfs-sdk v3.6.2, CMake Release for both types.
- **OpenOCD**: set `OPENOCD` to the Raspberry Pi fork's build
  (`../pico/openocd/src/openocd`). An older `openocd` earlier on `PATH` fails with
  `flash driver 'rp2xxx' not found`.

## The device: a session

1. Diego launches the test microfirmware from Booster (it sets the development UUID as the
   current app; a firmware flashed while Booster's settings name another app jumps straight to
   Booster).
2. `python3 tools/dev/console.py watch` in the background.
3. `tools/dev/flash.sh debug --probe` (or `release`): builds, flashes, verifies, reads the build ID.
   Only with the ST idle at the desktop: a flash while a program reads the cartridge crashes the
   ST (four bombs), even with the bus buffers forced inactive first.
4. `python3 tools/dev/swd.py window --pattern`, `heap`, `stack paint|peak`,
   `crash`.
5. A step that ends in Booster (`select_harness.py short`) ends the session. Never probe Booster.

## EmuMD

```sh
git submodule update --init emu/emumd
tools/dev/build_hatari_fork.sh                    # EmuMD's Hatari, once (a few minutes)
mkdir -p emu/tos emu/sd                           # TOS images linked here; both gitignored
```

The Hatari build and EmuTOS live in `tools/dev/builds/emumd-cache/`, not `~/.cache/emumd`, which
other projects share at other EmuMD versions. `tools/dev/mdfw.sh` is EmuMD's `mdfw` with that
cache. What EmuMD does and does not stand in for, for this firmware:

- The window is 128 KB, as on the device; ROM4 is served from its first half.
- An ST read of ROM3 reaches the firmware as a command-ring sample and returns no data: the ROM3
  half of the read tests can only be checked on the device.
- PIO, DMA and timing are not emulated. SELECT is a stand-in held for a set time (`select MS` in
  the harness's steps), the LED is EmuMD's radio-chip stand-in (`[wifi]` in `mdfw.ini`), the
  microSD card is the run's `sd` folder with a stand-in for the card itself (`card out|in`,
  `@cardfault N`), Wi-Fi goes through this computer's network, and the Bluetooth controller is a
  stand-in (`emu/standins.c`: three devices, `@btfail`, `@btnone`).
- Never give this Hatari a GEMDOS drive (`--harddrive`): it takes the first 1 KB of the
  cartridge. Programs travel on floppy images (`make_floppy.sh`).

The firmware itself (`mdfw.ini`, `emu/`; `emu/README.md` has the details):

```sh
python3 tools/dev/emu_check.py                    # build, boot TOS 1.04 / 1.62 / 2.06, PASS/FAIL
tools/dev/make_floppy.sh FOLDER disk.st           # a 720 KB floppy; its AUTO\ program runs at boot
python3 tools/dev/emu_check.py --disk-a disk.st --frames 3000 st-104
```

`emu_check.py` passes a setup when the firmware's log shows the test image verified in the
window and the main loop running, and keeps the log and a screenshot of the last frame in
`tools/dev/logs/emu/<setup>/`. `emu_harness.py` (rewritten from md-printer-emulator's for this program) runs
`MDTEST.TOS` from a floppy's AUTO folder against the firmware, or with nothing in the port
(`@hatari bare`), on the ST (TOS 1.04, 512 KB), STE, Mega STE, TT and Falcon setups. The program
prints every line it shows through NatFeats (`MDTEST: <line>` in `hatari.out`); the steps in
`tools/dev/emu_tests/*.steps` press keys, wait for lines, and check the files it saves on the
floppy once Hatari has stopped. `--all` runs every test on every setup, and `--all --jobs 6` six runs at once, each its own Hatari (the whole set in minutes instead of hours; `--test` and `--setup` narrow it). A change runs its own tests on every setup and the rest on the ST (`--all --jobs 6 --setup st-104`); the whole set on every setup before a release. `@fault KIND A B` in a
test serves the window as that faulty bus would (`MDTEST_FAULT`), to check the line test names it.
`@debug` builds the program's debug variant (EmuMD's ROM3 cannot pass the line test, so the
command channel's tests override the gate), `@drop N` loses every Nth answer (`MDTEST_DROP`),
`@protocol N` builds a program that speaks protocol N, `@bench MODE` has the firmware ask the
program for bench mode in the window's header (`MDTEST_BENCH`: `line`, `requests` or `commands`),
`@twice` puts the program in the AUTO folder twice (quitting restarts the computer, so the first
runs again), `@chipfail` makes the radio chip fail to start (the LED test's third verdict), the step
`request CODE ...` hands the program a bench request (`request 255`: the firmware restarts as the
ST sees it, `app restart`), and `select MS` holds SELECT down for MS ms. `@cardfault N` makes
the microSD card fail (`MDTEST_CARD_FAULT`: 1 no answer, 2 full, 3 a write fails, 4 a byte read
back differs), `@sdapps` puts Booster's apps folder on it, `card out` and `card in` take it out and
put it back, and `sdsame` checks at the end that the card's folder (a `USER.TXT` of the user's,
always) holds what it held before the run. `@wifi HOW` makes EmuMD's Wi-Fi fail (`badauth`,
`nonet`, `fail`, `off`), `@rssi DBM` sets its signal, `@btfail` and `@btnone` make the Bluetooth stand-in's controller
fail or its scan hear nothing, and `floppynot NAME REGEX` checks a saved
file does not hold something. A report the program saves is on the card: `sdfile NAME REGEX`,
`sdfilenot NAME REGEX` and `sdnofile NAME` check the run's card folder (`MDTEST-REPORT-00.TXT`).

The command channel, with no ST: `job.py run dummy 2 --expect seconds=2` selects ROM3 through
the debug hook, runs a job by injected commands, reads its result from the window and gives ROM3
back (`tools_harness.py --channel` adds hello, echo and the window afterwards). With the ST in
bench mode (`swd.py app bench 2` on a debug firmware, then start the program: the line test, then
the RP's requests): `st_loop.py run dummy 2 --expect seconds=2 --times
10`, or the same with `--emu st-104` under EmuMD.

The device tests run the same way: `job.py run device_info`, `job.py run select 10` (press
SELECT within 10 s), `job.py run led 1` (on; 0 off, 2 blink, 3 off and the chip powered down),
`job.py run sd_card` (the card described, its file written, read back and deleted), `job.py run
sd_swap 30` (take the card out and put it back within 30 s each); `swd.py app card_fault N` makes
the card fail as `@cardfault N` does. `job.py run wifi` runs the Wi-Fi test to the web server;
`swd.py app chip_fail 1` makes it stop at the chip. `swd.py app wifi_not_set_up 1` makes it read
Booster's Wi-Fi as not set up to join (a warning after the scan), as `@wifinotsetup` does.
`job.py run bluetooth` starts the controller and scans (about 14 s); `chip_fail` stops it at the
chip too.

On the device, `swd.py verdict` reads the last line-test verdict the ST sent, and
`bus_faults.py` serves each fault in turn (`swd.py app fault`) while `MDTEST.TOS` runs in bench
mode (`swd.py app bench 1` before starting it: the line test in a loop), and compares the verdict's
class with the one expected. The program never returns to the desktop: a key ends the bench loop
and restarts the computer.
Flash only with the ST at the desktop, never during the bench loop.

The sections below are md-rom-emulator's, kept for each tool's options. Where they mention a
setup menu, a ROM, a catalog or v2.1.2, read them with the differences above.

## Firmware support these tools rely on

- Debug builds run the console at 921,600 baud (`PICO_DEFAULT_UART_BAUD_RATE` in
  `rp/src/CMakeLists.txt`); release builds have no console at all.
- `rp.elf` keeps its symbol table (the link does not strip it): most commands find their addresses
  by symbol. The symbols never reach the `.uf2`.
- Every build carries its build ID in flash as `release_build_id` (`rp/src/build_id.cmake`).
- Debug builds carry the devhooks mailbox (`rp/src/include/devhooks.h`, included once from
  `emul.c`, served by `devhooks_poll()` in the main loop), which `key`, `app` and `inject` write.

## Debug console: `console.py`

Captures the debug console of a `debug` build (921,600 baud) to `tools/dev/logs/console.log`, with
a timestamp on every line, and shows it in the terminal. Use it instead of a serial terminal such
as CoolTerm: only one program can open the port.

```bash
python3 tools/dev/console.py watch          # leave running in a terminal
```

`watch` finds the Debug Probe by its USB name (`--port` to choose another device), waits for it when
it is unplugged, and reopens it when it returns. While it runs, other commands read the log:

```bash
python3 tools/dev/console.py since-boot                     # everything since the last boot
python3 tools/dev/console.py since-boot --boot 2            # the boot before that
python3 tools/dev/console.py tail 100
python3 tools/dev/console.py grep 'Checksum error' --since-boot
python3 tools/dev/console.py wait 'Start the app loop' --timeout 30
```

`grep` and `wait` take Python regular expressions and exit with 3 when nothing matches. `wait` only
matches lines that arrive after it starts, so start it before the action that should print the
line. The log rotates to `console.log.1` at 32 MB.

The settings dump can print bytes that make macOS `grep` treat the log as binary and print
nothing; use `console.py grep` or `grep -a`.

## Build, flash and verify: `flash.sh`

```bash
tools/dev/flash.sh debug                  # build, flash with picotool, check over SWD
tools/dev/flash.sh release --probe        # flash through the Debug Probe instead
tools/dev/flash.sh debug --build-only     # build only
tools/dev/flash.sh debug --src /tmp/src   # build a copy of rp/src (for example a patched linker script)
```

Builds out of tree in `tools/dev/builds/<type>`, incrementally. It does not touch `rp/build` or
the submodules, and warns when a submodule is not at the version `rp/build.sh` pins. It builds
with the same CMake build type as `rp/build.sh` (Release; `RP_CMAKE_BUILD_TYPE` overrides it,
with a warning, in a folder of its own). The window's
contents come from `common/mdtest.c`, compiled in: nothing to regenerate.

Every build carries a build ID: the git commit, `<sha7>`, or `<sha7>-dirty.<diff7>` when the tree
has uncommitted changes, followed by `+debug` in a debug build, so a debug and a release build of one
tree never share an ID. The same tree always gives the same ID, and at the same checkout path a
byte-identical binary (release builds embed source paths, so another path gives other bytes). The
ID is stored in flash as the `release_build_id` string, and `rp.elf` is kept as
`tools/dev/builds/elf/<type>-<id>.elf` for resolving crash addresses later.

Flashing uses `picotool load -f -x`, which reboots the running firmware into BOOTSEL over USB; when
picotool cannot see the RP it falls back to the Debug Probe. Then `flash.sh` checks the result over
SWD with `swd.py`: the RP booted the ELF, its flash matches the ELF byte for byte, and it carries
the new build ID. On failure it exits with 1 and prints the console since the last boot.

## Debug probe: `swd.py`

The tools talk to the RP only through picotool, the Debug Probe and the console UART, never through
the firmware's own services, so they work with any microfirmware built from this template and with
a hung RP. Memory is read while the CPU keeps running.

```bash
python3 tools/dev/swd.py running tools/dev/builds/debug/rp.elf   # booted this firmware?
python3 tools/dev/swd.py verify tools/dev/builds/debug/rp.elf    # flash identical to the ELF?
python3 tools/dev/swd.py build-id                                # which build is on the RP?
python3 tools/dev/swd.py read 0x20028000 8000 fb.bin             # dump memory
python3 tools/dev/swd.py program tools/dev/builds/debug/rp.elf   # flash through the probe
python3 tools/dev/swd.py shared                                  # token + shared variables
python3 tools/dev/swd.py resume                                  # release cores a debugger left halted
python3 tools/dev/swd.py reset                                   # reset the whole chip, watchdog-style
python3 tools/dev/swd.py select short                            # press SELECT (short press)
python3 tools/dev/select_harness.py bounce                       # 15 ms press: ignored
python3 tools/dev/select_harness.py during-test                  # press in the SELECT test: reported
python3 tools/dev/select_harness.py short                        # short press: leaves for Booster
python3 tools/dev/select_harness.py backup settings.bin          # save the settings flash
python3 tools/dev/select_harness.py long --force                 # 10 s press: factory reset
python3 tools/dev/select_harness.py restore settings.bin         # put the settings back
python3 tools/dev/swd.py app heap_hold 16                        # hold 16 KB more heap (0 releases)
python3 tools/dev/swd.py inject 0x0001 0x0067 0                  # any protocol command
python3 tools/dev/swd.py crash                                   # why did it last reboot? (health.c's record)
python3 tools/dev/swd.py app health 3                            # provoke a hang (1 panic, 2 fault, 4 stall, 5 overflow)
python3 tools/dev/swd.py postmortem                              # halt, backtraces, resume
python3 tools/dev/swd.py heap                                    # heap size, peak, free space
python3 tools/dev/swd.py counters                                # command channel counters, no halt
python3 tools/dev/swd.py ring                                    # the ST's commands, decoded, no halt
python3 tools/dev/swd.py ring --mark                             # ...then, after a test:
python3 tools/dev/swd.py ring --since-mark                       # only what the ST sent since the mark
python3 tools/dev/swd.py heap --watch 5 --csv tools/dev/logs/heap.csv   # sample during a test
```

`screen` and `text` are the template's (its framebuffer and terminal), which this headless
firmware does not have: they say so and exit 1. `shared` prints the command sentinel, the random token, the token seed and the shared variables, named
after the `*_SVAR_*` / `*_SHARED_VARIABLE_*` indexes in `rp/src/include` (`chandler.h` names the
first three slots). They take the
window address from the ELF; without `--elf` they use the cached ELF whose build ID the RP
carries, so flash the build with `flash.sh` first.

`program` halts both cores and stops every PIO state machine and DMA channel before it writes:
halting the cores does not stop the RP2040's DMA, and while the ST touches the cartridge the ROM3
capture ring keeps writing bus samples into RAM, where the flash write stages its data (it has
programmed bus samples into an image). Flash through the probe with this tool, not with OpenOCD's
own `program`.

`program` and `reset` restart the chip through the watchdog (PSM `WDSEL` + `WATCHDOG_CTRL.TRIGGER`),
never with OpenOCD's `reset`. A firmware that launches core 1 early (as the SELECT watcher this
template used to ship, `select_coreWaitPush()`, would have) meets OpenOCD's multi-core reset
sequence touching core 1 again just after that: core 1 died in the middle of its first trace holding
the SDK's stdio mutex, and every piece of debug output then waited out the 1 s
`PICO_STDIO_DEADLOCK_TIMEOUT_MS` (a debug boot of 220 s instead of 0.7 s, and a dead SELECT button).
A watchdog-style reset is the one that leaves the chip exactly as a power-on reset does, with DMA
and PIO stopped. If an old build shows that symptom, run `swd.py reset` or power-cycle. The same
applies to a VS Code debug session's restart button.

OpenOCD loads small routines into a RAM work area (`verify_image`'s CRC, the flash-size probe of a GDB
connect, when GDB still asked for a memory map). `rp2040.cfg` puts it at `0x20010000`, inside this firmware's RAM, where it once overwrote
the Wi-Fi driver's async context and the next `cyw43_arch_poll()` HardFaulted. `swd.py` gives every
run that does not write flash a 4 KB work area in `SCRATCH_X` instead, backed up and restored: that
is core 1's stack, and this firmware never starts core 1. A firmware that does must move it. Flash
writes keep the default, with the cores halted and a reset after. GDB's connect no longer probes
the flash (see `gdb` above); when it did, a connect during boot, while boot2 sets up XIP, also left
flash unreadable and the firmware executing zeros until the next reset.

A halted RP can still be read. Halting core 1 also pauses the RP2040's timer, so after a debugger
halt run `resume`, which releases both cores; OpenOCD's own `resume` fails in a new OpenOCD run.

`select` needs no firmware code: it forces the SELECT pin's input high through the RP2040's GPIO
input override for 300 ms (`short`) or `SELECT_LONG_RESET` + 1 s (`long`). A long press needs
`--force`, because in a microfirmware that wires SELECT the usual way it is a factory reset: it
erases the global settings, and Booster then clears every app's settings. Here a short press
leaves for Booster (during the SELECT test, and for 500 ms after, it is the test's). `select release` clears an
override left behind. The press is held inside one OpenOCD session, so nothing else can use the
probe until it ends: to look at the device during a press, put the reads in that same session
(`mww` the override, `sleep`, `mdw`/`mdb` what you want, `mww` it back).

`app` and `inject` need a `debug` build. They write a small mailbox in RAM
(`rp/src/include/devhooks.h`, found by its `devhooksMailbox` symbol) and wait for the main loop to
acknowledge it. `inject` queues a protocol command as if the ST had sent it, through
`chandler_injectProtocol()`, so the firmware handles it through its normal path: the same
callbacks, the same answer; it is answered only while ROM3 is selected (`app select 1`). `app NAME`
runs the app command defined as `DEVHOOKS_APP_<NAME>` in `rp/src/include/emul.h`, handled by
`emul_devhooksApp()`:

- `heap_hold KB`: hold KB more kilobytes of heap, on top of what is already held (`heap_hold 0`
  releases everything). Result 0 when the allocation is refused, so repeated calls walk the heap
  down to a known remainder. Watch it with `swd.py heap`.
- `health N`: provoke a failure from the main loop 250 ms later (`health.h`'s `health_test_t`): 1
  `panic()`, 2 a HardFault, 3 a hang (the watchdog), 4 a 500 ms stall (no reboot), 5 a stack
  overflow into the guard. Each reboots and names itself in `swd.py crash`, on the console and in
  the program's device information; three within 60 s are a restart loop, which device
  information warns about. `healthMaxFeedGapUs` and `healthMaxFeedGapPhase` hold the longest gap between watchdog feeds.
- `fault KIND A B`: refill the window, then serve it as a faulty bus would (`MDTEST_FAULT_*`,
  `common/mdtest.h`); `fault 0` serves it whole again. `tools/dev/bus_faults.py` runs each.
- `drop N`: every Nth command runs unanswered, as if its answer were lost (0: none).
- `select 0|1`: ROM3 given back or selected with no ST, for injected commands.
- `restart`: an RP restart as the ST sees it (ROM3 test data again, the job gone, a new boot ID),
  with no reset.
- `chip_fail 0|1`: the radio chip fails to start, and the LED with it, as a dead radio would.
- `card_fault N`: the microSD card fails as `CARDTEST_FAULT_N` (`cardtest.h`): 1 no card answers,
  2 no free space, 3 a write fails, 4 a byte read back differs; 0 puts it back.
- `wifi_not_set_up 0|1`: the Wi-Fi test reads Booster's Wi-Fi as not set up to join a network.
- `bench N`: the program starts in bench mode N (`MDTEST_BENCH_*`: 1 the line test in a loop, 2
  then the RP's requests, 3 the requests at once with a debug program); 0 puts it back.

Add an app's own commands the same way: a `DEVHOOKS_APP_<NAME>` define and a case in the handler.
Useful ones in other microfirmwares: stop a boot countdown; stall or fail the next answer on
purpose, to exercise the ST's retry path.

`ring` decodes what the ST sent from the ROM3 capture ring (`commemul.c`) without halting the RP,
on a release build as on a debug one. It holds the last 2,048 bus samples: about 200 small
commands (6 to 10 samples each), or 3 report chunks of 1,800 bytes. Each frame shows its sample
number since the RP booted, the command (named from chandler's framework commands and
`common/mdtest.h`'s `MDTEST_CMD_*`; others print as hex), its payload size, the random token, the first four 32-bit
parameters in the order `TPROTO_GET_PAYLOAD_PARAM32` reads them, and a mark on a bad checksum.
The oldest frame is often cut by the ring's start and shows as a bad checksum. A frame whose size
is past `MAX_PROTOCOL_PAYLOAD_SIZE` shows as dropped, as the RP drops it.
The DMA keeps writing while the ring is read, so `ring` reads its write position before and
after and drops the samples it may have overwritten meanwhile. During a sustained burst of large
frames the ST sends about 200,000 samples a second and refills the ring in about 10 ms, faster
than SWD copies it: `ring` then says so instead of printing half-overwritten frames, and reads
normally once the burst is over. `--mark` stores the position in
`tools/dev/logs/ring.mark` and `--since-mark` shows only what came after it, or says how much the
ring lost in between. Commands sent with `inject` go through the mailbox, not the bus, so they
never appear here.

`crash` prints the watchdog reason and scratch registers of the last reboot without stopping the
RP, with code addresses resolved to source lines by `addr2line`.

`postmortem` halts the RP and prints both cores' backtraces, the registers, the watchdog registers
and key variables through GDB (`$ARM_GDB_PATH/bin/arm-none-eabi-gdb`, as in `.vscode/launch.json`),
then resumes it; `--leave-halted` keeps it stopped for `swd.py resume`. Halting stops the
cartridge bus, so the ST sees a dead cartridge until the RP resumes.

`heap` reads newlib's own malloc state while the RP keeps running, so it needs no firmware code
and works on release builds. It prints the heap's size (from the end of `.bss` to
`__StackLimit`), the arena taken from it so far, the **peak** arena ever reached
(`__malloc_max_sbrked_mem`) with how close that came to the stack, and, by walking the heap's
chunks, the bytes in use, the free bytes inside the arena, how many free blocks they are in and the
largest one. The heap only grows (memory freed stays in the arena for reuse), so the peak also
catches short-lived allocations between samples. A shortage shows as a peak with little room left
before the stack, or as plenty of free bytes but a small largest block (fragmentation). `--watch
SECONDS` samples until Ctrl-C; `--csv FILE` appends every sample for later comparison. If the
heap changes while it is read, the chunk walk is retried once and otherwise reported as failed.
The heap's limit is `__StackLimit` in `rp/src/memmap_rp.ld`: the end of `RAM`, `0x20020000`, where
the cartridge window starts, so no allocation is handed addresses the ST reads.

OpenOCD is `$OPENOCD`, `openocd` on `PATH`, or `../pico/openocd/src/openocd`; its scripts come
from `$PICO_OPENOCD_PATH`, the variable `.vscode/launch.json` uses. A command that fails on a
momentary debug-port drop (common while the firmware changes its clock early in boot) is retried.
Close a VS Code debug session first: only one program can use the probe.

## Hardware harnesses

Scripts that run a set of checks on the device and print PASS or FAIL for each. All need the
Debug Probe and `console.py watch` running (they read its log, never the UART); each writes a JSON
report to `logs/` and exits 0 only when every check passes.

```bash
python3 tools/dev/tools_harness.py --channel                 # the command channel's checks
python3 tools/dev/tools_harness.py --channel --rounds 1000   # and 1,000 select/hello/unselect rounds
```

- `tools_harness.py --channel` checks the tools against the command channel on a debug build:
  ROM3 selected and given back with no ST (`app select`), injected commands and their answers,
  `counters`, `ring`, a job run to its result (`job.py run dummy 2`), and the window whole test
  data again after unselect (`swd.py window --pattern`); `--rounds N` adds N
  select/hello/unselect rounds. Its full mode, without `--channel`, is the template's: it checks
  `screen`, `text` and `key`, which this headless firmware refuses, and a SELECT press, which
  here leaves for Booster, so it does not apply. `--build` also builds both types and checks
  their flags and symbols, `--flash` flashes the debug build first.
- Not imported (they drive what this firmware does not have): `st_harness.py`, which tested the
  template's cartridge command path from the ST's side, and `download_harness.py`.

## Measuring builds

- `measure_builds.sh` builds each build type out of tree with `-fstack-usage` and
  `-fcallgraph-info=su`, and reports the flash, RAM and heap numbers.
- `stackdepth.py` computes worst-case stack depth from those builds (static call edges only, so
  treat its answer as a floor).
- clang-tidy runs in VS Code with the repository's `.clang-tidy`; the sibling's `tidy.sh` was not
  imported.

`logs/` and `builds/` are generated here and are gitignored.
