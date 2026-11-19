# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

See also: `README.md` (end-user instructions, test descriptions, blind-mode setup).

## What this repo is

The **Multidevice Test** microfirmware for the SidecarTridge Multi-device (Atari ST / STE / MegaST(E) / TT / Falcon), and the TOS program that tests the device from the Atari. The firmware serves a 128 KB window of computed test data over the cartridge port, which the program computes too and reads back; it validates the bus connection and the timing of the ROM emulation. It is headless: it draws nothing and puts no code on the ST, which boots as if no cartridge were there.

There are two deliverables:
- **The microfirmware** (`dist/<APP_UUID>-<VERSION>.uf2` + `dist/<APP_UUID>.json`), installed through the Booster Configurator, which fills ROM4 + ROM3 with the test pattern at boot.
- **The Atari test program** `MDTEST.TOS` (`app/atarist/`, `dist/MDTEST.TOS`), one file: it computes the same pattern, so it needs no data file.

The window's layout and the pattern are one plain C header, `common/mdtest.h` (with `common/mdtest.c`), shared by the firmware, the program, the host tests and the tools: never write a window offset anywhere else.

The firmware's app-agnostic modules are the SidecarTridge microfirmware template's (`md-microfirmware-template`), taken through `md-rom-emulator`, which adapted them to a 128 KB two-bank window; see "Divergences from the template". The command channel (ROM3 reads captured and parsed on the RP) is running; while ROM3 is test data the program sends only its one-way line-test verdict.

## Build

### Microfirmware

```bash
# <board_type> = pico_w (the only board: the device tests need its radio chip)
# <build_type> = release | debug
# <app_uuid_key> = UUID4 identifying this app; the development UUID locally
./build.sh pico_w release 44444444-4444-4444-8444-444444444444
```

Required host environment:
- ARM GNU Toolchain 14.2 (`arm-none-eabi-gcc` on `PATH`, or `PICO_TOOLCHAIN_PATH`), CMake 3.26+, Python 3.
- Submodules: `pico-sdk` 2.2.0, `pico-extras` sdk-2.2.0, `fatfs-sdk` v3.6.2, `emu/emumd` (EmuMD). `rp/build.sh` initialises and pins the first three.
- `atarist-toolkit-docker` 1.5.0 (`stcmd`) for the Atari program only. Run it with `STCMD_IMAGE_TAG=1.5.0` (the image, whatever `latest` is) and `STCMD_NO_TTY=1` outside a terminal; CI installs it with the v1.5.0 installer.

Build flow (`build.sh`, every script `set -Eeo pipefail`; a failed step stops the build with `ERROR: <script>: failed at line N`, and `build.sh` empties `dist/` first, so a failed build leaves no UF2 or JSON behind):
1. Refuses any board but `pico_w`, any build type but `release` / `debug`; copies `version.txt` into `rp/` and `app/atarist/`.
2. `rp/build.sh`: pins the submodules, wipes `rp/build/` and `rp/dist/`, CMake **Release** for both build types, writes `rp/dist/rp-<board>[-debug].uf2`.
3. `MDTEST.TOS`: `stcmd make -C app/atarist` with the repository mounted (it shares `common/`), toolkit image `STCMD_IMAGE_TAG` (default 1.5.0), copied to `dist/`.
4. MD5 of the UF2, rename to `dist/<APP_UUID>-<VERSION>.uf2`, and substitute UUID/MD5/version into `dist/<APP_UUID>.json` from `desc/app.json`.

`RELEASE_DATE="YYYY-MM-DD HH:MM:SS"` fixes the date the image carries, so two builds of one commit are byte-identical. `tools/dev/flash.sh <release|debug> --build-only` builds out of tree in `tools/dev/builds/` in seconds, without touching `rp/build` or the submodules.

### Atari test program (`app/atarist/`)

`build.sh` builds it. By hand, from the repository root (m68k-atari-mint-gcc + libcmini inside the Docker image; the whole repository is mounted because the program compiles `common/`):

```bash
cp version.txt app/atarist/
STCMD_IMAGE_TAG=1.5.0 ST_WORKING_FOLDER=$PWD stcmd make -C app/atarist   # app/atarist/dist/MDTEST.TOS
```

