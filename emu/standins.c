/*
 * File: standins.c
 * Description: What the Multidevice Test firmware needs from the hardware
 *              layer EmuMD leaves out (romemul.c, commemul.c, select.c), for
 *              its build under EmuMD only. The device build never sees this.
 */
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>

#include "commemul.h"
#include "romemul.h"
#include "select.h"

/* Both banks are in EmuMD's 128 KB window already (emul.c copied the image
 * there); EmuMD serves ROM4 from it, and ROM3 reads reach the command ring. */
int romemul_initTwoBanks(bool copyFlashToRAM) {
  (void)copyFlashToRAM;
  return 0;
}

void romemul_stop(void) {}
void commemul_stop(void) {}

/* SELECT. EmuMD drops the callbacks the firmware registers, so the bench
 * press does what the firmware's short press does (emul.c): it leaves for
 * Booster, which EmuMD does not run. The bench presses it by creating the
 * file EMU_SELECT_FILE names. */
#include "emul.h"

void select_poll(void) {
  static const char *path;
  static struct timespec last;
  if (!path && !(path = getenv("EMU_SELECT_FILE"))) path = "";
  if (!*path) return;
  struct timespec now;
  clock_gettime(CLOCK_MONOTONIC, &now);
  if ((now.tv_sec - last.tv_sec) * 1000 +
          (now.tv_nsec - last.tv_nsec) / 1000000 < 20)
    return;
  last = now;
  if (access(path, F_OK) != 0) return;
  unlink(path);
  fprintf(stderr, "emu: SELECT pressed (short)\n");
  emul_selectShortPress();
}

/* health.c records why the RP restarted from the RP2040's watchdog, fault
 * handlers and SCB, none of which EmuMD has: a restart there is a new run. */
#include "health.h"

volatile uint32_t healthMaxFeedGapUs = 0;
volatile uint8_t healthMaxFeedGapPhase = 0;

void health_init(void) {}
void health_watchdogStart(void) {}
void health_feed(void) {}
void health_markReset(void) {}
void health_prepareJump(void) {}
void health_tick(void) {}
bool health_isCrashLoop(void) { return false; }
#if defined(_DEBUG) && (_DEBUG != 0)
void health_requestTest(health_test_t test) { (void)test; }
#endif
