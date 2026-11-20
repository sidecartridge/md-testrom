/*
 * File: standins.c
 * Description: What the Multidevice Test firmware needs from the hardware
 *              layer EmuMD leaves out (romemul.c, commemul.c, select.c), for
 *              its build under EmuMD only. The device build never sees this.
 */
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "commemul.h"
#include "romemul.h"
#include "select.h"

/* Both banks are in EmuMD's 128 KB window already (emul.c copied the image
 * there); EmuMD serves ROM4 from it, and ROM3 reads reach the command ring. */
int romemul_initTwoBanks(bool copyFlashToRAM) {
  (void)copyFlashToRAM;
  return 0;
}

void romemul_stop(void) {}
void commemul_stop(void) {}
/* EmuMD's capture never overruns. */
uint32_t commemul_getOverruns(void) { return 0; }

/* SELECT. EmuMD drops the callbacks the firmware registers, so the bench
 * press does what the firmware's short press does (emul.c): it leaves for
 * Booster, which EmuMD does not run, unless the SELECT test runs. The bench
 * presses it by creating the file EMU_SELECT_FILE names, holding how long
 * the press lasts in ms (100 if empty); the level reads pressed that long
 * (emu_selectLevel(), the SELECT test's), and the release is the press. */
#include "emul.h"
#include "pico/stdlib.h"

#define EMU_SELECT_POLL_US 20000u
#define EMU_SELECT_DEFAULT_MS 100u

static bool emuPressed;
static uint32_t emuReleaseUs;

bool emu_selectLevel(void) {
  return emuPressed && (int32_t)(time_us_32() - emuReleaseUs) < 0;
}

void select_poll(void) {
  static const char *path;
  static uint32_t lastUs;
  if (!path && !(path = getenv("EMU_SELECT_FILE"))) path = "";
  if (!*path) return;
  uint32_t now = time_us_32();
  if (emuPressed && !emu_selectLevel()) {
    emuPressed = false;
    fprintf(stderr, "emu: SELECT released\n");
    emul_selectShortPress();
  }
  if (now - lastUs < EMU_SELECT_POLL_US) return;
  lastUs = now;
  FILE *f = fopen(path, "r");
  if (!f) return;
  unsigned int ms = 0;
  if (fscanf(f, "%u", &ms) != 1 || ms == 0) ms = EMU_SELECT_DEFAULT_MS;
  fclose(f);
  unlink(path);
  emuPressed = true;
  emuReleaseUs = now + ms * 1000u;
  fprintf(stderr, "emu: SELECT pressed for %u ms\n", ms);
}

/* The microSD card under the card tests (cardhw.h). EmuMD stands in for the
 * card at FatFs's level, its [run] sd folder, always mounted: the card's own
 * start is this stand-in, a card that is in unless the file EMU_CARD_OUT_FILE
 * names exists (the bench takes it out by creating that file). The same
 * pretend 32 GB card EmuMD's f_getfree() describes. */
#include "cardhw.h"

#define EMU_CARD_SECTORS (64u * 1024u * 1024u)
#define EMU_CARD_SPI_KHZ 12500u

static bool emuCardIn(void) {
  const char *path = getenv("EMU_CARD_OUT_FILE");
  return !path || access(path, F_OK) != 0;
}

int cardhw_start(CardHwInfo *info) {
  /* Maker 03, OEM "SD", product "EMUMD", revision 1.0, a serial, made
   * November 2026. */
  static const uint8_t cid[CARDHW_CID_BYTES] = {
      0x03, 'S', 'D', 'E', 'M', 'U', 'M', 'D', 0x10,
      0x4D, 0x44, 0x45, 0x4D, 0x01, 0xAB, 0x00};
  memset(info, 0, sizeof(*info));
  info->spiKhz = EMU_CARD_SPI_KHZ;
  if (!emuCardIn()) return CARDHW_NO_ANSWER;
  info->type = CARDHW_TYPE_SDHC;
  info->sectors = EMU_CARD_SECTORS;
  memcpy(info->cid, cid, sizeof(cid));
  return CARDHW_OK;
}

bool cardhw_present(void) { return emuCardIn(); }

void cardhw_stop(void) {}

/* Bluetooth: EmuMD has no bare BTstack, so the controller is this stand-in.
 * It starts 300 ms after it is asked, as Cypress (company 305) with
 * Bluetooth 5.1, and a scan hears three devices: a classic one with a name,
 * a low-energy one with a name and one without. EMU_BT_FAIL in the
 * environment makes the controller fail to start, EMU_BT_NONE makes the
 * scans hear nothing; the radio chip fails as the LED test's does
 * (MDTEST_CHIP_FAIL). */
