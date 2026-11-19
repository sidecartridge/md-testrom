/**
 * File: emul.h
 * Author: Diego Parrilla Santamaría
 * Date: January 2025, November 2026
 * Copyright: 2025-2026 - GOODDATA LABS SL
 * Description: The Multidevice Test's application: serve the test window and
 *              listen for the ST's commands, with no screen of its own.
 */

#ifndef EMUL_H
#define EMUL_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "aconfig.h"
#include "constants.h"
#include "debug.h"
#include "memfunc.h"
#include "pico/stdlib.h"
#include "reset.h"
#include "romemul.h"
#include "select.h"

// App commands that tools/dev/swd.py sends over SWD on debug builds
// (`swd.py app <name> [words]`): the name after DEVHOOKS_APP_ is the one the
// tool takes.

// heap_hold N: hold N KB more of heap (0 releases everything held), to test
// running out of it.
#define DEVHOOKS_APP_HEAP_HOLD 1

// health N: run health.c's test N (health_test_t): a hang, a panic, a hard
// fault or a stack overflow, to check the reboot reason recorded for each.
#define DEVHOOKS_APP_HEALTH 4

// fault KIND A B: refill the window, then serve it as a faulty bus would
// (MDTEST_FAULT_*, common/mdtest.h); fault 0 serves it whole again.
#define DEVHOOKS_APP_FAULT 5
// Every Nth command runs unanswered, as if its answer were lost (0: none).
#define DEVHOOKS_APP_DROP 6
// ROM3 selected (1) or given back (0) with no ST, for injected commands.
#define DEVHOOKS_APP_SELECT 7
// An RP restart as the ST sees it, with no reset (channel_benchRestart()).
#define DEVHOOKS_APP_RESTART 8

/**
 * @brief The application: never returns.
 *
 * Copies the test image into the cartridge window and serves both banks
 * (ROM4 at $FA0000, ROM3 at $FB0000) to the ST, captures the ST's ROM3 reads
 * for the command channel, and watches SELECT. A short press leaves for
 * Booster; a press held for SELECT_LONG_RESET is a factory reset.
 */
void emul_start();

/**
 * @brief What a short SELECT press does: the main loop leaves for Booster.
 *
 * select.c calls it on the device; EmuMD's bench, which has no SELECT
 * button, calls it when its press file appears (emu/standins.c).
 */
void emul_selectShortPress(void);

#endif  // EMUL_H
