/**
 * File: bttest.c
 * Author: Diego Parrilla Santamaría
 * Date: November 2026
 * Copyright: 2026 - GOODDATA LABS SL
 * Description: The Bluetooth test (bttest.h): the controller started, a
 *              classic inquiry and a low-energy scan, each device kept once
 *              (its strongest signal, its name when one came), the
 *              strongest listed. Nothing is paired or written.
 */

#include "bttest.h"

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "bthw.h"
#include "jobs.h"
#include "pico/stdlib.h"

#define US_PER_MS 1000U
#define MS_PER_SECOND 1000U
#define PROGRESS_DONE 100U
#define START_MAX_MS 5000U   // the controller powered on and working
#define SCAN_SLACK_MS 3000U  // an inquiry past its length: stopped
#define HEARD_MAX 48U        // devices kept, by address
#define NO_RSSI (-128)
#define ADDRESS_TEXT_BYTES 18U  // "XX:XX:XX:XX:XX:XX"
#define RSSI_TEXT_BYTES 8U

typedef enum {
  STEP_CHIP,
  STEP_START,
  STEP_CLASSIC_START,
  STEP_CLASSIC,
  STEP_LE_START,
  STEP_LE,
  STEP_END,
} Step;

static Step step;
static uint16_t phase;
static uint16_t endState;
static uint32_t phaseMs;
static uint32_t startUs;

static BtHwDevice heard[HEARD_MAX];
static uint16_t heardCount;   // kept, at most HEARD_MAX
static uint16_t classicCount;
static uint16_t leCount;
static bool heardFull;        // more were heard than HEARD_MAX

static uint32_t since(uint32_t nowMs, uint32_t thenMs) { return nowMs - thenMs; }

static void enterPhase(uint16_t which, uint32_t nowMs) {
  phase = which;
  phaseMs = nowMs;
}

static void fail(const char *error) {
  jobs_resultLine("error=%s", error);
  endState = MDTEST_JOB_FAILED;
  step = STEP_END;
}

// A name as text for a result line: what is not printable as '?'.
static void printable(char *name) {
  for (char *at = name; *at; at++) {
    if (*at < ' ' || *at > '~') *at = '?';
  }
}

static void deviceSeen(const BtHwDevice *device) {
  for (unsigned i = 0; i < heardCount; i++) {
    BtHwDevice *known = &heard[i];
    if (memcmp(known->address, device->address, BTHW_ADDRESS_BYTES) != 0) {
      continue;
    }
    if (device->rssi != 0 && (known->rssi == 0 || device->rssi > known->rssi)) {
      known->rssi = device->rssi;
    }
    if (known->name[0] == '\0' && device->name[0] != '\0') {
      memcpy(known->name, device->name, sizeof(known->name));
    }
    return;
  }
  // Once the table is full a new address cannot be told from one heard
  // before: counted no more, said instead.
  if (heardCount == HEARD_MAX) {
    heardFull = true;
    return;
  }
  if (device->kind == BTHW_KIND_CLASSIC) {
    classicCount++;
  } else {
    leCount++;
  }
  heard[heardCount++] = *device;
}

static int signalOf(const BtHwDevice *device) {
  return device->rssi != 0 ? device->rssi : NO_RSSI;
}

static int strongerFirst(const void *one, const void *other) {
  return signalOf((const BtHwDevice *)other) - signalOf((const BtHwDevice *)one);
}

static const char *addressText(const uint8_t *address) {
  static char text[ADDRESS_TEXT_BYTES];
  char *out = text;
  for (unsigned i = 0; i < BTHW_ADDRESS_BYTES; i++) {
    out += snprintf(out, (size_t)(text + sizeof(text) - out),
                    i == 0U ? "%02X" : ":%02X", address[i]);
  }
  return text;
}

static const char *versionText(uint8_t hci) {
  static const char *const versions[] = {"1.0b", "1.1", "1.2", "2.0", "2.1",
                                         "3.0",  "4.0", "4.1", "4.2", "5.0",
                                         "5.1",  "5.2", "5.3", "5.4", "6.0"};
  return hci < sizeof(versions) / sizeof(versions[0]) ? versions[hci] : "?";
}

static void startedLines(uint32_t nowMs) {
  BtHwInfo info;
  bthw_info(&info);
  jobs_resultLine("bt=ok");
  jobs_resultLine("start_ms=%lu", (unsigned long)((time_us_32() - startUs) / US_PER_MS));
  jobs_resultLine("address=%s", addressText(info.address));
  jobs_resultLine("version=%s", versionText(info.hciVersion));
  jobs_resultLine("firmware=%04X", info.lmpSubversion);
  jobs_resultLine("manufacturer=%u", info.manufacturer);
  (void)nowMs;
}

