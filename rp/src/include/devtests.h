/**
 * File: devtests.h
 * Author: Diego Parrilla Santamaría
 * Date: November 2026
 * Copyright: 2026 - GOODDATA LABS SL
 * Description: What the device tests (jobs.c) need from the hardware: the
 *              device's identity and health, SELECT's level, the green LED.
 *              devtests.c on the RP and under EmuMD; the host tests have
 *              their own.
 */

#ifndef DEVTESTS_H
#define DEVTESTS_H

#include <stdbool.h>
#include <stdint.h>

/** @brief The device's identity and health as result lines
 *         (jobs_resultLine). Reads only. */
void devtests_info(void);

/** @brief SELECT's raw level: true while pressed. */
bool devtests_selectLevel(void);

/**
 * @brief The radio chip up, for the LED: 1 when this call brought it up
 *        (startMs and heapBytes say what it cost; heapBytes -1 when unknown),
 *        0 when it was up already, negative when it did not answer.
 */
int devtests_ledChipUp(uint32_t *startMs, int32_t *heapBytes);

/** @brief The LED on or off: 0 when the chip took it and reads it back, 1
 *         when it reads back otherwise, negative when it did not answer. */
int devtests_ledSet(bool on);

/** @brief The LED off and the chip powered down: as the firmware started. */
void devtests_ledRelease(void);

/** @brief While the chip is up, its driver's work: from the main loop. */
void devtests_poll(void);

#if (defined(_DEBUG) && (_DEBUG != 0)) || (defined(EMUMD) && EMUMD)
/** @brief The chip fails to start, as a dead radio would (a bench fault). */
extern volatile bool devtestsChipFails;
#endif

#endif  // DEVTESTS_H
