/**
 * File: jobs.c
 * Author: Diego Parrilla Santamaría
 * Date: November 2026
 * Copyright: 2026 - GOODDATA LABS SL
 * Description: The job runner (jobs.h).
 */

#include "jobs.h"

#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#include "cardtest.h"
#include "devtests.h"
#include "reportsave.h"
#include "bttest.h"
#include "wifitest.h"

#define PROGRESS_DONE 100U
#define DUMMY_DEFAULT_SECONDS 10U
#define DUMMY_MAX_SECONDS 60U
#define MS_PER_SECOND 1000U
#define SELECT_DEBOUNCE_MS 30U
#define LIVE_MS_MAX 0xFFFFU
// After the SELECT test ends, how long a short press is still its: the test
// and select.c debounce the release apart, and either may see it first.
#define SELECT_CLAIM_MS 500U

static JobStatus status;
static uint16_t params[4];
static uint16_t lastSeq;
static int seqValid;
static uint32_t startedMs;
static uint32_t steps;
static uint32_t lastPollMs;
static char result[JOBS_RESULT_BYTES];

// The SELECT test's view of the button: the raw level and since when, the
// debounced level, and whether that level is a press that began before the
// test (not counted: its start was not seen).
static struct {
  bool raw;
  uint32_t rawSince;
  bool level;
  bool fromStart;
  uint32_t pressStart;
} button;

// When the SELECT test ended (selectEnded: it has).
static bool selectEnded;
static uint32_t selectEndedMs;

// The LED test's blink.
static bool ledLit;
static uint32_t ledToggledMs;
static uint32_t blinks;

static void resultLineV(const char *fmt, va_list args) {
  if (status.resultLength >= JOBS_RESULT_BYTES - 1U) return;
  int written = vsnprintf(&result[status.resultLength],
                          JOBS_RESULT_BYTES - status.resultLength, fmt, args);
  if (written < 0) return;
  uint32_t end = status.resultLength + (uint32_t)written;
  if (end > JOBS_RESULT_BYTES - 2U) end = JOBS_RESULT_BYTES - 2U;
  result[end++] = '\n';
  status.resultLength = (uint16_t)end;
}

static void resultLine(const char *fmt, ...) {
  va_list args;
  va_start(args, fmt);
  resultLineV(fmt, args);
  va_end(args);
}

void jobs_resultLine(const char *fmt, ...) {
  va_list args;
  va_start(args, fmt);
  resultLineV(fmt, args);
  va_end(args);
}

static void finish(uint16_t state) {
  if (status.test == MDTEST_TEST_SELECT) {
    selectEnded = true;
    selectEndedMs = lastPollMs;
  }
  status.state = state;
  if (state == MDTEST_JOB_DONE) status.progress = PROGRESS_DONE;
}

// The dummy test: waits its parameter's seconds (1 to 60, 10 if 0), counting
// the steps it was given, so the bench measures how a job shares the loop.
static void dummyStep(uint32_t nowMs) {
  uint32_t seconds = params[0] ? params[0] : DUMMY_DEFAULT_SECONDS;
  if (seconds > DUMMY_MAX_SECONDS) seconds = DUMMY_MAX_SECONDS;
  uint32_t total = seconds * MS_PER_SECOND;
  uint32_t elapsed = nowMs - startedMs;
  steps++;
  status.progress = (uint16_t)(elapsed >= total
                                   ? PROGRESS_DONE
                                   : elapsed * PROGRESS_DONE / total);
  if (elapsed >= total) {
    resultLine("test=dummy");
    resultLine("seconds=%lu", (unsigned long)seconds);
    resultLine("steps=%lu", (unsigned long)steps);
    finish(MDTEST_JOB_DONE);
  }
}

static uint32_t capSeconds(uint16_t value, uint32_t byDefault, uint32_t max) {
  uint32_t seconds = value ? value : byDefault;
  return seconds > max ? max : seconds;
}