static void deviceLines(void) {
  qsort(heard, heardCount, sizeof(heard[0]), strongerFirst);
  jobs_resultLine("devices=%u", classicCount + leCount);
  jobs_resultLine("classic=%u", classicCount);
  jobs_resultLine("le=%u", leCount);
  if (heardFull) jobs_resultLine("devices_more=yes");
  for (unsigned i = 0; i < heardCount && i < MDTEST_BT_DEVICES_LISTED; i++) {
    BtHwDevice *dev = &heard[i];
    printable(dev->name);
    char rssi[RSSI_TEXT_BYTES];
    if (dev->rssi != 0) {
      snprintf(rssi, sizeof(rssi), "%d", dev->rssi);
    } else {
      snprintf(rssi, sizeof(rssi), "?");
    }
    jobs_resultLine("dev=%s|%s|%s|%s", rssi,
                    dev->kind == BTHW_KIND_CLASSIC ? "classic" : "LE",
                    addressText(dev->address), dev->name);
  }
}

void bttest_begin(uint32_t nowMs) {
  step = STEP_CHIP;
  endState = MDTEST_JOB_FAILED;
  enterPhase(MDTEST_BT_PHASE_CHIP, nowMs);
  heardCount = 0;
  classicCount = 0;
  leCount = 0;
  heardFull = false;
  jobs_resultLine("test=bluetooth");
}

static void chipStep(uint32_t nowMs) {
  startUs = time_us_32();
  int answer = bthw_start(deviceSeen);
  if (answer != 0) {
    jobs_resultLine("bt=no answer");
    jobs_resultLine("chip_error=%d", answer);
    fail("chip silent");
    return;
  }
  enterPhase(MDTEST_BT_PHASE_START, nowMs);
  step = STEP_START;
}

static void startStep(uint32_t nowMs) {
  int state = bthw_state();
  if (state == BTHW_WORKING) {
    startedLines(nowMs);
    enterPhase(MDTEST_BT_PHASE_CLASSIC, nowMs);
    step = STEP_CLASSIC_START;
  } else if (state == BTHW_FAILED) {
    fail("controller failed");
  } else if (since(nowMs, phaseMs) > START_MAX_MS) {
    fail("controller silent");
  }
}

static void scanStart(bool classic, uint32_t nowMs) {
  uint32_t seconds = classic ? MDTEST_BT_CLASSIC_SECONDS : MDTEST_BT_LE_SECONDS;
  int answer = bthw_scan(classic, seconds);
  if (answer != 0) {
    jobs_resultLine("scan_error=%d", answer);
    fail(classic ? "inquiry failed" : "le scan failed");
    return;
  }
  phaseMs = nowMs;
  step = classic ? STEP_CLASSIC : STEP_LE;
}

uint16_t bttest_step(uint32_t nowMs, uint16_t *progress,
                     uint16_t live[MDTEST_JOB_LIVE_WORDS]) {
  switch (step) {
    case STEP_CHIP:
      chipStep(nowMs);
      break;
    case STEP_START:
      startStep(nowMs);
      break;
    case STEP_CLASSIC_START:
      scanStart(true, nowMs);
      break;
    case STEP_CLASSIC:
      // The inquiry ends by itself; one past its length is stopped.
      if (!bthw_scanning() ||
          since(nowMs, phaseMs) > (MDTEST_BT_CLASSIC_SECONDS * MS_PER_SECOND) +
                                      SCAN_SLACK_MS) {
        bthw_scanStop();
        enterPhase(MDTEST_BT_PHASE_LE, nowMs);
        step = STEP_LE_START;
      }
      break;
    case STEP_LE_START:
      scanStart(false, nowMs);
      break;
    case STEP_LE:
      (void)bthw_scanning();  // events serviced
      if (since(nowMs, phaseMs) >= MDTEST_BT_LE_SECONDS * MS_PER_SECOND) {
        bthw_scanStop();
        deviceLines();
        endState = MDTEST_JOB_DONE;
        step = STEP_END;
      }
      break;
    case STEP_END:
    default:
      bthw_stop();
      *progress = PROGRESS_DONE;
      return endState;
  }
  *progress = (uint16_t)(phase * PROGRESS_DONE / MDTEST_BT_PHASES);
  live[0] = phase;
  live[1] = (uint16_t)(classicCount + leCount);
  live[2] = (uint16_t)(since(nowMs, phaseMs) / MS_PER_SECOND);
  return MDTEST_JOB_RUNNING;
}

void bttest_abort(void) {
  bthw_stop();
  step = STEP_END;
}
