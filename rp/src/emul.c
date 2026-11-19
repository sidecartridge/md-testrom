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
#include "build_id.h"
#include "gconfig.h"
#include "health.h"
#include "mdtest.h"
#include "tprotocol.h"

// Included in this file only: it holds the definitions.
#include "devhooks.h"  // Debug-only SWD mailbox

// The window, as the engine serves it.
static uint16_t *window;

// The last line-test verdict the ST sent, for the bench (swd.py reads it by
// symbol); the sequence number tells a new one from the last.
typedef struct {
  uint16_t words[MDTEST_VERDICT_WORDS];
  uint32_t count;
} LineVerdict;
volatile LineVerdict lineVerdict;
// Frames seen while ROM3 is data, and those with a bad checksum.
volatile uint32_t dataStateFrames;
volatile uint32_t dataStateChecksumErrors;

// While ROM3 is test data, frames are parsed but never answered: the
// template's dispatcher writes its answer into the window, over the data
// under test. The parser is fed by this loop alone until the command channel
// is selected, so it is never fed two streams.
static void onDataStateFrame(const TransmissionProtocol *frame) {
  dataStateFrames++;
  if (frame->command_id != MDTEST_CMD_LINE_VERDICT ||
      frame->payload_size < 4U + 2U * MDTEST_VERDICT_WORDS) {
    return;
  }
  const uint16_t *words = (const uint16_t *)frame->payload + 2;  // the token
  for (unsigned int i = 0; i < MDTEST_VERDICT_WORDS; i++) {
    lineVerdict.words[i] = words[i];
  }
  lineVerdict.count++;
  DPRINTF("Line test verdict %u: class %u, first $%04X%04X, errors %u, run %u\n",
          words[0], words[1], words[2], words[3], words[4], words[5]);
}

static void onDataStateChecksumError(const TransmissionProtocol *frame) {
  (void)frame;
  dataStateChecksumErrors++;
}

static void consumeDataStateSample(uint16_t sample) {
  tprotocol_parse((uint16_t)(sample ^ CHANDLER_ADDRESS_HIGH_BIT),
                  onDataStateFrame, onDataStateChecksumError);
}

#if (defined(_DEBUG) && (_DEBUG != 0)) || (defined(EMUMD) && EMUMD)
// Serve the window as a faulty bus would (debug builds, and EmuMD's bench).
static void serveFault(unsigned int kind, unsigned int a, unsigned int b) {
  mdtest_fillWindow(window, RELEASE_VERSION, RELEASE_BUILD_ID);
  if (mdtest_injectFault(window, kind, a, b) == 0) {
    DPRINTF("Serving fault %u (%u, %u)\n", kind, a, b);
  } else {
    DPRINTF("Fault %u (%u, %u) refused: window served whole\n", kind, a, b);
  }
}
#endif

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
    case DEVHOOKS_APP_FAULT:
      if (payloadSize < 6U) return 0;
      serveFault(payload[0], payload[1], payload[2]);
      return 1;
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

  // The window's contents (common/mdtest.h): the computed test pattern in
  // both banks and the header the ST program recognises the firmware by.
  // Written after chandler_init(), which seeds its random token and seed in
  // the window (at $FA2004): until the ST selects ROM3 as the command
  // channel, every word of the window but the header is test data.
  window = (uint16_t *)&__rom_in_ram_start__;
  mdtest_fillWindow(window, RELEASE_VERSION, RELEASE_BUILD_ID);
#if defined(_DEBUG) && (_DEBUG != 0)
  {
    unsigned int wrong = 0;
    for (uint32_t i = 0; i < MDTEST_WINDOW_WORDS; i++) {
      bool header = i >= MDTEST_HEADER_INDEX &&
                    i < MDTEST_HEADER_INDEX + MDTEST_HEADER_WORDS;
      if (!header && window[i] != mdtest_patternWord((uint16_t)i)) wrong++;
    }
    if (wrong) {
      DPRINTF("ERROR: %u words of the window are not the pattern\n", wrong);
    } else {
      DPRINTF("Test image in RAM verified (%u words)\n",
              (unsigned)MDTEST_WINDOW_WORDS);
    }
  }
#endif

#if defined(EMUMD) && EMUMD
  // EmuMD's bench: MDTEST_FAULT="KIND A B" in Hatari's environment serves a
  // faulty bus from boot (after the check above, which expects it whole).
  {
    const char *fault = getenv("MDTEST_FAULT");
    unsigned int kind = 0, a = 0, b = 0;
    if (fault && sscanf(fault, "%u %u %u", &kind, &a, &b) >= 1) {
      serveFault(kind, a, b);
    }
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
    // Drain the ROM3 capture on every pass. ROM3 is test data: frames are
    // parsed, never answered (onDataStateFrame()).
    commemul_poll(consumeDataStateSample);
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
