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
  `health`, and this app's `fault`, `drop`, `select`, `restart`, `chip_fail`, `card_fault`) work on debug builds, and
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
  machine this old; a boot of the RP alone goes through `swd.py reset` or SELECT.
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
- SELECT, the LED, PIO, DMA and timing are not emulated. Bluetooth is Bluepad32's scripted
  controllers only.
- Never give this Hatari a GEMDOS drive (`--harddrive`): it takes the first 1 KB of the
  cartridge. Programs travel on floppy images (`make_floppy_image.py`).

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
`@protocol N` builds a program that speaks protocol N, `@bench TEXT` puts an `MDTEST.BNC` holding
TEXT on the floppy, `@twice` puts the program in the AUTO folder twice (quitting the first starts
it again), `@chipfail` makes the radio chip fail to start (the LED test's third verdict), the step
`request CODE ...` hands the program a bench request (`request 255`: the firmware restarts as the
ST sees it, `app restart`), and `select MS` holds SELECT down for MS ms. `@cardfault N` makes
the microSD card fail (`MDTEST_CARD_FAULT`: 1 no answer, 2 full, 3 a write fails, 4 a byte read
back differs), `@sdapps` puts Booster's apps folder on it, `card out` and `card in` take it out and
put it back, and `sdsame` checks at the end that the card's folder (a `USER.TXT` of the user's,
always) holds what it held before the run. `@wifi HOW` makes EmuMD's Wi-Fi fail (`badauth`,
`nonet`, `fail`, `off`), `@rssi DBM` sets its signal, `@btfail` and `@btnone` make the Bluetooth stand-in's controller
fail or its scan hear nothing, and `floppynot NAME REGEX` checks a saved
file does not hold something (a network's name in a report).

The command channel, with no ST: `job.py run dummy 2 --expect seconds=2` selects ROM3 through
the debug hook, runs a job by injected commands, reads its result from the window and gives ROM3
back (`tools_harness.py --channel` adds hello, echo and the window afterwards). With the ST in
bench mode (`MDTEST.BNC` holding `requests`): `st_loop.py run dummy 2 --expect seconds=2 --times
10`, or the same with `--emu st-104` under EmuMD.

The device tests run the same way: `job.py run device_info`, `job.py run select 10` (press
SELECT within 10 s), `job.py run led 1` (on; 0 off, 2 blink, 3 off and the chip powered down),
`job.py run sd_card` (the card described, its file written, read back and deleted), `job.py run
sd_swap 30` (take the card out and put it back within 30 s each); `swd.py app card_fault N` makes
the card fail as `@cardfault N` does. `job.py run wifi` runs the Wi-Fi test to the web server;
`swd.py app chip_fail 1` makes it stop at the chip.
`job.py run bluetooth` starts the controller and scans (about 14 s); `chip_fail` stops it at the
chip too.

On the device, `swd.py verdict` reads the last line-test verdict the ST sent, and
`bus_faults.py` serves each fault in turn (`swd.py app fault`) while `MDTEST.TOS` runs in bench
mode (an `MDTEST.BNC` file next to it), and compares the verdict's class with the one expected.
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

Builds out of tree in `tools/dev/builds/<type>`, incrementally, the profile releases ship (HTTP and
HTTPS downloads; `APP_DOWNLOAD_HTTPS=0` builds HTTP only, in `tools/dev/builds/<type>-http`). It does not touch `rp/build` or
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
python3 tools/dev/swd.py screen menu.png                         # the setup menu as the ST shows it
python3 tools/dev/swd.py text                                    # the setup menu as text
python3 tools/dev/swd.py shared                                  # token + shared variables
python3 tools/dev/swd.py resume                                  # release cores a debugger left halted
python3 tools/dev/swd.py reset                                   # reset the whole chip, watchdog-style
python3 tools/dev/swd.py select short                            # press SELECT (short press)
python3 tools/dev/select_harness.py bounce                       # 15 ms press: ignored
python3 tools/dev/select_harness.py during-test                  # press in the SELECT test: reported
python3 tools/dev/select_harness.py short                        # short press: the RP restarts
python3 tools/dev/select_harness.py backup settings.bin          # save the settings flash
python3 tools/dev/select_harness.py long --force                 # 10 s press: factory reset
python3 tools/dev/select_harness.py restore settings.bin         # put the settings back
python3 tools/dev/swd.py key g                                   # a keystroke, as if typed on the ST
python3 tools/dev/swd.py key down                                # a named key: up down left right esc return space
python3 tools/dev/swd.py type "put_str FOLDER /roms" --enter     # a line in [S]ettings (the menu takes single keys), one OpenOCD
python3 tools/dev/swd.py app heap_hold 16                        # hold 16 KB more heap (0 releases)
python3 tools/dev/swd.py app wifi 0                              # Wi-Fi down as a lost network (1: up)
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

`screen` renders the 320×200 framebuffer at `DISPLAY_BUFFER_OFFSET` of the cartridge window as a
PNG (scaled 2×, `--scale`). It shows what the RP draws for the ST: the setup menu, not GEM or a
running program. `text` prints the terminal's character buffer (the `screen` array of term.c); the
bottom status line is drawn straight to the framebuffer and only shows in `screen`. `shared`
prints the command sentinel, the random token, the token seed and the shared variables, named
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
erases the global settings, and Booster then clears every app's settings. In this template a short
press restarts the RP. `select release` clears an
override left behind. The press is held inside one OpenOCD session, so nothing else can use the
probe until it ends: to look at the device during a press, put the reads in that same session
(`mww` the override, `sleep`, `mdw`/`mdb` what you want, `mww` it back).

`key`, `app` and `inject` need a `debug` build. They write a small mailbox in RAM
(`rp/src/include/devhooks.h`, found by its `devhooksMailbox` symbol) and wait for the main loop to
acknowledge it. `key` and `inject` queue a protocol command as if the ST had sent it, through
`chandler_injectProtocol()`, so the firmware handles it through its normal path: the same
callbacks, the same answer. The setup terminal is line-based, so a menu command is its key and
then Enter (`swd.py key h`, then `swd.py key $'\n'`); the `[B]` and `[D]` lists take single keys,
by name for the cursor keys (`swd.py key down`: ASCII 0 and the ST's scan code). `app NAME` runs the app command defined as
`DEVHOOKS_APP_<NAME>` in `rp/src/include/emul.h`, handled by `emul_devhooksApp()`:

- `heap_hold KB`: hold KB more kilobytes of heap, on top of what is already held (`heap_hold 0`
  releases everything). Result 0 when the allocation is refused, so repeated calls walk the heap
  down to a known remainder. Watch it with `swd.py heap`.
- `sd 0|1`: 0 makes the SD card look pulled (the presence check fails at once, and nothing mounts),
  1 puts it back (the next poll mounts it, within 2 s): the path a real pull and reinsertion take.
  A pull in the middle of an operation needs GDB: break in `disk_read` or `disk_write`, return
  `RES_ERROR` (1) and set `sdcardTestRemoved`; presence checks read sector 0, so a condition on the
  sector argument (`$r2 != 0`) leaves them alone.
- `health N`: provoke a failure from the main loop 250 ms later (`health.h`'s `health_test_t`): 1
  `panic()`, 2 a HardFault, 3 a hang (the watchdog), 4 a 500 ms stall (no reboot), 5 a stack
  overflow into the guard. Each reboots and names itself in `swd.py crash`, on the console and on
  the menu's "Last restart:" line. Three within 60 s trip the crash-loop guard (setup mode, no
  autorun); a SELECT press clears it. A hang in ROM mode or the Delay wait: write 1 to
  `romModeTestHang` over SWD (a GDB `set var`), and the watchdog reboots the RP into the same ROM.
  `healthMaxFeedGapUs` and `healthMaxFeedGapPhase` hold the longest gap between watchdog feeds.
- `wifi 0|1`: 0 leaves the access point as a lost network would: the link check notices, the
  menu says "Wi-Fi lost; joining again.", and the app joins again by itself 5 s later and refreshes
  the catalog (about 10 s in all). 1 makes the next try now instead of after the backoff. A
  silent loss (the link up, the network gone): set the station's gateway to an address nobody
  answers with GDB (`set var cyw43_state.netif[0].gw.addr = 0xFD01A8C0`, 192.168.1.253) and the
  gateway probe gives up within about 75 s. No AP at boot: rename `WIFI_SSID` in RAM at the first
  connect, with a GDB breakpoint there, and put it back later.

Add an app's own commands the same way: a `DEVHOOKS_APP_<NAME>` define and a case in the handler.
Useful ones in other microfirmwares: stop a boot countdown; stall or fail the next answer on
purpose, to exercise the ST's retry path.

`ring` decodes what the ST sent from the ROM3 capture ring (`commemul.c`) without halting the RP,
on a release build as on a debug one. It holds the last 8,192 bus samples: about 800 small
commands (6 to 10 samples each), or 15 writes of 1 KB. Each frame shows its sample number since the RP booted, the command (named
from the `APP_<APP>` / `APP_<APP>_<COMMAND>` defines, as `term.h` has them, and chandler's
framework commands; others print as hex), its payload size, the random token, the first four 32-bit
parameters in the order `TPROTO_GET_PAYLOAD_PARAM32` reads them, and a mark on a bad checksum.
The oldest frame is often cut by the ring's start and shows as a bad checksum. A frame whose size
is past `MAX_PROTOCOL_PAYLOAD_SIZE` shows as dropped, as the RP drops it.
The DMA keeps writing while the ring is read, so `ring` reads its write position before and
after and drops the samples it may have overwritten meanwhile. During a sustained burst of 1 KB
writes the ST sends about 200,000 samples a second and refills the ring in about 40 ms, faster
than SWD copies it: `ring` then says so instead of printing half-overwritten frames, and reads
normally once the burst is over. `--mark` stores the position in
`tools/dev/logs/ring.mark` and `--since-mark` shows only what came after it, or says how much the
ring lost in between. Commands sent with `key` or `inject` go through the mailbox, not the bus, so
they never appear here. An ST reset through the sentinel shows up as `CHANDLER_ST_HELLO` and two
`CHANDLER_SET_SHARED_VAR` (the machine, then the TOS version).

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
The heap's limit is `__StackLimit` in `rp/src/memmap_rp.ld`. In this firmware it is still the end
of the 128 KB cartridge window (`0x20040000`), not the end of `RAM`, so the size `heap` reports
includes the window, and an allocation can be handed addresses the ST reads.

OpenOCD is `$OPENOCD`, `openocd` on `PATH`, or `../pico/openocd/src/openocd`; its scripts come
from `$PICO_OPENOCD_PATH`, the variable `.vscode/launch.json` uses. A command that fails on a
momentary debug-port drop (common while the firmware changes its clock early in boot) is retried.
Close a VS Code debug session first: only one program can use the probe.

## Hardware harnesses

Scripts that run a set of checks on the device and print PASS or FAIL for each. All need the
Debug Probe and `console.py watch` running (they read its log, never the UART); each writes a JSON
report to `logs/` and exits 0 only when every check passes.

```bash
python3 tools/dev/tools_harness.py --build --flash --reset   # every tool above, against the device
python3 tools/dev/st_harness.py --oversize --long            # the command path, from the ST's side
python3 tools/dev/download_harness.py                        # real downloads, checked by MD5
```

- `tools_harness.py` checks the tools themselves on a debug build: the running firmware and its
  build ID, the console, `screen` / `text` / `shared`, the mailbox commands (`key`, `inject`,
  `app heap_hold`), `counters`, `ring`, a SELECT press (which restarts the RP), `crash` and
  `postmortem`. `--build` also builds both types and checks their flags and symbols, `--flash`
  flashes the debug build first, and `--reset` restarts the RP at the end.
- `st_harness.py` tests the command path from the ST's side. It copies the tree to
  `builds/sttree`, puts `sttest.s` in place of `userfw.s`, builds and flashes that debug firmware,
  resets the ST through the sentinel so it runs the new cartridge code, and starts the tests with
  the debug build's hidden `f` command. The ST reports each result as a `$7Fxx` command, which the setup terminal logs:
  - T0: the return address into TOS the cartridge hands over, the machine and TOS, and on a
    Mega STE its speed and cache at the handover, which must be the user's setting.
  - T1: `d0 = 0` with Z set after each sender.
  - T2: the registers each send keeps.
  - T3 and T4: 100 small commands and 10 of 1 KB, none failed.
  - T5 (`--oversize`): an oversize frame, then a command that must still be answered.
  - T6 (`--long`): a burst of 3,000 commands of 1 KB, about 7 s.

  On a Mega STE `--mste 8|16|16c` sets the speed and cache for the run, as a user would (the
  test program turns the cache off around its sends, as any user firmware must, and puts the
  setting back before its closing reboot). A reset brings the machine back to 8 MHz with no
  cache, so the next handover reads that.

  `--connect-race` instead resets the ST and the RP together, and times the ST's boot commands
  against the RP's Wi-Fi connect. `--no-build --no-flash --no-reboot` runs the tests again at once
  on an ST still in the setup menu from a run: with `download_harness.py --url ... --no-wait` just
  before, the command path is tested while a download runs. The test firmware stays on the RP, so flash the real one
  afterwards. The working tree is never touched.
- `download_harness.py` downloads the files Booster downloads, on the device, and checks each by
  MD5. The expected hashes are read at run time: F1 is `upgrade.bin` against `upgrade.md5`, F2 a
  catalog microfirmware against the catalog's MD5, F3 the catalog itself, F4 a catalog entry behind
  a GitHub redirect chain, and F5 a missing file, which must fail. PORT writes the port out, and
  LONG is a URL past the build's limit, which must be refused. On an HTTPS build (`+https` in
  the build ID) every case runs over http:// and https://; on an HTTP build `https://` must fail
  as not built in. For a failure it checks the reason (`download_err_t`, read from `download.h`)
  and that no file was left. The download runs in the debug-only `rp/src/devdownload.c`, through
  `download.h` as an app would: the harness writes the URL into its RAM, starts it through the
  devhooks mailbox (`DEVHOOKS_APP_DOWNLOAD`) and reads its state over SWD. It needs the SD card and Wi-Fi. `--url URL` downloads one URL and prints the outcome;
  `--no-wait` starts it and frees the probe for another tool (to run `st_harness.py` during a
  download), and `--status` reads the outcome later.

## Measuring builds

- `measure_builds.sh` builds each build type out of tree with `-fstack-usage` and
  `-fcallgraph-info=su`, and reports the flash, RAM and heap numbers.
- `stackdepth.py` computes worst-case stack depth from those builds (static call edges only, so
  treat its answer as a floor).
- `tidy.sh [--build debug|release] [FILE ...]` runs clang-tidy with the repository's `.clang-tidy`
  on the app's own sources (`emul.c`, `catalog.c`, `navlist.c`, `romstore.c`) or the files given,
  relative to `rp/src`, using a `flash.sh --build-only` build's compile commands. They are
  `arm-none-eabi-gcc`'s, so clang parses them as ARMv6-M with the toolchain's newlib as its sysroot;
  without that every C library header is missing and the checks run on a broken parse. It reports
  the files given and their own headers, and exits non-zero when anything was found.

`logs/` and `builds/` are generated here and are gitignored.
