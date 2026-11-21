/**
 * File: reportsave.h
 * Author: Diego Parrilla Santamaría
 * Date: November 2026
 * Copyright: 2026 - GOODDATA LABS SL
 * Description: The program's report saved on the microSD card
 *              (MDTEST_TEST_REPORT), stepped by jobs.c: the card started, a
 *              new MDTEST-REPORT-nn.TXT opened, the text the program sends
 *              (MDTEST_CMD_REPORT_DATA, through channel.c) written as it
 *              comes, the file closed and the card stopped.
 */

#ifndef REPORTSAVE_H
#define REPORTSAVE_H

#include <stdint.h>

#include "mdtest.h"

/** @brief Begin saving a report. */
void reportsave_begin(uint32_t nowMs);

/**
 * @brief One step. Returns MDTEST_JOB_RUNNING, or the state the job ends in
 *        once its result lines are written; sets the progress and the live
 *        words.
 */
uint16_t reportsave_step(uint32_t nowMs, uint16_t *progress,
                         uint16_t live[MDTEST_JOB_LIVE_WORDS]);

/** @brief The job was cancelled: the file removed, the card stopped. */
void reportsave_abort(void);

/**
 * @brief Whether a chunk of this many bytes can be taken now (the job is
 *        ready and the last chunk has been written).
 */
int reportsave_ready(uint16_t bytes);

/** @brief Take a chunk: words two characters each; 0 bytes ends the text. */
void reportsave_take(const uint16_t *words, uint16_t bytes);

#endif  // REPORTSAVE_H
