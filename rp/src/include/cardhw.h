/**
 * File: cardhw.h
 * Author: Diego Parrilla Santamaría
 * Date: November 2026
 * Copyright: 2026 - GOODDATA LABS SL
 * Description: The microSD card's hardware for the card tests: started on
 *              demand, asked whether it is still there, stopped. cardhw.c on
 *              the device (the SPI driver); emu/standins.c under EmuMD.
 */

#ifndef CARDHW_H
#define CARDHW_H

#include <stdbool.h>
#include <stdint.h>

// How a start went.
#define CARDHW_OK 0
#define CARDHW_NO_ANSWER 1   // nothing answered: no card, or a dead one
#define CARDHW_OLD_CARD 2    // a standard-capacity card (2 GB or less)
#define CARDHW_FAILED 3      // it answered, then failed its start

#define CARDHW_CID_BYTES 16U

// The card's kind, from its start.
#define CARDHW_TYPE_SDSC 1
#define CARDHW_TYPE_SDHC 2  // SDHC or SDXC: the size tells them apart

typedef struct {
  int type;              // CARDHW_TYPE_*
  uint32_t sectors;      // 512-byte sectors
  uint8_t cid[CARDHW_CID_BYTES];  // the card's identification register
  uint32_t spiKhz;       // the bus speed in use
} CardHwInfo;

/** @brief Start the card (the SPI bus at Booster's speed). CARDHW_*. */
int cardhw_start(CardHwInfo *info);

/** @brief Whether a card answers now, started or not (a few ms). */
bool cardhw_present(void);

/** @brief Stop the card and forget it: the next start is from scratch. */
void cardhw_stop(void);

#endif  // CARDHW_H
