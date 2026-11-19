/**
 * File: jobs.c
 * Author: Diego Parrilla Santamaría
 * Date: November 2026
 * Copyright: 2026 - GOODDATA LABS SL
 * Description: The job runner (jobs.h).
 */

#include "jobs.h"

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#define PROGRESS_DONE 100U
#define DUMMY_DEFAULT_SECONDS 10U
#define DUMMY_MAX_SECONDS 60U
#define MS_PER_SECOND 1000U

static JobStatus status;
static uint16_t params[4];
static uint16_t lastSeq;
static int seqValid;
static uint32_t startedMs;
static uint32_t steps;
static char result[JOBS_RESULT_BYTES];

static void resultLine(const char *fmt, ...) {
  if (status.resultLength >= JOBS_RESULT_BYTES - 1U) return;
  va_list args;
  va_start(args, fmt);
  int written = vsnprintf(&result[status.resultLength],
                          JOBS_RESULT_BYTES - status.resultLength, fmt, args);
  va_end(args);
  if (written < 0) return;
  uint32_t end = status.resultLength + (uint32_t)written;
  if (end > JOBS_RESULT_BYTES - 2U) end = JOBS_RESULT_BYTES - 2U;
  result[end++] = '\n';
  status.resultLength = (uint16_t)end;
}

static void finish(uint16_t state) {
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
  if (test != MDTEST_TEST_DUMMY) {
    *number = 0;
    return MDTEST_STATUS_UNKNOWN;
  }
  status.number++;
  if (status.number == 0) status.number = 1;
  status.test = test;
  status.state = MDTEST_JOB_RUNNING;
  status.progress = 0;
  status.resultLength = 0;
  memcpy(params, p, sizeof(params));
  lastSeq = seq;
  seqValid = 1;
  startedMs = nowMs;
  steps = 0;
  *number = status.number;
  return MDTEST_STATUS_OK;
}

void jobs_cancel(void) {
  if (status.state != MDTEST_JOB_RUNNING) return;
  resultLine("cancelled=yes");
  finish(MDTEST_JOB_CANCELLED);
}

void jobs_poll(uint32_t nowMs) {
  if (status.state != MDTEST_JOB_RUNNING) return;
  switch (status.test) {
    case MDTEST_TEST_DUMMY:
      dummyStep(nowMs);
      break;
    default:
      resultLine("error=unknown test");
      finish(MDTEST_JOB_FAILED);
      break;
  }
}

const JobStatus *jobs_status(void) { return &status; }

const char *jobs_result(void) { return result; }

void jobs_reset(void) {
  memset(&status, 0, sizeof(status));
  seqValid = 0;
  steps = 0;
}
