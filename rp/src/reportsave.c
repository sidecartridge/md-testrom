/**
 * File: reportsave.c
 * Author: Diego Parrilla Santamaría
 * Date: November 2026
 * Copyright: 2026 - GOODDATA LABS SL
 * Description: The report on the microSD card (reportsave.h). A chunk the
 *              program sends is copied as the command is answered and
 *              written by the next step, so no command waits for the card.
 */

#include "reportsave.h"

#include <stdbool.h>
#include <stdio.h>

#include "cardhw.h"
#include "debug.h"
#include "ff.h"
#include "jobs.h"

#define DRIVE "0:"
#define NAME_BYTES 32U
#define PROGRESS_DONE 100U
#define BYTES_PER_KB 1024U
#define BITS_PER_BYTE 8U
// The program sends the next chunk within milliseconds; a pause this long
// means it has gone.
#define DATA_WAIT_MS 30000U

typedef enum {
  STEP_START,
  STEP_OPEN,
  STEP_READY,
  STEP_CLOSE,
  STEP_END,
} Step;

static Step step;
static uint16_t endState;
static uint32_t waitedFrom;
static FATFS reportFs;
static FIL file;
static bool mounted;
static bool fileOpen;
static char name[NAME_BYTES];
static uint32_t written;

static uint8_t chunk[MDTEST_REPORT_CHUNK_BYTES];
static volatile uint16_t chunkBytes;
static volatile bool chunkFull;
static volatile bool ended;

static void fail(const char *error, FRESULT result) {
  jobs_resultLine("error=%s", error);
  if (result != FR_OK) jobs_resultLine("fs_error=%d", (int)result);
  endState = MDTEST_JOB_FAILED;
  step = STEP_END;
}

static void stopAll(bool keepFile) {
  if (fileOpen) {
    f_close(&file);
    fileOpen = false;
    if (!keepFile) f_unlink(name);
  }
  if (mounted) {
    f_unmount(DRIVE);
    mounted = false;
  }
  cardhw_stop();
}

void reportsave_begin(uint32_t nowMs) {
  step = STEP_START;
  endState = MDTEST_JOB_FAILED;
  waitedFrom = nowMs;
  mounted = false;
  fileOpen = false;
  name[0] = '\0';
  written = 0;
  chunkBytes = 0;
  chunkFull = false;
  ended = false;
  jobs_resultLine("test=report");
}

int reportsave_ready(uint16_t bytes) {
  return step == STEP_READY && !chunkFull && !ended &&
         bytes <= MDTEST_REPORT_CHUNK_BYTES;
}

void reportsave_take(const uint16_t *words, uint16_t bytes) {
  if (bytes == 0) {
    ended = true;
    return;
  }
  for (uint16_t i = 0; i < bytes; i++) {
    uint16_t word = words[i / 2U];
    chunk[i] = (uint8_t)((i & 1U) ? word : (word >> BITS_PER_BYTE));
  }
  chunkBytes = bytes;
  chunkFull = true;
}

static void startStep(void) {
  CardHwInfo info;
  int started = cardhw_start(&info);
  if (started != CARDHW_OK) {
    fail(started == CARDHW_NO_ANSWER ? "no card" : "card failed", FR_OK);
    return;
  }
  FRESULT result = f_mount(&reportFs, DRIVE, 1);
  if (result != FR_OK) {
    fail("no filesystem", result);
    return;
  }
  mounted = true;
  step = STEP_OPEN;
}

static void openStep(uint32_t nowMs) {
  FILINFO fno;
  for (unsigned number = 0; number < MDTEST_REPORT_FILES; number++) {
    snprintf(name, sizeof(name), MDTEST_REPORT_NAME, number);
    if (f_stat(name, &fno) == FR_NO_FILE) {
      FRESULT result = f_open(&file, name, FA_CREATE_NEW | FA_WRITE);
      if (result != FR_OK) {
        name[0] = '\0';
        fail(result == FR_WRITE_PROTECTED ? "write protected" : "cannot create",
             result);
        return;
      }
      fileOpen = true;
      waitedFrom = nowMs;
      step = STEP_READY;
      DPRINTF("Report: writing %s\n", name);
      return;
    }
  }
  name[0] = '\0';
  fail("no free name", FR_OK);
}

static void readyStep(uint32_t nowMs) {
  if (chunkFull) {
    UINT done = 0;
    FRESULT result = f_write(&file, chunk, chunkBytes, &done);
    if (result != FR_OK || done != chunkBytes) {
      fail(result == FR_OK ? "card full" : "write failed", result);
      return;
    }
    written += done;
    chunkFull = false;
    waitedFrom = nowMs;
    return;
  }
  if (ended) {
    step = STEP_CLOSE;
    return;
  }
  if (nowMs - waitedFrom > DATA_WAIT_MS) fail("no text", FR_OK);
}

static void closeStep(void) {
  FRESULT result = f_close(&file);
  fileOpen = false;
  if (result != FR_OK) {
    f_unlink(name);
    fail("write failed", result);
    return;
  }
  jobs_resultLine("file=%s", name);
  jobs_resultLine("bytes=%lu", (unsigned long)written);
  endState = MDTEST_JOB_DONE;
  step = STEP_END;
}

uint16_t reportsave_step(uint32_t nowMs, uint16_t *progress,
                         uint16_t live[MDTEST_JOB_LIVE_WORDS]) {
  switch (step) {
    case STEP_START:
      startStep();
      break;
    case STEP_OPEN:
      openStep(nowMs);
      break;
    case STEP_READY:
      readyStep(nowMs);
      break;
    case STEP_CLOSE:
      closeStep();
      break;
    case STEP_END:
    default:
      // A failure leaves no half-written file behind.
      stopAll(endState == MDTEST_JOB_DONE);
      *progress = PROGRESS_DONE;
      return endState;
  }
  live[0] = MDTEST_REPORT_PHASE_START;
  if (step == STEP_READY) live[0] = MDTEST_REPORT_PHASE_READY;
  if (step >= STEP_CLOSE) live[0] = MDTEST_REPORT_PHASE_CLOSE;
  live[1] = (uint16_t)(written / BYTES_PER_KB);
  return MDTEST_JOB_RUNNING;
}

void reportsave_abort(void) {
  stopAll(false);
  step = STEP_END;
}
