/**
 * File: devtests.c
 * Author: Diego Parrilla Santamaría
 * Date: November 2026
 * Copyright: 2026 - GOODDATA LABS SL
 * Description: The device tests' hardware (devtests.h): the RP's identity
 *              and health, SELECT's level, and the green LED, which on a
 *              Pico W belongs to the radio chip.
 */

#include "devtests.h"

#include "aconfig.h"
#include "build_id.h"
#include "debug.h"
#include "gconfig.h"
#include "health.h"
#include "jobs.h"
#include "hardware/clocks.h"
#include "pico/cyw43_arch.h"
#include "pico/stdlib.h"
#include "pico/unique_id.h"
#include "select.h"

#if !(defined(EMUMD) && EMUMD)
#include "hardware/adc.h"
#include "hardware/vreg.h"
#endif

#define US_PER_MS 1000U
#define HZ_PER_MHZ 1000000U
#define BYTES_PER_KB 1024U

#if (defined(_DEBUG) && (_DEBUG != 0)) || (defined(EMUMD) && EMUMD)
volatile bool devtestsChipFails = false;
#define CHIP_FAILS devtestsChipFails
#else
#define CHIP_FAILS false
#endif

static bool chipUp = false;

#if !(defined(EMUMD) && EMUMD)
// The core voltage in mV: the RP2040's regulator steps 50 mV from 0.85 V at
// VREG_VOLTAGE_0_85, with 0.80 V and below folded into its lowest settings.
#define VREG_MV_BASE 850U
#define VREG_MV_STEP 50U
#define VREG_MV_FLOOR 800U

static unsigned int coreMillivolts(void) {
  unsigned int setting = (unsigned int)vreg_get_voltage();
  if (setting < VREG_VOLTAGE_0_85) return VREG_MV_FLOOR;
  return VREG_MV_BASE + ((setting - VREG_VOLTAGE_0_85) * VREG_MV_STEP);
}

// The die's temperature in tenths of a degree, from the ADC's sensor
// (RP2040 datasheet: 27 C at 0.706 V, -1.721 mV a degree), 3.3 V reference.
#define TEMP_INPUT 4U
#define TEMP_SAMPLES 16U
#define ADC_FULL_SCALE 4096
#define ADC_REF_UV 3300000
#define TEMP_REF_UV 706000
#define TEMP_UV_PER_TENTH 172  // 1.721 mV a degree, rounded
#define TEMP_REF_TENTHS 270    // 27.0 C

static int dieTenths(void) {
  adc_init();
  adc_set_temp_sensor_enabled(true);
  adc_select_input(TEMP_INPUT);
  uint32_t sum = 0;
  for (unsigned int i = 0; i < TEMP_SAMPLES; i++) sum += adc_read();
  adc_set_temp_sensor_enabled(false);
  int32_t microvolts = (int32_t)((int64_t)sum * ADC_REF_UV /
                                 ((int64_t)TEMP_SAMPLES * ADC_FULL_SCALE));
  return TEMP_REF_TENTHS - ((microvolts - TEMP_REF_UV) / TEMP_UV_PER_TENTH);
}
#endif

static const char *settingValue(SettingsContext *ctx, const char *key) {
  SettingsConfigEntry *entry = settings_find_entry(ctx, key);
  return entry ? entry->value : NULL;
}