`EXTRA_CFLAGS=-DPROGRAM_LAYOUT_VERSION=N` (with its own `DIST=`/`BUILD=`) builds a program that reads layout N, which the EmuMD harness uses to show a version mismatch.

### Build gotchas
- **Both build types are CMake `Release`.** `<build_type>` sets only `DEBUG_MODE` (`DPRINTF`, UART stdio at 921,600 baud, the SWD mailbox) and the UF2's name, so a debug build runs the code that ships. `RP_CMAKE_BUILD_TYPE` overrides the CMake type in `rp/build.sh` and `tools/dev/flash.sh` (with a warning; the build ID then carries it, `+cmakedebug` for Debug).
- **v0.0.3's program and data are retired.** The `TESTSCRT.TOS` / `TESTROM.BIN` pair users have (the `sidecartridge/atarist-sidecart-test-rom` v0.0.3 release) fails against this firmware, which no longer serves that data; `MDTEST.TOS` recognises a device still running v0.0.3 and says to update it.
- `rp/build.sh` runs `git submodule update --init --recursive` and hard-checkouts the pinned tags on every build, and `build.sh` deletes `dist/` — local edits inside a submodule, or anything left in `dist/`, are lost. The FatFs configuration is `rp/src/ff/ffconf.h`, which shadows the submodule's.
- Without `APP_UUID_KEY` CMake falls back to the development UUID `44444444-4444-4444-8444-444444444444`.

### CI / release
- `.github/workflows/build.yml` runs the host tests and builds `pico_w` release and debug on every pull request, with the ARM GNU Toolchain 14.2 and toolkit 1.5.0, and the `APP_UUID` secret (the development UUID where it is not available).
- `.github/workflows/release.yml` triggers on `v*` tags: builds, attaches the UF2 + JSON to the GitHub Release (notes from `CHANGELOG.md` up to the first `---`), uploads them to `s3://atarist.sidecartridge.com/`. It does not attach `MDTEST.TOS` yet (the build workflow keeps it as an artifact).
- `make tag` tags HEAD with the contents of `version.txt` and pushes it (triggers the release). Tagging/releasing is the repo owner's call — a change lands as a `version.txt` bump plus a `CHANGELOG.md` entry.
- `secrets.sh` holds AWS credentials and is gitignored. Never print, commit or copy its contents.

### Tests / verification
Three benches, used in this order (`tools/dev/README.md` has the commands):