#include "bthw.h"
#include "devtests.h"

#define EMU_BT_START_US 300000u
#define EMU_BT_CYPRESS 305u
#define EMU_BT_HCI_5_1 10u
#define EMU_BT_SUBVERSION 0x2257u

static int emuBtState = BTHW_OFF;
static uint32_t emuBtAskedUs;
static BtHwDeviceSeen emuBtSeen;
static bool emuBtScanning;

int bthw_start(BtHwDeviceSeen seen) {
  if (devtestsChipFails) return -1;
  emuBtSeen = seen;
  emuBtAskedUs = time_us_32();
  emuBtState = BTHW_STARTING;
  return 0;
}

int bthw_state(void) {
  if (emuBtState == BTHW_STARTING &&
      time_us_32() - emuBtAskedUs >= EMU_BT_START_US) {
    emuBtState = getenv("EMU_BT_FAIL") ? BTHW_FAILED : BTHW_WORKING;
  }
  return emuBtState;
}

void bthw_info(BtHwInfo *info) {
  static const uint8_t address[BTHW_ADDRESS_BYTES] = {0x28, 0xCD, 0xC1,
                                                      0x4D, 0x44, 0x02};
  memset(info, 0, sizeof(*info));
  memcpy(info->address, address, sizeof(address));
  info->hciVersion = EMU_BT_HCI_5_1;
  info->lmpSubversion = EMU_BT_SUBVERSION;
  info->manufacturer = EMU_BT_CYPRESS;
}

static void emuBtHear(const uint8_t last, int8_t rssi, uint8_t kind,
                      const char *name) {
  BtHwDevice device;
  memset(&device, 0, sizeof(device));
  static const uint8_t base[BTHW_ADDRESS_BYTES - 1] = {0xE0, 0x4D, 0x44,
                                                       0x45, 0x4D};
  memcpy(device.address, base, sizeof(base));
  device.address[BTHW_ADDRESS_BYTES - 1] = last;
  device.rssi = rssi;
  device.kind = kind;
  snprintf(device.name, sizeof(device.name), "%s", name);
  if (emuBtSeen) emuBtSeen(&device);
}

int bthw_scan(bool classic, uint32_t seconds) {
  (void)seconds;
  if (emuBtState != BTHW_WORKING) return -1;
  if (!getenv("EMU_BT_NONE")) {
    if (classic) {
      emuBtHear(0x01, -58, BTHW_KIND_CLASSIC, "EmuMD Phone");
    } else {
      emuBtHear(0x02, -71, BTHW_KIND_LE, "EmuMD Tag");
      emuBtHear(0x03, -88, BTHW_KIND_LE, "");
      emuBtHear(0x02, -66, BTHW_KIND_LE, "");  /* heard again, stronger */
    }
  }
  /* A classic inquiry ends by itself; a low-energy scan runs until
   * stopped. */
  emuBtScanning = !classic;
  return 0;
}

bool bthw_scanning(void) { return emuBtScanning; }

void bthw_scanStop(void) { emuBtScanning = false; }

void bthw_stop(void) {
  emuBtSeen = NULL;
  emuBtScanning = false;
  emuBtState = BTHW_OFF;
}

/* health.c records why the RP restarted from the RP2040's watchdog, fault
 * handlers and SCB, none of which EmuMD has: a restart there is a new run. */
#include "health.h"

volatile uint32_t healthMaxFeedGapUs = 0;
volatile uint8_t healthMaxFeedGapPhase = 0;

void health_init(void) {}
void health_watchdogStart(void) {}
void health_feed(void) {}
void health_markReset(void) {}
void health_prepareJump(void) {}
void health_tick(void) {}
bool health_isCrashLoop(void) { return false; }
/* What the device information shows: a power-on, no heap or stack measured. */
void health_getReport(health_report_t *out) {
  memset(out, 0, sizeof(*out));
  out->uptime_ms = to_ms_since_boot(get_absolute_time());
  out->boot = HEALTH_BOOT_POWER_ON;
}
const char *health_bootName(health_boot_t boot) {
  return boot == HEALTH_BOOT_POWER_ON ? "power_on" : "other";
}
const char *health_phaseName(uint8_t phase) {
  (void)phase;
  return "main_loop";
}
#if defined(_DEBUG) && (_DEBUG != 0)
void health_requestTest(health_test_t test) { (void)test; }
#endif
