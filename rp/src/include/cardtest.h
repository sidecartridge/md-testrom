/**
 * File: cardtest.h
 * Author: Diego Parrilla Santamaría
 * Date: November 2026
 * Copyright: 2026 - GOODDATA LABS SL
 * Description: The microSD card's tests, stepped by jobs.c: the card and its
 *              filesystem described, one temporary file written, read back
 *              and deleted (MDTEST_TEST_SD_CARD), and the card taken out and
 *              put back (MDTEST_TEST_SD_SWAP). Each step is short; the card
 *              is started by the test and stopped when it ends.
 */

#ifndef CARDTEST_H
#define CARDTEST_H

#include <stdbool.h>
#include <stdint.h>

#include "mdtest.h"

// Debug and EmuMD builds: a card that fails as asked (swd.py app card_fault,
// MDTEST_CARD_FAULT).
#define CARDTEST_FAULT_NONE 0U
#define CARDTEST_FAULT_SILENT 1U  // no card answers
#define CARDTEST_FAULT_FULL 2U    // no free space
#define CARDTEST_FAULT_WRITE 3U   // a write fails halfway
#define CARDTEST_FAULT_READ 4U    // a byte read back differs

#if (defined(_DEBUG) && (_DEBUG != 0)) || (defined(EMUMD) && EMUMD)
extern volatile uint32_t cardtestFault;
#endif

/** @brief Begin a card test (MDTEST_TEST_SD_*) with the job's parameters. */
void cardtest_begin(uint16_t test, const uint16_t params[4], uint32_t nowMs);

/**
 * @brief One step. Returns MDTEST_JOB_RUNNING, or the state the job ends in
 *        once its result lines are written; sets the progress and the live
 *        words.
 */
uint16_t cardtest_step(uint32_t nowMs, uint16_t *progress,
                       uint16_t live[MDTEST_JOB_LIVE_WORDS]);

/** @brief The job was cancelled: the file removed, the card stopped. */
void cardtest_abort(void);

#endif  // CARDTEST_H
