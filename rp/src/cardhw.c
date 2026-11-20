/**
 * File: cardhw.c
 * Author: Diego Parrilla Santamaría
 * Date: November 2026
 * Copyright: 2026 - GOODDATA LABS SL
 * Description: The microSD card's hardware (cardhw.h) over the template's
 *              SPI driver (hw_config.c, sdcard.c). Nothing here runs at
 *              boot: the card is started only when a card test asks.
 */

#include "cardhw.h"

#include <string.h>

#include "debug.h"
#include "diskio.h"
#include "sd_card.h"
#include "sd_timeouts.h"
#include "sdcard.h"

#define CARD_SLOT 0U
#define HZ_PER_KHZ 1000U

// The driver's own timeouts are 2 s a command with 8 s locks, so a card that
// hangs could hold a job's step past the watchdog's 8 s. These bound a step
// to a few seconds; a card that takes the full 1 s the specification allows
// to leave its idle state still starts.
// NOLINTBEGIN(readability-magic-numbers)
sd_timeouts_t sd_timeouts = {
    .sd_command = 1000,
    .sd_command_retries = 2,
    .sd_lock = 1000,
    .sd_spi_read = 500,
    .sd_spi_write = 500,
    .sd_spi_write_read = 500,
    .spi_lock = 1000,
    .rp2040_sdio_command_R1 = 10,
    .rp2040_sdio_command_R2 = 2,
    .rp2040_sdio_command_R3 = 2,
    .rp2040_sdio_rx_poll = 1000,
    .rp2040_sdio_tx_poll = 1000,
    .sd_sdio_begin = 1000,
    .sd_sdio_stopTransmission = 200,
};
// NOLINTEND(readability-magic-numbers)

static bool driverUp = false;

static sd_card_t *card(void) {
  if (!driverUp) {
    if (!sd_init_driver()) return NULL;
    driverUp = true;
  }
  return sd_get_by_num(CARD_SLOT);
}

// One start from scratch: the driver's view of the card forgotten first
// (its init returns at once while it believes the card started).
static sd_card_t *startCard(void) {
  sd_card_t *sd = card();
  if (sd == NULL) return NULL;
  sd->deinit(sd);
  sdcard_setSpiSpeedSettings();
  sd->init(sd);
  return sd;
}

int cardhw_start(CardHwInfo *info) {
  memset(info, 0, sizeof(*info));
  sd_card_t *sd = startCard();
  if (sd == NULL) return CARDHW_FAILED;
  info->spiKhz = sd->spi_if_p->spi->baud_rate / HZ_PER_KHZ;
  card_type_t type = sd->state.card_type;
  if (!(sd->state.m_Status & STA_NOINIT)) {
    info->type = CARDHW_TYPE_SDHC;
    info->sectors = sd->state.sectors;
    memcpy(info->cid, sd->state.CID, sizeof(info->cid));
    return CARDHW_OK;
  }
  DPRINTF("Card: no start, type %d\n", (int)type);
  if (type == SDCARD_NONE) return CARDHW_NO_ANSWER;
  if (type == SDCARD_V1 || type == SDCARD_V2) {
    info->type = CARDHW_TYPE_SDSC;
    return CARDHW_OLD_CARD;
  }
  return CARDHW_FAILED;
}

bool cardhw_present(void) {
  sd_card_t *sd = startCard();
  bool up = sd != NULL && !(sd->state.m_Status & STA_NOINIT);
  if (sd != NULL) sd->deinit(sd);
  return up;
}

void cardhw_stop(void) {
  if (!driverUp) return;
  sd_card_t *sd = sd_get_by_num(CARD_SLOT);
  if (sd != NULL) sd->deinit(sd);
}