1. **Host tests**: `make -C tests/host test` — the command parser (`tprotocol.c`) under ASan and UBSan, in seconds.
2. **EmuMD** (submodule `emu/emumd`): the firmware's own C code built for this computer (`mdfw.ini`, `emu/`) and run in EmuMD's Hatari with a Multi-device on the cartridge port. Use it first, and for everything it can show. `tools/dev/build_hatari_fork.sh` once (the build and EmuTOS go to `tools/dev/builds/emumd-cache/`), TOS images linked in `emu/tos/` (gitignored), then `python3 tools/dev/emu_check.py`: it builds `build/md-testrom.mdfw` and boots it on TOS 1.04 (ST), 1.62 (STE) and 2.06 (Mega STE), PASS when the firmware's log shows the test image verified and the main loop running. `tools/dev/emu_harness.py` runs `MDTEST.TOS` from a floppy's AUTO folder against the firmware (or with nothing in the port), presses keys and checks the lines the program prints through NatFeats and the files it saves (`tools/dev/emu_tests/*.steps`; `--all` runs every test on the ST, STE, Mega STE, TT and Falcon setups); never give its Hatari a GEMDOS drive, which takes the first 1 KB of the cartridge. EmuMD's limits for this firmware: an ST read of ROM3 returns no firmware data (`$FFFF`; it only feeds the command ring), so the ROM3 half of the read tests runs on the device only; and there is no PIO, DMA, SELECT button, LED, timing or real radio.
3. **The device**, only for what EmuMD cannot show (the bus and its timing, ROM3 as data, the real card and radios, SELECT, the LED, flash and memory numbers): a Multi-device on an Atari with a Raspberry Pi Debug Probe on SWD and the debug UART. A session starts with the test microfirmware launched from Booster (a firmware flashed while Booster's settings name another app jumps straight back to Booster); then `tools/dev/console.py watch`, `tools/dev/flash.sh debug --probe`, `tools/dev/swd.py window --pattern`. Flash only with the ST idle at the desktop: a flash while a program reads the cartridge crashes the ST. Never probe Booster.

- `DPRINTF` (`rp/src/include/debug.h`) compiles to nothing unless `_DEBUG != 0`, and UART stdio is only enabled for debug builds (921,600 baud), so a release UF2 prints nothing.
- Every build carries a build ID in flash (`rp/src/build_id.cmake`: the git commit, `-dirty.<hash>` for uncommitted changes, `+debug` for `DEBUG_MODE`), and the ELF keeps its symbol table; `tools/dev/swd.py` finds everything by symbol.
- `OPENOCD` must be the Raspberry Pi fork's build (`../pico/openocd/src/openocd`); an older `openocd` earlier on `PATH` fails with `flash driver 'rp2xxx' not found`.
- On-chip debugging uses a Picoprobe/Debug Probe and the `cortex-debug` config in `.vscode/launch.json` (needs `ARM_GDB_PATH` and `PICO_OPENOCD_PATH`). CMake source dir `rp/src`, build dir `rp/build`; `.vscode/cmake-variants.yaml` gives the two builds.

### Planning backlog (`docs/`)
`docs/epics/` holds the local planning notes (`cockpit.sh` regenerates `STATUS.md`; `ITERATIONS.md`
carries the narrative; `DECISIONS.md` the decisions and standing constraints; its `README.md` the
working rules). **The whole `docs/` tree is gitignored** and lives only on the developer's machine.

- **Never name an epic, story, iteration or task in anything that is committed** — code comments,
  documentation, `CHANGELOG.md`, commit messages, PR descriptions. An identifier like
  `EPIC-03 STORY-01` tells a reader of this repository nothing and cannot be looked up, and a
  commit message cannot be scrubbed afterwards. Write what the code does and why instead.
- **Never reference a path under `docs/`** from anything that ships, for the same reason: someone
  who clones the repo has no `docs/`. Inline the information itself.
- Traceability runs the other way: once work lands, record the commit hash in the planning note.
- Before tagging a release, check:
  `git grep -IiE "EPIC-|STORY-|\bepics?\b|\bstor(y|ies)\b|docs/" -- . ':!CLAUDE.md' ':!.gitignore'`
  must come back empty (this section and the `.gitignore` entry are the only places the backlog
  is mentioned).

## Architecture

### RP2040 side (`rp/src/`)
- `main.c` — the template's: sets clock (225 MHz) and voltage (`VREG_VOLTAGE_1_10`), prints the flash layout in debug, runs `gconfig_init` / `aconfig_init` (jumping to **Booster** via `reset_jump_to_booster()` on failure) and calls `emul_start()`. **Don't add features to `main.c`.**
- `emul.c` — the app. Bring-up order: `health_init()` + watchdog → `commemul_init()` (the ROM3 capture) → `chandler_init()` → the test image copied into `ROM_IN_RAM` → `romemul_initTwoBanks()` (only now does the ST see the window) → SELECT. Main loop: `health_feed()`, `devhooks_poll()`, `select_poll()`, `chandler_loop()`, `health_tick()`. A short SELECT press (`emul_selectShortPress()`) sets `BOOT_FEATURE=BOOSTER` and jumps to Booster; a 10 s press is the factory reset. Nothing else starts at boot: no SD card, no radio chip, no LED.
- `romemul.c` / `romemul.pio` — the two-bank ROM read engine (md-rom-emulator's ROM mode): two monitor state machines raise a PIO IRQ on !ROM4 or !ROM3, a reader samples A1–A16 plus !ROM4 as bit 16, and a chained DMA pair serves the word from `ROM_IN_RAM`. No IRQ handler, no CPU. This is exactly what the test exercises; changes here produce very strange bugs.
- `commemul.c` / `commemul.pio` — the template's ROM3 capture: a PIO state machine samples every ROM3 access into a 4 KB DMA ring (`.comm_ring`, at the start of `RAM`), drained by polling. It shares pio0 with the engine: its writes to the READ/WRITE level-shifter enables fall before and after the engine's (md-rom-emulator runs the same pair in its debug ROM mode).
- `chandler.c` / `include/chandler.h`, `tprotocol.c` — the template's polled command dispatcher and parser, and the RP-side source of truth for the shared-block offsets (`CHANDLER_*`, at `$FA2000`). No app command is registered yet.
- `health.c` — boot reason, watchdog, heap and stack sampling; `swd.py crash` reads its record.
- `gconfig.c` / `aconfig.c` / `settings/` — global and per-app config in flash sectors. This app keeps no settings of its own (`aconfig.c`'s defaults table is empty, as in v0.0.3).
- `select.c` (SELECT on core 0, polled, with an edge interrupt), `reset.c`, `blink.c`, `network.c`, `sdcard.c`, `hw_config.c` — the template's; `blink.c`, `network.c` and `sdcard.c` are linked but nothing calls them yet.
- `include/devhooks.h` — the debug-only SWD mailbox `tools/dev/swd.py` writes (`inject`, `app heap_hold N`, `app health N`).

### Cartridge window
The Atari sees 128 KB at `$FA0000`–`$FBFFFF`: ROM4 (`$FA0000`, word indexes 0–`$7FFF`) and ROM3 (`$FB0000`, `$8000`–`$FFFF`). The RP serves both from `ROM_IN_RAM` (`0x20020000`, 128 KB) and fills them at boot with `mdtest_fillWindow()` (`common/mdtest.h`):
- **The pattern:** `mdtest_patternWord(index)`, a bijection of the 16-bit word index, so no two words are equal and a word read from the wrong address names the address it came from. Index 0 gives 0: the window never starts with a cartridge magic (`$ABCDEF42`, `$FA52235F`), so the ST boots as if no cartridge were there.
- **The header:** 32 words at `$FA1FC0` (index `$0FE0`, where no address-line test reads): the identifier `MDTESTWN`, the layout version, a state word (ROM3 as data, or selected for commands), a checksum (the header's words sum to 0), the firmware's version and its build ID. The program recognises the firmware from it with plain reads.
- `chandler_init()` seeds its token and seed at `$FA2004`, so the window is filled after it; nothing writes the window afterwards. The command block (`$FA2000`) and the result buffer (`$FA4000`) are named in `mdtest.h`, test data until ROM3 is selected.

### Atari test program (`app/atarist/src/`)
`MDTEST.TOS` runs in supervisor mode on its own stack, text only over GEMDOS and BIOS (any resolution, the AUTO folder):
- `main.c` — the flow: the machine, the device, the menu ([L] quick line test, [K] long pass, [M] on a Mega STE the quick pass at 8, 16 and 16 MHz + cache with the user's setting put back, [S] save report, [Q] quit). The device tests stay locked until a line test passes, for the session; a `make DEBUG=1` build adds [O] to override the gate, and says so. An `MDTEST.BNC` file next to the program starts bench mode: the quick pass in a loop until a key.
- `linetest.c` — the reads, over a window pointer (so the host tests run the same code): words and bytes in order, random words and bytes, the address-line walk (each of A1–A15 alone high and alone low), the same address back to back, far-apart addresses alternating, reads right after a RAM write, and words in order with interrupts off; on each bank. Quick pass about 26 s on an 8 MHz ST, the long one v0.0.3's million random reads. The expected word is computed on the fly (`mdtest_patternWord()`, about 85 µs a word at 8 MHz), so a 512 KB machine needs no 128 KB buffer.
- `diagnose.c` — from mismatches to a named fault, per bank: a word read from the wrong address names its source (`mdtest_patternIndex()`), so address lines are told from data lines; then stuck or shorted data lines, a dead RP pin (its data and address line together), a whole bank answering open bus, the banks swapped, one byte lane, errors that come and go, or "fits no single fault" with the raw first errors.
- `ltscreen.c` — the line test on the screen, the findings with their advice, and the verdict sent to the RP as a one-way frame of ROM3 reads (`MDTEST_CMD_LINE_VERDICT`: verdict, class, first error, error count, run number).
- `machine.c`, `device.c` (the header, or the pattern behind an unreadable header, v0.0.3's data, another microfirmware, open bus), `console.c` (screen, report, NatFeats), `report.c` (every screen line, after one that states the gate), `texts.h` (every string, and the wording rules).

### What the line test can see
The RP's GPIO 6–21 carry the port's data lines D0–D15 and, at another moment of each cycle, its address lines A0–A15 through the level shifters (A0 is never driven: the 68000 has no A0 on the bus). So GPIO 6+n is both Dn and An, and window index bit k is A(k+1); index bit 15 is the bank, chosen by the selects.

| Signal | RP pin | Exercised by | A fault reads as |
| --- | --- | --- | --- |
| D0–D15 | GPIO 6–21 | every read | one bit always 0 or 1; two bits that follow each other; one byte lane |
| A1–A15 | GPIO 7–21 | the address walk, and every read | another address's word: the pattern names which line |
| GPIO 6+n dead | — | every read | Dn and An wrong together |
| !ROM4 | GPIO 22 | every ROM4 read | ROM4 open bus, or ROM3's words |
| !ROM3 | GPIO 26 | every ROM3 read | ROM3 open bus, or ROM4's words |
| READ / WRITE shifter enables | GPIO 27 / 28, driven by the RP | every read | no answer at all, or every address wrong |

No read from the ST tests UDS and LDS (the RP does not sample them: every cartridge read is a word, the CPU picks the byte), R/W, the port's +5 V, SELECT (GPIO 5) or the LED and the radio (the device tests). Under EmuMD ROM3 reads return `$FFFF`, so the line test there passes ROM4 and names ROM3 as not answering.

While ROM3 is test data the firmware parses the ROM3 frames without answering them (`emul.c`): the template's dispatcher writes every answer into the window at `$FA2004`, over the data under test. Debug builds can serve the window as a faulty bus would (`mdtest_injectFault()`: `swd.py app fault KIND A B` on the device, `MDTEST_FAULT="KIND A B"` in Hatari's environment under EmuMD); `tools/dev/bus_faults.py` checks each against the bench loop's verdicts (`swd.py verdict`).

### Memory layout (`rp/src/memmap_rp.ld`)

| Region | Origin | Length | Purpose |
| --- | --- | --- | --- |
| `FLASH` | `0x10000000` | 1024 K | App code |
| `ROM_TEMP` | `0x10100000` | 128 K | Scratch area for loaded ROMs (unused here) |
| `BOOSTER_APP_FLASH` | `0x10120000` | 768 K | Reserved for the Booster app (do not write) |
| `CONFIG_FLASH` | `0x101E0000` | 120 K | 30 sectors of per-app config |
| `GLOBAL_LOOKUP_FLASH` | `0x101FE000` | 4 K | UUID → config-sector lookup |
| `GLOBAL_CONFIG_FLASH` | `0x101FF000` | 4 K | Global config |
| `RAM` | `0x20000000` | 128 K | The ROM3 capture ring (first 4 KB), data, BSS, heap |
| `ROM_IN_RAM` | `0x20020000` | 128 K | ROM4 + ROM3 as the Atari reads them |
| `SCRATCH_X` / `SCRATCH_Y` | `0x20040000` / `0x20041000` | 4 K each | Core 1 (never started) / core 0's stack, guarded |

The heap stops at the end of `RAM` (`__StackLimit` = `0x20020000`, where the window starts), `malloc` returns NULL instead of panicking (`PICO_MALLOC_PANIC=0`), and the link guarantees 32 KB of heap (`PICO_HEAP_SIZE`). v0.0.3's heap limit ran past the window. Release build: 27 KB of code, 133 KB of read-only data (the test image is 128 KB of it), 6.9 KB of data and 6.2 KB of BSS, leaving 113 KB below the window for the heap.

### App identity
`CURRENT_APP_UUID_KEY` (from the `APP_UUID_KEY` env var at CMake time) is the app's UUID4 and the key into `GLOBAL_LOOKUP_FLASH`. A UUID Booster doesn't know → the app jumps to Booster.

### Divergences from the template
The modules under `rp/src` are md-rom-emulator's at `3cd573d` (the template's at `6935f53`, with the two-bank window and the fixes it reported upstream), except:

- **`emul.c`, `emul.h`:** this app's, headless (above).
- **`aconfig.c`, `aconfig.h`:** the template's, with an empty defaults table.
- **`CMakeLists.txt`:** without the display, terminal, downloads, HTTPS and the ROM Emulator's own modules.
- **`chandler.c`, `aconfig.c`, `gconfig.c`, `main.c`:** window and flash addresses kept in `uintptr_t` (or cast through it), so the same sources build for EmuMD's 64-bit host; the device's code is unchanged by it.
- **Not taken:** `term.*`, `display*.*`, `u8g2/`, `download.*`, `devdownload.*`, `httpc/`, `mbedtls_*`, `md5/`, `catalog.*`, `navlist.*`, `ui.*`, `romstore.*`, and the m68k sources.

## Publishing

`desc/app.json` is the descriptor template. Leave `<APP_UUID>`, `<APP_VERSION>` and `<BINARY_MD5_HASH>` as placeholders — `build.sh` substitutes them. The `binary` URL points at `https://atarist.sidecartridge.com/<APP_UUID>-<APP_VERSION>.uf2`, where the release workflow uploads it.

## Editing guardrails

- **Never modify** `pico-sdk/`, `pico-extras/`, `fatfs-sdk/` or `emu/emumd/` — pinned submodules (the first three re-pinned on every build). FatFs is configured in `rp/src/ff/ffconf.h`.
- Don't touch `main.c` for feature work — start in `emul.c`.
- Never change `mdtest_patternWord()` or the header without bumping `MDTEST_LAYOUT_VERSION`: the program and the firmware of different releases must tell each other apart.
- Match the existing C style (`.clang-format`, `.clang-tidy`). VS Code runs both; the command-line build runs neither.
- Keep window and flash addresses in `uintptr_t`, never `uint32_t`: the EmuMD build is 64-bit and fails on a cast that cuts a pointer (`-Werror=pointer-to-int-cast`).

---

## Working style

These behavioral guidelines bias toward caution over speed. For trivial tasks, use judgment.

### 1. Think before coding

Before implementing:
- State your assumptions explicitly. If uncertain, ask.
- If multiple interpretations exist, present them — don't pick silently.
- If a simpler approach exists, say so. Push back when warranted.
- If something is unclear, stop. Name what's confusing. Ask.

### 2. Simplicity first

Minimum code that solves the problem. Nothing speculative.
- No features beyond what was asked.
- No abstractions for single-use code.
- No "flexibility" or "configurability" that wasn't requested.
- No error handling for impossible scenarios.
- If you write 200 lines and it could be 50, rewrite it.

Ask: "Would a senior engineer say this is overcomplicated?" If yes, simplify.

### 3. Surgical changes

Touch only what you must. Clean up only your own mess.
- Don't "improve" adjacent code, comments, or formatting.
- Don't refactor things that aren't broken.
- Match existing style, even if you'd do it differently.
- If you notice unrelated dead code, mention it — don't delete it.
- When your changes orphan an import/variable/function, remove it. Don't remove pre-existing dead code unless asked.

The test: every changed line should trace directly to the user's request.

### 4. Goal-driven execution

Define success criteria. Loop until verified.
- "Add validation" → "Write tests for invalid inputs, then make them pass"
- "Fix the bug" → "Write a test that reproduces it, then make it pass"
- "Refactor X" → "Ensure tests pass before and after"

For multi-step tasks, state a brief plan with a verification check per step.

### 5. No AI attribution

Never add AI-tool attribution to commits, PR descriptions, code comments,
docs, or any other artifact. This means **no**:
- "Generated with Claude Code", "Co-authored by Claude", "Made with ChatGPT",
  or any similar phrasing.
- `Co-Authored-By: Claude …`, `Co-Authored-By: ChatGPT …`, or any other
  AI co-author trailer.
- "AI-assisted", "written with the help of an LLM", etc., as comments or
  changelog entries.

Write the message as the human author. Do not mention AI tools used to
produce the work.
