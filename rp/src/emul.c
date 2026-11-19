/**
 * File: emul.c
 * Author: Diego Parrilla Santamaría
 * Date: February 2025, November 2026
 * Copyright: 2025-2026 - GOODDATA LABS
 * Description: The Multidevice Test's application. Headless: it draws nothing
 *              and puts no code on the ST. It serves the 128 KB test window
 *              (ROM4 and ROM3), captures the ST's ROM3 reads for the command
 *              channel, and watches SELECT.
 */

#include "emul.h"

#include "chandler.h"
#include "commemul.h"
#include "gconfig.h"
#include "health.h"

// Included in this file only: it holds the definitions.
#include "devhooks.h"         // Debug-only SWD mailbox
#include "target_firmware.h"  // The test image the ST reads

// Set by a short SELECT press, handled by the main loop.
static volatile bool leaveForBooster = false;

void emul_selectShortPress(void) { leaveForBooster = true; }

// Nothing of this app's runs past a reset or the jump to Booster: the bus
// engine and the command ring stop first (the jump itself masks and clears
// the interrupts, reset.h).
static void emul_quiesce(void) {
  romemul_stop();
  commemul_stop();
}

static void emul_quiesceAndFactoryReset(void) {
  emul_quiesce();
  reset_deviceAndEraseFlash();
}

#if defined(_DEBUG) && (_DEBUG != 0)
// Heap held on request by `swd.py app heap_hold`, to test running out of it.
typedef struct DevhooksHeldBlock {
  struct DevhooksHeldBlock *next;
} DevhooksHeldBlock;
static DevhooksHeldBlock *devhooksHeldHeap = NULL;

// App commands sent over SWD by tools/dev/swd.py (see emul.h).
static uint32_t emul_devhooksApp(uint16_t commandId, const uint16_t *payload,
                                 uint16_t payloadSize) {
  switch (commandId) {
    case DEVHOOKS_APP_HEAP_HOLD: {
      uint32_t holdKb = (payloadSize >= 2U) ? payload[0] : 0U;
      if (holdKb == 0U) {
        while (devhooksHeldHeap != NULL) {
          DevhooksHeldBlock *next = devhooksHeldHeap->next;
          free(devhooksHeldHeap);
          devhooksHeldHeap = next;
        }
        DPRINTF("devhooks: heap hold released\n");
        return 1;
      }
      DevhooksHeldBlock *block =
          malloc(sizeof(DevhooksHeldBlock) + (holdKb * 1024U));
      if (block != NULL) {
        block->next = devhooksHeldHeap;
        devhooksHeldHeap = block;
      }
      DPRINTF("devhooks: holding %lu KB more heap: %s\n", (unsigned long)holdKb,
              (block != NULL) ? "ok" : "refused");
      return (block != NULL) ? 1U : 0U;
    }
    case DEVHOOKS_APP_HEALTH:
      if (payloadSize < 2U || payload[0] == HEALTH_TEST_NONE ||
          payload[0] > HEALTH_TEST_STACK_OVERFLOW) {
        return 0;
      }
      health_requestTest((health_test_t)payload[0]);
      return 1;
    default:
      return 0;
  }
}
#endif

void emul_start() {
  // Why the RP started, then the watchdog: from here a hang reboots the RP
  // and says where it was.
  health_init();
  health_watchdogStart();

  // The ROM3 capture (PIO + DMA ring) beside the engine, and the command
  // handler that polls the ring and parses the protocol. ROM3 reads are the
  // ST's commands as well as test data.
  if (commemul_init() < 0) {
    panic("commemul_init failed: PIO/DMA claim or program load returned <0");
  }
  chandler_init();

  // The test image, copied after chandler_init(), which seeds its random
  // token and seed in the window (at $FA2004): until the ST selects ROM3 as
  // the command channel, every word of the window is test data. ROM4 is the
  // lower 64 KB of the window, ROM3 the upper 64 KB. A plain copy rather than
  // the XIP-stream DMA one: it is a few milliseconds, once, and it is the same
  // copy under EmuMD, which has no XIP stream.
  ERASE_FIRMWARE_IN_RAM();
  COPY_FIRMWARE_TO_RAM_MEMCPY(target_firmware, target_firmware_length);
#if defined(_DEBUG) && (_DEBUG != 0)
  // The ST must see exactly the generated image.
  if (memcmp((const void *)&__rom_in_ram_start__, target_firmware,
             (size_t)target_firmware_length * sizeof(uint16_t)) != 0) {
    DPRINTF("ERROR: test image in RAM does not match target_firmware\n");
  } else {
    DPRINTF("Test image in RAM verified (%u words)\n",
            (unsigned)target_firmware_length);
  }
#endif

  // Only now does the ST see the window: both banks served by chained DMAs
  // feeding the PIO, no CPU involved. Without the engine the ST reads
  // nothing, so a failure here is fatal.
  if (romemul_initTwoBanks(false) < 0) {
    panic("romemul_initTwoBanks failed: PIO/DMA claim returned <0");
  }

  // SELECT: a short press leaves for Booster; a press held for
  // SELECT_LONG_RESET is a factory reset.
  select_configure();
  select_setResetCallback(emul_selectShortPress);
  select_setLongResetCallback(emul_quiesceAndFactoryReset);

  // The LED stays off, as in v0.0.3 (whose blink_on() was never compiled in):
  // on a Pico W it belongs to the radio chip, which nothing starts at boot.

#if defined(_DEBUG) && (_DEBUG != 0)
  // Debug builds only: serve the SWD mailbox of tools/dev/swd.py
  devhooks_setAppHandler(emul_devhooksApp);
#endif

  DPRINTF("Start the app loop here\n");
  while (!leaveForBooster) {
    health_feed();
    health_setPhase(HEALTH_PHASE_MAIN_LOOP);
    devhooks_poll();
    select_poll();
    // Drain the ROM3 command ring and dispatch on every pass: the ST spins
    // on its answer, so the loop never waits.
    chandler_loop();
    // Heap sampling, the debug summary and the debug test hooks
    health_tick();
  }

  // Booster takes over at its next boot.
  DPRINTF("SELECT pressed: jumping to the booster app...\n");
  emul_quiesce();
  settings_put_string(gconfig_getContext(), PARAM_BOOT_FEATURE, "BOOSTER");
  settings_save(gconfig_getContext(), true);
  health_prepareJump();
  reset_jump_to_booster();
}
