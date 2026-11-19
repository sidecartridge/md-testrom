# AGENTS.md — Multidevice Test Playbook

**Read `CLAUDE.md` first.** It is the source of truth for what this repo is (the
SidecarTridge Multi-device's test microfirmware and the Atari program that runs
against it), the build flow and its gotchas, the architecture, the divergences
from the microfirmware template, and the working style. This file does not
repeat any of it.

What lives here instead: getting a host machine set up, the commands worth
copy-pasting, a symptom → fix table for the failures this project actually
produces, and the hard rules.

## 1. Host environment setup

- **ARM GNU Toolchain 14.2** — `arm-none-eabi-gcc` on `PATH`, or
  `PICO_TOOLCHAIN_PATH` pointing at its `bin` directory.
- **CMake 3.26+** and **Python 3**.
- **Git submodules** (`pico-sdk` 2.2.0, `pico-extras` sdk-2.2.0, `fatfs-sdk`
  v3.6.2, `emu/emumd`): `git submodule update --init` (`rp/build.sh` pins the
  first three on every build).
- **`atarist-toolkit-docker` 1.5.0** (`stcmd`, needs Docker) — only for the
  Atari program. Export `STCMD_IMAGE_TAG=1.5.0`, and `STCMD_NO_TTY=1` when not
  in a terminal.
- **EmuMD's Hatari** — `tools/dev/build_hatari_fork.sh` once (macOS:
  `brew install cmake sdl2 libpng pkg-config`), and TOS images linked into
  `emu/tos/` as `tos104uk.img`, `tos162uk.img`, `tos206us.img`.
- **Raspberry Pi Debug Probe**, for the device only — wired to the
  Multi-device's SWD pins and its UART (GPIO 0/1), and **OpenOCD**: set
  `OPENOCD` to the Raspberry Pi fork's build (`../pico/openocd/src/openocd`)
  and `PICO_OPENOCD_PATH` to its `tcl/`. See `tools/dev/README.md`.

## 2. Common commands

```bash
# Full build: dist/<uuid>-<version>.uf2 and dist/<uuid>.json
./build.sh pico_w release 44444444-4444-4444-8444-444444444444

# Verify a change quickly: out of tree in tools/dev/builds/<type>
tools/dev/flash.sh release --build-only        # or debug

# Host tests, under ASan and UBSan
make -C tests/host test

# EmuMD: build the .mdfw and boot it on TOS 1.04, 1.62 and 2.06
python3 tools/dev/emu_check.py
# MDTEST.TOS against the firmware: every test on ST, STE, Mega STE, TT, Falcon
python3 tools/dev/emu_harness.py --all
python3 tools/dev/emu_harness.py --setup st-104 --test main-screen

# The device (a session starts from the test microfirmware launched from Booster)
python3 tools/dev/console.py watch &           # the debug UART, to tools/dev/logs/console.log
tools/dev/flash.sh debug --probe               # build, flash, verify, read the build ID
python3 tools/dev/swd.py window --pattern
python3 tools/dev/swd.py counters              # the command channel's counters
python3 tools/dev/swd.py heap
python3 tools/dev/swd.py crash
```

## 3. Symptom → fix

| Symptom | Cause | Fix |
| --- | --- | --- |
| `flash driver 'rp2xxx' not found` | An old `openocd` earlier on `PATH` | `export OPENOCD=…/pico/openocd/src/openocd` |
| Flashed, but the RP jumps straight to Booster | Booster's settings name another app as the current one | Launch the test microfirmware from Booster, then flash over it |
| EmuMD build: `cast to smaller integer type` | An address kept in a 32-bit integer | `uintptr_t` (EmuMD's host is 64-bit) |
| EmuMD: every ROM3 read is `$FFFF` | EmuMD does not serve ROM3 data | Expected: ROM3's data is checked on the device |
| `emu_harness.py`: no `MDTEST:` lines | Hatari without NatFeats | The harness passes `--natfeats yes`; by hand, add it |
| EmuMD screenshot blank | The TOS has not finished booting | More `--frames` (STE and Mega STE take about 2,000) |
| `swd.py window --pattern` fails a few words at `$FA2004` | Something wrote the template's token into the window | The window must be filled after `chandler_init()` |
| `stcmd` pulls an image that does not exist | `$VERSION` (the app's) taken as the image tag by an old wrapper | `STCMD_IMAGE_TAG=1.5.0`, and never export `VERSION` |

## 4. Hard rules

- Never modify a submodule; FatFs is configured in `rp/src/ff/ffconf.h`.
- Never probe Booster. A step that ends in Booster ends the probe session.
- Never flash through the probe while a program on the ST reads the cartridge: the ST
  crashes (four bombs). Flash with the ST idle at the desktop.
- Never give EmuMD's Hatari a GEMDOS drive.
- Nothing committed names a planning item or the local planning notes; no AI
  attribution anywhere (`CLAUDE.md`).