void devtests_info(void) {
  char uid[2 * PICO_UNIQUE_BOARD_ID_SIZE_BYTES + 1];
  pico_get_unique_board_id_string(uid, sizeof(uid));
  jobs_resultLine("version=%s", RELEASE_VERSION);
  jobs_resultLine("build=%s", RELEASE_BUILD_ID);
  jobs_resultLine("board=pico_w");
  jobs_resultLine("uid=%s", uid);
  jobs_resultLine("flash_kb=%lu",
                  (unsigned long)(PICO_FLASH_SIZE_BYTES / BYTES_PER_KB));
  jobs_resultLine("clock_mhz=%lu",
                  (unsigned long)(clock_get_hz(clk_sys) / HZ_PER_MHZ));
#if !(defined(EMUMD) && EMUMD)
  jobs_resultLine("core_mv=%u", coreMillivolts());
  int tenths = dieTenths();
  jobs_resultLine("temp_c=%s%d.%d", tenths < 0 ? "-" : "",
                  (tenths < 0 ? -tenths : tenths) / 10,
                  (tenths < 0 ? -tenths : tenths) % 10);
#endif

  health_report_t r;
  health_getReport(&r);
  jobs_resultLine("uptime_s=%lu", (unsigned long)(r.uptime_ms / US_PER_MS));
#if !(defined(EMUMD) && EMUMD)  // EmuMD's health record measures neither
  jobs_resultLine("heap_free=%lu", (unsigned long)r.heap_free);
  jobs_resultLine("heap_min_free=%lu", (unsigned long)r.heap_min_free);
  jobs_resultLine("stack_used=%lu", (unsigned long)r.stack_high_water);
  jobs_resultLine("stack_size=%lu", (unsigned long)r.stack_reserved);
#endif
  jobs_resultLine("restart=%s", health_bootName(r.boot));
  if (r.boot == HEALTH_BOOT_HANG) {
    // The template's name for the main loop is its setup menu's.
    jobs_resultLine("restart_phase=%s",
                    r.boot_phase == HEALTH_PHASE_MAIN_LOOP
                        ? "main loop"
                        : health_phaseName(r.boot_phase));
  }
  if (r.boot == HEALTH_BOOT_PANIC || r.boot == HEALTH_BOOT_HARDFAULT) {
    jobs_resultLine("restart_pc=%08lX", (unsigned long)r.boot_pc);
  }
  jobs_resultLine("crashes=%u", (unsigned)r.crash_count);
  jobs_resultLine("crash_loop=%s", r.crash_loop ? "yes" : "no");

  // Booster's settings, read only. The Wi-Fi network's name and password
  // are never read out: only whether one is set.
  SettingsContext *global = gconfig_getContext();
  if (!global || !global->configData.entries) {
    jobs_resultLine("settings=unreadable");
    return;
  }
  SettingsContext *app = aconfig_getContext();
  jobs_resultLine("app_entry=%s", app && app->flashSettingsOffset
                                      ? "found"
                                      : "missing");
  const char *hostname = settingValue(global, PARAM_HOSTNAME);
  const char *ssid = settingValue(global, PARAM_WIFI_SSID);
  const char *sdBaud = settingValue(global, PARAM_SD_BAUD_RATE_KB);
  jobs_resultLine("hostname=%s", hostname ? hostname : "");
  jobs_resultLine("wifi=%s", ssid && ssid[0] ? "configured" : "none");
  jobs_resultLine("sd_baud_kb=%s", sdBaud ? sdBaud : "");
}

#if defined(EMUMD) && EMUMD
// emu/standins.c: the bench's press.
bool emu_selectLevel(void);
bool devtests_selectLevel(void) { return emu_selectLevel(); }
#else
bool devtests_selectLevel(void) { return select_detectPush(); }
#endif

int devtests_ledChipUp(uint32_t *startMs, int32_t *heapBytes) {
  if (chipUp) return 0;
  if (CHIP_FAILS) return PICO_ERROR_GENERIC;
  health_report_t before;
  health_getReport(&before);
  uint32_t t0 = time_us_32();
  // The driver loads the chip's firmware at its first use, not here: one
  // read of the LED's pin brings it up, so the cost is measured and a chip
  // that does not answer shows now.
  int answer = cyw43_arch_init();
  if (answer == 0) {
    bool level = false;
    answer = cyw43_gpio_get(&cyw43_state, CYW43_WL_GPIO_LED_PIN, &level);
    if (answer != 0) cyw43_arch_deinit();
  }
  *startMs = (time_us_32() - t0) / US_PER_MS;
  if (answer != 0) {
    DPRINTF("LED: the radio chip did not start: %d\n", answer);
    return answer < 0 ? answer : -answer;
  }
  chipUp = true;
  health_report_t after;
  health_getReport(&after);
  *heapBytes = before.heap_free >= after.heap_free
                   ? (int32_t)(before.heap_free - after.heap_free)
                   : -1;
#if defined(EMUMD) && EMUMD
  *heapBytes = -1;  // the stand-in health record has no heap
#endif
  DPRINTF("LED: the radio chip started in %lu ms\n", (unsigned long)*startMs);
  return 1;
}

int devtests_ledSet(bool on) {
  if (!chipUp || CHIP_FAILS) return PICO_ERROR_GENERIC;
  int answer = cyw43_gpio_set(&cyw43_state, CYW43_WL_GPIO_LED_PIN, on);
  if (answer != 0) return answer < 0 ? answer : -answer;
  bool level = !on;
  answer = cyw43_gpio_get(&cyw43_state, CYW43_WL_GPIO_LED_PIN, &level);
  if (answer != 0) return answer < 0 ? answer : -answer;
  return level == on ? 0 : 1;
}

void devtests_ledRelease(void) {
  if (!chipUp) return;
  (void)cyw43_gpio_set(&cyw43_state, CYW43_WL_GPIO_LED_PIN, false);
  cyw43_arch_deinit();
  chipUp = false;
  DPRINTF("LED: off, the radio chip powered down\n");
}

void devtests_poll(void) {
  if (chipUp) cyw43_arch_poll();
}