// SELECT: done at the first press and release, failed with none in time or
// with a button that reads pressed from the start for MDTEST_SELECT_STUCK_MS.
static void selectStep(uint32_t nowMs) {
  uint32_t total =
      capSeconds(params[0], MDTEST_SELECT_SECONDS, MDTEST_SELECT_MAX_SECONDS) *
      MS_PER_SECOND;
  uint32_t elapsed = nowMs - startedMs;
  bool raw = devtests_selectLevel();
  if (steps++ == 0) {
    button.raw = raw;
    button.rawSince = nowMs;
    button.level = raw;
    button.fromStart = raw;
    button.pressStart = nowMs;
  }
  if (raw != button.raw) {
    button.raw = raw;
    button.rawSince = nowMs;
  }
  if (button.raw != button.level &&
      nowMs - button.rawSince >= SELECT_DEBOUNCE_MS) {
    button.level = button.raw;
    if (button.level) {
      button.pressStart = button.rawSince;
    } else if (button.fromStart) {
      button.fromStart = false;
    } else {
      status.live[2]++;
      status.live[0] = 0;
      status.live[1] = 0;
      resultLine("test=select");
      resultLine("presses=%u", (unsigned)status.live[2]);
      resultLine("press_ms=%lu",
                 (unsigned long)(button.rawSince - button.pressStart));
      finish(MDTEST_JOB_DONE);
      return;
    }
  }
  uint32_t held = button.level ? nowMs - button.pressStart : 0U;
  status.live[0] = button.level ? 1U : 0U;
  status.live[1] = (uint16_t)(held > LIVE_MS_MAX ? LIVE_MS_MAX : held);
  if (button.level && button.fromStart && held >= MDTEST_SELECT_STUCK_MS) {
    resultLine("test=select");
    resultLine("error=pressed from the start");
    resultLine("held_ms=%lu", (unsigned long)held);
    finish(MDTEST_JOB_FAILED);
    return;
  }
  status.progress = (uint16_t)(elapsed >= total
                                   ? PROGRESS_DONE
                                   : elapsed * PROGRESS_DONE / total);
  if (elapsed >= total) {
    resultLine("test=select");
    resultLine("presses=0");
    resultLine(button.level ? "error=still pressed" : "error=no press");
    finish(MDTEST_JOB_FAILED);
  }
}

static bool ledSet(bool on) {
  int answer = devtests_ledSet(on);
  status.live[0] = on ? 1U : 0U;
  if (answer == 0) return true;
  resultLine(answer > 0 ? "error=led reads back wrong"
                        : "error=chip did not answer");
  resultLine("led_error=%d", answer);
  finish(MDTEST_JOB_FAILED);
  return false;
}

// The LED: the chip brought up the first time, then the mode; a blink runs
// until cancelled or MDTEST_LED_BLINK_MAX_S.
static void ledStep(uint32_t nowMs) {
  if (steps++ == 0) {
    resultLine("test=led");
    if (params[0] == MDTEST_LED_RELEASE) {
      devtests_ledRelease();
      status.live[0] = 0;
      resultLine("led=off");
      resultLine("chip=off");
      finish(MDTEST_JOB_DONE);
      return;
    }
    if (params[0] > MDTEST_LED_BLINK) {
      resultLine("error=bad mode");
      finish(MDTEST_JOB_FAILED);
      return;
    }
    uint32_t startMs = 0;
    int32_t heapBytes = -1;
    int up = devtests_ledChipUp(&startMs, &heapBytes);
    if (up < 0) {
      resultLine("chip=no answer");
      resultLine("chip_error=%d", up);
      finish(MDTEST_JOB_FAILED);
      return;
    }
    if (up > 0) {
      resultLine("chip=started");
      resultLine("chip_start_ms=%lu", (unsigned long)startMs);
      if (heapBytes >= 0) resultLine("chip_heap_bytes=%ld", (long)heapBytes);
    } else {
      resultLine("chip=up");
    }
    if (params[0] == MDTEST_LED_BLINK) {
      ledLit = true;
      ledToggledMs = nowMs;
      blinks = 0;
      if (ledSet(true)) resultLine("led=blink");
      return;
    }
    if (ledSet(params[0] == MDTEST_LED_ON)) {
      resultLine(params[0] == MDTEST_LED_ON ? "led=on" : "led=off");
      finish(MDTEST_JOB_DONE);
    }
    return;
  }
  uint32_t elapsed = nowMs - startedMs;
  uint32_t total = MDTEST_LED_BLINK_MAX_S * MS_PER_SECOND;
  if (nowMs - ledToggledMs >= MDTEST_LED_BLINK_MS) {
    ledLit = !ledLit;
    ledToggledMs = nowMs;
    blinks++;
    if (!ledSet(ledLit)) return;
  }
  status.progress = (uint16_t)(elapsed >= total
                                   ? PROGRESS_DONE
                                   : elapsed * PROGRESS_DONE / total);
  if (elapsed >= total) {
    resultLine("blinks=%lu", (unsigned long)blinks);
    finish(MDTEST_JOB_DONE);
  }
}

