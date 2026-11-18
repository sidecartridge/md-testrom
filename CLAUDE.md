# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

See also: `README.md` (end-user instructions, test descriptions, blind-mode setup).

## What this repo is

The **Multidevice Test** microfirmware for the SidecarTridge Multi-device (Atari ST / STE / MegaST(E) / TT / Falcon). It is a stripped-down fork of `md-microfirmware-template` (an early 2025 version) that does one thing: serve a fixed 128 KB block of pseudo-random data over the cartridge port so a TOS program on the Atari can read it back and compare it against a copy on disk. It validates the bus connection and the timing of the ROM emulation.

There are two deliverables:
- **The microfirmware** (`dist/<APP_UUID>-<VERSION>.uf2` + `dist/<APP_UUID>.json`), installed through the Booster Configurator, which emulates ROM4 + ROM3 with `target/atarist/TESTROM.BIN`.
- **The Atari test program** `TESTSCRT.TOS` (`app/atarist/`), which the user copies to the Atari together with a `TESTROM.BIN` that must be **byte-identical** to the one embedded in the firmware.

Unlike the template there is no m68k cartridge code, no command channel (ROM3 is plain read-only data here), no terminal/display, no SD card and no networking.

## Build

### Microfirmware

```bash
# <board_type> = pico | pico_w
# <build_type> = release | debug
# <app_uuid_key> = UUID4 identifying this app, must match the app's release UUID
./build.sh pico_w release 44444444-4444-4444-8444-444444444444
```

