/**
 * File: wifitest.h
 * Author: Diego Parrilla Santamaría
 * Date: November 2026
 * Copyright: 2026 - GOODDATA LABS SL
 * Description: The Wi-Fi test (MDTEST_TEST_WIFI), stepped by jobs.c: the
 *              radio chip started, a scan, the network Booster was set up
 *              for joined with Booster's settings, the router, a name and a
 *              web server asked, each a short step; the first that fails
 *              ends the test. The chip is powered down when it ends.
 */

#ifndef WIFITEST_H
#define WIFITEST_H

#include <stdbool.h>
#include <stdint.h>

#include "mdtest.h"

#if (defined(_DEBUG) && (_DEBUG != 0)) || (defined(EMUMD) && EMUMD)
// Debug and EmuMD builds: Booster's Wi-Fi as if not set up to join a
// network (swd.py app wifi_not_set_up 1, MDTEST_WIFI_NOT_SET_UP).
extern volatile bool wifitestNotSetUp;
#endif

/** @brief Begin the Wi-Fi test. */
void wifitest_begin(uint32_t nowMs);

/**
 * @brief One step. Returns MDTEST_JOB_RUNNING, or the state the job ends in
 *        once its result lines are written; sets the progress and the live
 *        words.
 */
uint16_t wifitest_step(uint32_t nowMs, uint16_t *progress,
                       uint16_t live[MDTEST_JOB_LIVE_WORDS]);

/** @brief The job was cancelled: the connection closed, the chip down. */
void wifitest_abort(void);

#endif  // WIFITEST_H
