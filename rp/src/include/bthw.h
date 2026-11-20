/**
 * File: bthw.h
 * Author: Diego Parrilla Santamaría
 * Date: November 2026
 * Copyright: 2026 - GOODDATA LABS SL
 * Description: The radio chip's Bluetooth controller for the Bluetooth test:
 *              started on demand, asked to scan (classic inquiry, then a
 *              low-energy scan), stopped with the chip powered down.
 *              bthw.c on the device (BTstack); emu/standins.c under EmuMD.
 */

#ifndef BTHW_H
#define BTHW_H

#include <stdbool.h>
#include <stdint.h>

// The controller.
#define BTHW_OFF 0
#define BTHW_STARTING 1
#define BTHW_WORKING 2
#define BTHW_FAILED 3

#define BTHW_ADDRESS_BYTES 6U
#define BTHW_NAME_BYTES 33U  // a name as received, cut to 32 characters

#define BTHW_KIND_CLASSIC 0U
#define BTHW_KIND_LE 1U

typedef struct {
  uint8_t address[BTHW_ADDRESS_BYTES];
  int8_t rssi;            // dBm; 0 when the device gave none
  uint8_t kind;           // BTHW_KIND_*
  char name[BTHW_NAME_BYTES];  // "" when none came
} BtHwDevice;

typedef struct {
  uint8_t address[BTHW_ADDRESS_BYTES];
  uint8_t hciVersion;      // the Bluetooth version, as HCI numbers it
  uint16_t lmpSubversion;  // the controller's firmware build
  uint16_t manufacturer;   // its company identifier
} BtHwInfo;

/** @brief Each device heard during a scan, as it is heard (again, maybe). */
typedef void (*BtHwDeviceSeen)(const BtHwDevice *device);

/**
 * @brief Start the radio chip and power the controller on. 0 once asked
 *        (then poll bthw_state()), negative when the chip does not start.
 */
int bthw_start(BtHwDeviceSeen seen);

/** @brief The controller's state (BTHW_*), with its events serviced. */
int bthw_state(void);

/** @brief Who the controller is, once BTHW_WORKING. */
void bthw_info(BtHwInfo *info);

/**
 * @brief Scan for seconds: classic inquiry, or a low-energy scan. 0 when
 *        started.
 */
int bthw_scan(bool classic, uint32_t seconds);

/** @brief Whether the scan still runs (a low-energy one until stopped). */
bool bthw_scanning(void);

/** @brief Stop a scan. */
void bthw_scanStop(void);

/** @brief The controller off and the chip powered down. */
void bthw_stop(void);

#endif  // BTHW_H