Required host environment:
- ARM GNU toolchain (CI uses Ubuntu's `gcc-arm-none-eabi`); locally export `PICO_TOOLCHAIN_PATH` if needed.
- `atarist-toolkit-docker` (`stcmd`) — used by `target/atarist/build.sh`. It runs `docker run -it`, so it needs a TTY; CI strips `-it` from `/usr/local/bin/stcmd` with `sed`.
- Python 3 (for `firmware.py`) and `md5sum` (on macOS, from coreutils).

Build flow (`build.sh`):
1. Copies `version.txt` into `rp/`, `target/` and `app/atarist/`.
2. Deletes and recreates `dist/`.
3. `target/atarist/build.sh`: copies `TESTROM.BIN` to `dist/FIRMWARE.IMG`, pads it to 128 KB, and `firmware.py` converts it into `rp/src/include/target_firmware.h` (a `uint16_t` array, trailing zero bytes trimmed). **It does not generate the random data** — it embeds whatever `target/atarist/TESTROM.BIN` is committed.
4. `rp/build.sh`: pins submodules (pico-sdk `2.1.1`, pico-extras `sdk-2.1.1`), wipes `rp/build/`, runs CMake and `make -j4`, writes `rp/dist/rp-<board>[-debug].uf2`.
5. MD5 of the UF2, rename to `dist/<APP_UUID>-<VERSION>.uf2`, and substitute UUID/MD5/version into `dist/<APP_UUID>.json` from `desc/app.json`.

### Atari test program (`app/atarist/`)

`build.sh` does **not** build it. Build it by hand with `stcmd` from `app/atarist/` (m68k-atari-mint-gcc + libcmini inside the Docker image):

```bash
cd app/atarist
ST_WORKING_FOLDER=$PWD stcmd make            # release: regenerates dist/TESTROM.BIN
ST_WORKING_FOLDER=$PWD stcmd make DEBUG_MODE=1   # debug: uses test/hatarrom.bin, 1,000 iterations
```

(The README's "run `make` at the repo root" instructions are stale: the root `Makefile` only has `tag`.)

### Build gotchas
- **The random data and the firmware must match.** `make` (release) in `app/atarist/` runs `src/generate_random_data.py`, which writes a *new* random `dist/TESTROM.BIN` on every run. That file does not reach the firmware unless it is copied over `target/atarist/TESTROM.BIN` and the firmware rebuilt. Shipping a `TESTSCRT.TOS`/`TESTROM.BIN` pair against a firmware built from a different `TESTROM.BIN` makes every test fail. Never regenerate casually; if you must, update `target/atarist/TESTROM.BIN` in the same change.
- **Version string.** Bytes 4–14 of `TESTROM.BIN` hold the version string that `testDifferentVersions()` compares with `VERSION` (from `version.txt`, passed as `-DVERSION`). The generator writes the first line of `version.txt` there. The committed `target/atarist/TESTROM.BIN` currently carries `0.0.3` while `version.txt` is `v0.0.3` — verify before relying on that test.
- **Debug data is different data.** With `DEBUG_MODE` set, the TOS program reads `HATARROM.BIN` (`app/atarist/test/hatarrom.bin`), which differs from `target/atarist/TESTROM.BIN`, so a debug TOS build only passes against a firmware built from `hatarrom.bin`.
- `rp/build.sh` always configures CMake as `Debug` (`-DCMAKE_BUILD_TYPE=Debug`); `<build_type>` only sets `DEBUG_MODE` (UART stdio + `DPRINTF`) and the UF2 file name.
- `target/atarist/build.sh` calls `stcmd make` in a directory with no Makefile; that step fails harmlessly. None of the scripts use `set -e`, so read the output: a failed step does not stop the build, and a stale `target_firmware.h` or UF2 can be left behind.
- `rp/build.sh` runs `git submodule update --init --recursive` and hard-checkouts the pinned tags on every build, and `build.sh` deletes `dist/` — local edits inside `pico-sdk/`/`pico-extras/`, or anything left in `dist/`, are lost.
- Without `APP_UUID_KEY` CMake falls back to the development UUID `44444444-4444-4444-8444-444444444444`.

### CI / release
- `.github/workflows/build.yml` builds `pico_w` release on every pull request with the `APP_UUID` secret.
- `.github/workflows/release.yml` triggers on `v*` tags: builds, attaches the UF2 + JSON to the GitHub Release (notes from `CHANGELOG.md` up to the first `---`), uploads them to `s3://atarist.sidecartridge.com/`. It does **not** build or attach `TESTSCRT.TOS` / `TESTROM.BIN`.
- `make tag` tags HEAD with the contents of `version.txt` and pushes it (triggers the release). Tagging/releasing is the repo owner's call — a change lands as a `version.txt` bump plus a `CHANGELOG.md` entry.
- `secrets.sh` holds AWS credentials and is gitignored. Never print, commit or copy its contents.

### Tests / verification
There is no test suite yet. Two benches, used in this order (`tools/dev/README.md` has the commands):

1. **EmuMD** (submodule `emu/emumd`): the firmware's own C code built for this computer and run in EmuMD's Hatari with a Multi-device on the cartridge port. Use it first, and for everything it can show. `tools/dev/build_hatari_fork.sh` once (the build and EmuTOS go to `tools/dev/builds/emumd-cache/`), TOS images linked in `emu/tos/` (gitignored), `tools/dev/mdfw.sh` to build and run. Its limits for this firmware: ROM4 is a 64 KB array (this window is 128 KB, so a port must not write past 64 KB), an ST read of ROM3 returns no firmware data (it only feeds the command ring), and there is no PIO, DMA, SELECT, LED, timing or real radio. Never give its Hatari a GEMDOS drive: it takes the first 1 KB of the cartridge. The firmware itself is not ported to EmuMD yet.
2. **The device**, only for what EmuMD cannot show (the bus and its timing, ROM3 as data, the real card and radios, SELECT, the LED, flash and memory numbers): a Multi-device on an Atari with a Raspberry Pi Debug Probe on SWD and the debug UART. A session starts with the test microfirmware launched from Booster (a firmware flashed while Booster's settings name another app jumps straight back to Booster); then `tools/dev/console.py watch`, `tools/dev/flash.sh debug --probe`, `tools/dev/swd.py window target/atarist/TESTROM.BIN`. Never probe Booster.

- `DPRINTF` (`rp/src/include/debug.h`) compiles to nothing unless `_DEBUG != 0`, and UART stdio is only enabled for debug builds (921,600 baud), so a release UF2 prints nothing.
- Every build carries a build ID in flash (`rp/src/build_id.cmake`: the git commit, `-dirty.<hash>` for uncommitted changes, `+cmakedebug` for a CMake Debug build, `+debug` for `DEBUG_MODE`), and the ELF keeps its symbol table; `tools/dev/swd.py` finds everything by symbol.
- `OPENOCD` must be the Raspberry Pi fork's build (`../pico/openocd/src/openocd`); an older `openocd` earlier on `PATH` fails with `flash driver 'rp2xxx' not found`.
- On-chip debugging uses a Picoprobe/Debug Probe and the `cortex-debug` config in `.vscode/launch.json` (needs `ARM_GDB_PATH` and `PICO_OPENOCD_PATH`). CMake source dir `rp/src`, build dir `rp/build`; `tools/dev/flash.sh` builds out of tree in `tools/dev/builds/`.

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
- `main.c` — sets clock (225 MHz) and voltage (`VREG_VOLTAGE_1_10`), prints the flash layout in debug, runs `gconfig_init` / `aconfig_init` (jumping to **Booster** via `reset_jump_to_booster()` on failure) and calls `emul_start()`. **Don't add features to `main.c`.**
- `emul.c` — the whole app: copies `target_firmware` into `ROM_IN_RAM`, `init_romemul(NULL, NULL, false)` (ROM emulation only, no command callbacks), turns the LED on, then waits for a SELECT press; on release it sets `BOOT_FEATURE=BOOSTER` in the global config and jumps back to Booster.
- `romemul.c` / `romemul.pio` — the ROM read engine: a PIO SM latches the 16-bit address and a chained DMA pair serves the word from `ROM_IN_RAM`. No IRQ, no CPU. This is exactly what the test exercises; changes here produce very strange bugs.
- `gconfig.c` / `aconfig.c` / `settings/` — global and per-app config in flash sectors.
- `blink.c`, `select.c`, `reset.c` — LED, SELECT button, jump-to-booster.
- `include/constants.h` — GPIO map, `ROM_BANKS 2`, `ROM_SIZE_BYTES 0x10000`, clock/voltage.

### Cartridge window
The Atari sees 128 KB at `$FA0000`–`$FBFFFF`: ROM4 (`$FA0000`, first 64 KB of `TESTROM.BIN`) and ROM3 (`$FB0000`, second 64 KB). The RP serves both from `ROM_IN_RAM` (`0x20020000`, 128 KB). The TOS program reads the window as one contiguous block from `$FA0000`.

### Atari test program (`app/atarist/src/`)
- `main.c` — loads `TESTROM.BIN` (or `HATARROM.BIN` in debug) from the current directory, then for each bank runs: version string check, sequential word/byte reads, random word/byte reads (1,000,000 iterations each in release), and address-line walks, each with a "stats" variant. Mismatches print the failing address. (`ROM3_MEMORY_START` is defined wrongly as `$FC0000` but is unused; addresses are computed from `ROM4_MEMORY_START + bank * 64 KB`.)
- `screen.c` — screen setup helpers.

### Memory layout (`rp/src/memmap_rp.ld`)

| Region | Origin | Length | Purpose |
| --- | --- | --- | --- |
| `FLASH` | `0x10000000` | 1024 K | App code |
| `ROM_TEMP` | `0x10100000` | 128 K | Scratch area for loaded ROMs |
| `BOOSTER_APP_FLASH` | `0x10120000` | 768 K | Reserved for the Booster app (do not write) |
| `CONFIG_FLASH` | `0x101E0000` | 120 K | 30 sectors of per-app config |
| `GLOBAL_LOOKUP_FLASH` | `0x101FE000` | 4 K | UUID → config-sector lookup |
| `GLOBAL_CONFIG_FLASH` | `0x101FF000` | 4 K | Global config |
| `RAM` | `0x20000000` | 128 K | Data, BSS, heap |
| `ROM_IN_RAM` | `0x20020000` | 128 K | ROM4 + ROM3 as the Atari reads them |
| `SCRATCH_X` / `SCRATCH_Y` | `0x20040000` / `0x20041000` | 4 K each | Stacks |

### App identity
`CURRENT_APP_UUID_KEY` (from the `APP_UUID_KEY` env var at CMake time) is the app's UUID4 and the key into `GLOBAL_LOOKUP_FLASH`. A UUID Booster doesn't know → the app jumps to Booster.

## Publishing

`desc/app.json` is the descriptor template. Leave `<APP_UUID>`, `<APP_VERSION>` and `<BINARY_MD5_HASH>` as placeholders — `build.sh` substitutes them. The `binary` URL points at `https://atarist.sidecartridge.com/<APP_UUID>-<APP_VERSION>.uf2`, where the release workflow uploads it.

## Editing guardrails

- **Never modify** `pico-sdk/`, `pico-extras/` or `emu/emumd/` — pinned submodules (the first two re-pinned on every build).
- Don't touch `main.c` for feature work — start in `emul.c`.
- Don't change `target/atarist/TESTROM.BIN` or `app/atarist/test/hatarrom.bin` unless that is the task, and then keep the firmware and the TOS-side copy in sync (see Build gotchas).
- Match the existing C style (`.clang-format`, `.clang-tidy`). VS Code runs both; the command-line build runs neither.

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
