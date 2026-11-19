/**
 * File: jobs.h
 * Author: Diego Parrilla Santamaría
 * Date: November 2026
 * Copyright: 2026 - GOODDATA LABS SL
 * Description: Tests that take time, run as jobs: started by a command that
 *              is answered at once, stepped from the main loop, polled, and
 *              ended with a result of "key=value" lines. One at a time.
 *
 * Plain C, no SDK: the caller passes the time, so the host tests drive it.
 */

#ifndef JOBS_H
#define JOBS_H

#include <stdint.h>

#include "mdtest.h"

#define JOBS_RESULT_BYTES 2048U

typedef struct {
  uint16_t number;    // 0 before the first job
  uint16_t state;     // MDTEST_JOB_*
  uint16_t test;      // MDTEST_TEST_*
  uint16_t progress;  // 0 to 100
  uint16_t resultLength;
} JobStatus;

/**
 * @brief Start a test. Returns MDTEST_STATUS_OK with the new job's number,
 *        or the last one's when seq repeats the last start's (a start the ST
 *        sent again because it missed the answer); MDTEST_STATUS_BUSY with
 *        the running job's; MDTEST_STATUS_UNKNOWN for a test this build does
 *        not have.
 */
uint16_t jobs_start(uint16_t test, uint16_t seq, const uint16_t params[4],
                    uint32_t nowMs, uint16_t *number);

/** @brief Stop the running job, if any: its state becomes CANCELLED. */
void jobs_cancel(void);

/** @brief One bounded step of the running job, from the main loop. */
void jobs_poll(uint32_t nowMs);

const JobStatus *jobs_status(void);

/** @brief The ended job's result: resultLength bytes, no terminator. */
const char *jobs_result(void);

/** @brief Forget every job (an RP start, or a host test). */
void jobs_reset(void);

#endif  // JOBS_H
