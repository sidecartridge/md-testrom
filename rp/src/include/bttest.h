/**
 * File: bttest.h
 * Author: Diego Parrilla Santamaría
 * Date: November 2026
 * Copyright: 2026 - GOODDATA LABS SL
 * Description: The Bluetooth test (MDTEST_TEST_BLUETOOTH), stepped by
 *              jobs.c over bthw.h: the controller started, a classic
 *              inquiry, a low-energy scan, the devices heard listed. The
 *              chip is powered down when it ends.
 */

#ifndef BTTEST_H
#define BTTEST_H

#include <stdint.h>

#include "mdtest.h"

/** @brief Begin the Bluetooth test. */
void bttest_begin(uint32_t nowMs);

/**
 * @brief One step. Returns MDTEST_JOB_RUNNING, or the state the job ends in
 *        once its result lines are written; sets the progress and the live
 *        words.
 */
uint16_t bttest_step(uint32_t nowMs, uint16_t *progress,
                     uint16_t live[MDTEST_JOB_LIVE_WORDS]);

/** @brief The job was cancelled: the scan stopped, the chip down. */
void bttest_abort(void);

#endif  // BTTEST_H
