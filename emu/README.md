# The firmware in Hatari (EmuMD)

[EmuMD](https://github.com/neilrackett/emumd) builds this firmware's own C code
for the computer as a `.mdfw` and runs it in its fork of Hatari, with a
Multi-device on the cartridge port. It is the fast bench for the firmware's
logic and the Atari program, on every machine Hatari emulates. It times
nothing (the emulated Multi-device is infinitely fast) and has no PIO, DMA,
pins or radio, so the bus, timing and sign-off stay on the device, with the
tools in `tools/dev/`.

## Set-up

```sh
git submodule update --init emu/emumd
tools/dev/build_hatari_fork.sh                    # EmuMD's Hatari, once
mkdir -p emu/tos emu/sd
ln -s "/path/to/TOS 1.04 - English UK - 50Hz.img" emu/tos/tos104uk.img
ln -s "/path/to/TOS 1.62 - English-UK - 50Hz"     emu/tos/tos162uk.img
ln -s "/path/to/TOS v2.06 (Mega-STE)(US).img"     emu/tos/tos206us.img
```

`emu/tos/` and `emu/sd/` (the emulated microSD card) are not committed. The
Hatari build and EmuTOS live in `tools/dev/builds/emumd-cache/`, not
`~/.cache/emumd`, which other projects share at other EmuMD versions.

## Build and run

```sh
python3 tools/dev/emu_check.py                    # build, boot three TOS, PASS/FAIL
tools/dev/mdfw.sh build                           # build/md-testrom.mdfw
tools/dev/mdfw.sh run --no-user-config            # a window, an ST with TOS 1.04
```

`mdfw.ini` builds everything in `rp/src` but the hardware layer EmuMD stands in
for, and `emu/standins.c` supplies the rest:

- the two-bank engine's set-up and stop (`romemul_initTwoBanks()`,
  `romemul_stop()`, `commemul_stop()`): EmuMD's window is already 128 KB;
- SELECT: creating the file `EMU_SELECT_FILE` names is a short press, which
  leaves for Booster (EmuMD does not run Booster, so the firmware stops);
- `health.c`, left out: it reads the RP2040's watchdog, fault handlers and SCB.

The radio chip's code (`network.c`, `blink.c`) is left out until a test uses
it; it then needs a `[wifi]` section in `mdfw.ini`. `emu/shim/prefix.h` puts
EmuMD's own `reset.h` (no ARM code) ahead of the firmware's.

## What it cannot show

- **ROM3 data.** An ST read of `$FB0000`–`$FBFFFF` reaches the firmware's
  command ring and returns `$FFFF` to the ST, so the ROM3 half of the read
  tests is checked on the device only. ROM4 is served from the same 128 KB
  window as on the device. Selecting ROM3 as the command channel, its
  commands and unselecting it all work here (they go through the ring and
  answer in ROM4); since the line test cannot pass, the tests reach them
  through the program's debug build: its gate override, or the bench with
  `MDTEST.BNC` holding `commands`.
- **Answer timing.** The firmware's main loop runs on a host thread beside
  the emulator, so an answer now and then comes later than on the device.
- PIO, DMA and bus timing; the SELECT button and the LED; the real card and
  radio; flash and memory numbers.

## Gotchas

- **Never give Hatari a GEMDOS drive** (`--harddrive`): it takes the first 1 KB
  of the cartridge. Programs travel on floppy images
  (`tools/dev/make_floppy.sh FOLDER disk.st`, then `--disk-a disk.st`).
- **`mdfw run --headless` stops after 500 frames** unless `--frames` says
  otherwise; the STE and the Mega STE take about 2,000 to reach the desktop.
- **Without `--no-user-config`** Hatari reads your own `hatari.cfg`.
- **Addresses are 64 bits on the host**: `mdfw.ini` turns a cast that cuts a
  pointer into a build error.