uint16_t jobs_start(uint16_t test, uint16_t seq, const uint16_t p[4],
                    uint32_t nowMs, uint16_t *number) {
  if (seqValid && seq == lastSeq && status.number != 0) {
    *number = status.number;
    return MDTEST_STATUS_OK;
  }
  if (status.state == MDTEST_JOB_RUNNING) {
    *number = status.number;
    return MDTEST_STATUS_BUSY;
  }
  if (test < MDTEST_TEST_DUMMY || test > MDTEST_TEST_LAST) {
    *number = 0;
    return MDTEST_STATUS_UNKNOWN;
  }
  status.number++;
  if (status.number == 0) status.number = 1;
  status.test = test;
  status.state = MDTEST_JOB_RUNNING;
  status.progress = 0;
  status.resultLength = 0;
  memset(status.live, 0, sizeof(status.live));
  memcpy(params, p, sizeof(params));
  lastSeq = seq;
  seqValid = 1;
  startedMs = nowMs;
  steps = 0;
  if (test == MDTEST_TEST_SD_CARD || test == MDTEST_TEST_SD_SWAP) {
    cardtest_begin(test, params, nowMs);
  }
  if (test == MDTEST_TEST_WIFI) wifitest_begin(nowMs);
  if (test == MDTEST_TEST_BLUETOOTH) bttest_begin(nowMs);
  if (test == MDTEST_TEST_REPORT) reportsave_begin(nowMs);
  *number = status.number;
  return MDTEST_STATUS_OK;
}

void jobs_cancel(void) {
  if (status.state != MDTEST_JOB_RUNNING) return;
  if (status.test == MDTEST_TEST_SD_CARD ||
      status.test == MDTEST_TEST_SD_SWAP) {
    cardtest_abort();
  }
  if (status.test == MDTEST_TEST_WIFI) wifitest_abort();
  if (status.test == MDTEST_TEST_BLUETOOTH) bttest_abort();
  if (status.test == MDTEST_TEST_REPORT) reportsave_abort();
  resultLine("cancelled=yes");
  finish(MDTEST_JOB_CANCELLED);
}

void jobs_poll(uint32_t nowMs) {
  lastPollMs = nowMs;
  if (status.state != MDTEST_JOB_RUNNING) return;
  switch (status.test) {
    case MDTEST_TEST_DUMMY:
      dummyStep(nowMs);
      break;
    case MDTEST_TEST_DEVICE_INFO:
      resultLine("test=device");
      devtests_info();
      finish(MDTEST_JOB_DONE);
      break;
    case MDTEST_TEST_SELECT:
      selectStep(nowMs);
      break;
    case MDTEST_TEST_LED:
      ledStep(nowMs);
      break;
    case MDTEST_TEST_SD_CARD:
    case MDTEST_TEST_SD_SWAP: {
      uint16_t state = cardtest_step(nowMs, &status.progress, status.live);
      if (state != MDTEST_JOB_RUNNING) finish(state);
      break;
    }
    case MDTEST_TEST_WIFI: {
      uint16_t state = wifitest_step(nowMs, &status.progress, status.live);
      if (state != MDTEST_JOB_RUNNING) finish(state);
      break;
    }
    case MDTEST_TEST_BLUETOOTH: {
      uint16_t state = bttest_step(nowMs, &status.progress, status.live);
      if (state != MDTEST_JOB_RUNNING) finish(state);
      break;
    }
    case MDTEST_TEST_REPORT: {
      uint16_t state = reportsave_step(nowMs, &status.progress, status.live);
      if (state != MDTEST_JOB_RUNNING) finish(state);
      break;
    }
    default:
      resultLine("error=unknown test");
      finish(MDTEST_JOB_FAILED);
      break;
  }
}

const JobStatus *jobs_status(void) { return &status; }

const char *jobs_result(void) { return result; }

int jobs_selectOwnsPress(uint32_t nowMs) {
  if (status.state == MDTEST_JOB_RUNNING && status.test == MDTEST_TEST_SELECT) {
    return 1;
  }
  return selectEnded && nowMs - selectEndedMs < SELECT_CLAIM_MS;
}

void jobs_reset(void) {
  memset(&status, 0, sizeof(status));
  seqValid = 0;
  selectEnded = false;
  steps = 0;
}
