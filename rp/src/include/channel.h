/**
 * File: channel.h
 * Author: Diego Parrilla Santamaría
 * Date: November 2026
 * Copyright: 2026 - GOODDATA LABS SL
 * Description: ROM3 as test data or as the command channel (common/mdtest.h).
 *
 * From boot ROM3 is test data: the firmware drains the ROM3 capture itself,
 * answers nothing, and listens for two frames only, the line test's one-way
 * verdict and the select frame. Selected, the template's dispatcher answers
 * the app's commands through the block it borrows in ROM4, and the jobs run.
 * Unselected again, the whole window is the pattern.
 */

#ifndef CHANNEL_H
#define CHANNEL_H

#include <stdbool.h>
#include <stdint.h>

/**
 * @brief Register the app's commands. The window is the one the engine
 *        serves, already filled.
 */
void channel_init(uint16_t *window);

/** @brief One pass of the main loop: the capture, the jobs, the bench. */
void channel_poll(void);

bool channel_selected(void);

/** @brief True once the ST's exit to Booster has been answered and read. */
bool channel_exitRequested(void);

#if (defined(_DEBUG) && (_DEBUG != 0)) || (defined(EMUMD) && EMUMD)
/**
 * @brief Debug builds and EmuMD: what the ST sees of an RP restart, without
 *        one (a reset while the ST reads the cartridge crashes the ST): ROM3
 *        given back as test data, the job forgotten, a new boot ID, the
 *        sequence numbers from the start.
 */
void channel_benchRestart(void);
#endif

#if defined(_DEBUG) && (_DEBUG != 0)
/**
 * @brief Debug builds: select ROM3, or give it back, with no ST (the probe's
 *        commands are injected, so nothing is answered through the token).
 */
void channel_benchSelect(bool on);
#endif

#endif  // CHANNEL_H
