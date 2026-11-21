#include "devscreen.h"

#include <stdlib.h>
#include <string.h>

#include "chscreen.h"
#include "commands.h"
#include "console.h"
#include "mdtest.h"
#include "texts.h"

#define HZ_200 (*(volatile unsigned long *)0x4BA)

#define INFO_POLL_TICKS 20UL   /* 100 ms */
#define SELECT_POLL_TICKS 10UL /* 50 ms: the press shows as it happens */
#define LED_POLL_TICKS 20UL
#define CARD_POLL_TICKS 20UL
#define WIFI_POLL_TICKS 20UL
#define BT_POLL_TICKS 20UL
#define BT_BROADCOM 15L
#define BT_CYPRESS 305L
#define WIFI_WEAK_DBM (-70L)
/* A card slower than this, writing or reading, gets a warning. */
#define CARD_SLOW_KBS 100L
/* When a press has lasted this long, the factory reset is near. */
#define SELECT_WARN_MS 5000U
/* The coolest and hottest die temperature that is not worth a word. */
#define TEMP_LOW_C 0
#define TEMP_HIGH_C 70

static char text[CH_RESULT_BYTES];

/* The last test's verdict, for the guided run: a test that returns early
 * because the device stopped answering stays "lost". */
static Outcome lastOutcome;
static Reason lastReason;

static void verdict(Outcome o, Reason r) {
  lastOutcome = o;
  lastReason = r;
}

static void begin(void) { verdict(OUT_FAIL, R_LOST); }

void devscreen_verdict(Outcome *o, Reason *r) {
  *o = lastOutcome;
  *r = lastReason;
}
static const unsigned short noParams[4] = {0, 0, 0, 0};

/* A value, up to its newline, copied (empty when absent); the last
 * VALUE_SLOTS stay valid, for one line's arguments. */
#define VALUE_SLOTS 8
static const char *value(const char *key) {
  static char out[VALUE_SLOTS][64];
  static int next;
  char *v = out[next++ & (VALUE_SLOTS - 1)];
  const char *found = chscreen_value(text, key);
  int i = 0;
  if (found) {
    for (; found[i] && found[i] != '\n' && i < 63; i++) v[i] = found[i];
  }
  v[i] = '\0';
  return v;
}

static int has(const char *key) { return chscreen_value(text, key) != 0; }

static const char *restartName(const char *code) {
  if (!strcmp(code, "power_on")) return T_INFO_RESTART_POWER;
  if (!strcmp(code, "reset")) return T_INFO_RESTART_RESET;
  if (!strcmp(code, "panic")) return T_INFO_RESTART_PANIC;
  if (!strcmp(code, "hardfault")) return T_INFO_RESTART_FAULT;
  if (!strcmp(code, "hang")) return T_INFO_RESTART_HANG;
  return T_INFO_RESTART_OTHER;
}

/* A restart the user did not ask for: the RP crashed or hung. */
static int crashed(const char *code) {
  return !strcmp(code, "panic") || !strcmp(code, "hardfault") ||
         !strcmp(code, "hang");
}

void devscreen_info(void) {
  console_blank();
  console_line(T_INFO_TITLE);
  begin();
  int state = chscreen_run(MDTEST_TEST_DEVICE_INFO, noParams, INFO_POLL_TICKS,
                           0, text);
  if (state < 0) return;
  if (state != MDTEST_JOB_DONE) {
    console_line(T_JOB_FAILED, MDTEST_TEST_DEVICE_INFO);
    verdict(OUT_FAIL, R_NONE);
    return;
  }
  verdict(OUT_PASS, R_NONE);
  console_line(T_INFO_FIRMWARE, value("version"), value("build"));
  console_line(T_INFO_BOARD, value("uid"));
  console_line(T_INFO_FLASH, value("flash_kb"));
  if (has("core_mv")) {
    console_line(T_INFO_CLOCK_MV, value("clock_mhz"), value("core_mv"));
  } else {
    console_line(T_INFO_CLOCK, value("clock_mhz"));
  }
  if (has("temp_c")) console_line(T_INFO_TEMP, value("temp_c"));
  console_line(T_INFO_UPTIME, value("uptime_s"));
  if (has("heap_free")) {
    console_line(T_INFO_HEAP, value("heap_free"), value("heap_min_free"));
  }
  if (has("stack_size")) {
    console_line(T_INFO_STACK, value("stack_used"), value("stack_size"));
  }
  const char *restart = value("restart");
  console_line(T_INFO_RESTART, restartName(restart));
  if (has("restart_phase")) {
    console_line(T_INFO_RESTART_PHASE, value("restart_phase"));
  }
  if (has("restart_pc")) console_line(T_INFO_RESTART_PC, value("restart_pc"));
  int settings = !chscreen_valueIs(text, "settings", "unreadable");
  if (settings) {
    console_line(T_INFO_HOSTNAME, value("hostname"));
    console_line(T_INFO_WIFI, chscreen_valueIs(text, "wifi", "configured")
                                  ? T_INFO_WIFI_SET
                                  : T_INFO_WIFI_NONE);
    console_line(T_INFO_SD, value("sd_baud_kb"));
  }

  console_blank();
  int warned = 0;
  if (chscreen_valueIs(text, "crash_loop", "yes")) {
    console_line(T_INFO_WARN_LOOP);
    console_line(T_INFO_WARN_LOOP_DETAIL, value("crashes"));
    console_line(T_INFO_WARN_CRASH_ADVICE);
    verdict(OUT_WARN, R_DEV_CRASH);
    warned = 1;
  } else if (crashed(restart)) {
    console_line(T_INFO_WARN_CRASH);
    console_line(T_INFO_WARN_CRASH_DETAIL, restart);
    console_line(T_INFO_WARN_CRASH_ADVICE);
    verdict(OUT_WARN, R_DEV_CRASH);
    warned = 1;
  }
  if (has("temp_c")) {
    int celsius = atoi(value("temp_c"));
    if (celsius < TEMP_LOW_C || celsius > TEMP_HIGH_C) {
      console_line(T_INFO_WARN_TEMP);
      console_line(T_INFO_WARN_TEMP_DETAIL, value("temp_c"), TEMP_LOW_C,
                   TEMP_HIGH_C);
      if (!warned) verdict(OUT_WARN, R_DEV_TEMP);
      warned = 1;
    }
  }
  if (!settings || chscreen_valueIs(text, "app_entry", "missing")) {
    console_line(T_INFO_WARN_SETTINGS);
    console_line(T_INFO_WARN_SETTINGS_ADVICE);
    if (!warned) verdict(OUT_WARN, R_DEV_SETTINGS);
    warned = 1;
  }
  if (!warned) console_line(T_INFO_PASS);
}

/* The SELECT test's live view: each press and release as it happens, and a
 * warning as a press nears the factory reset. */
static int selectWasPressed;
static int selectWarned;

static int selectWatch(const CmdAnswer *a) {
  int pressed = a->data[MDTEST_JOB_ANS_LIVE] != 0;
  unsigned short heldMs = a->data[MDTEST_JOB_ANS_LIVE + 1];
  if (pressed && !selectWasPressed) console_line(T_SEL_PRESSED);
  if (!pressed && selectWasPressed) console_line(T_SEL_RELEASED);
  if (pressed && heldMs >= SELECT_WARN_MS && !selectWarned) {
    console_line(T_SEL_WARN_RESET);
    console_line(T_SEL_WARN_RESET_DETAIL);
    selectWarned = 1;
  }
  selectWasPressed = pressed;
  return 0;
}

void devscreen_select(void) {
  console_blank();
  console_line(T_SEL_TITLE);
  console_line(T_SEL_ASK, (unsigned)MDTEST_SELECT_SECONDS);
  begin();
  selectWasPressed = 0;
  selectWarned = 0;
  int state = chscreen_run(MDTEST_TEST_SELECT, noParams, SELECT_POLL_TICKS,
                           selectWatch, text);
  if (state < 0) return;
  /* The job ends at the release, before a poll can show it. */
  if (state == MDTEST_JOB_DONE) console_line(T_SEL_RELEASED);
  console_blank();
  if (state == MDTEST_JOB_DONE) {
    console_line(T_SEL_PASS);
    console_line(T_SEL_PASS_DETAIL, value("press_ms"));
    console_line(T_SEL_AFTER);
    verdict(OUT_PASS, R_NONE);
    return;
  }
  if (chscreen_valueIs(text, "error", "pressed from the start")) {
    console_line(T_SEL_STUCK);
    console_line(T_SEL_STUCK_DETAIL, value("held_ms"));
    console_line(T_SEL_STUCK_ADVICE);
    verdict(OUT_FAIL, R_SEL_STUCK);
    return;
  }
  console_line(T_SEL_NONE);
  console_line(T_SEL_NONE_DETAIL, (unsigned)MDTEST_SELECT_SECONDS,
               value("error"));
  console_line(T_SEL_NONE_ADVICE);
  verdict(OUT_FAIL, R_SEL_NONE);
}

/* One LED step: the job run to its end. 1 when the chip took it; 0 when it
 * did not (the verdict said), -1 when the device stopped answering. */
static int ledStep(unsigned short mode) {
  const unsigned short params[4] = {mode, 0, 0, 0};
  int state = chscreen_run(MDTEST_TEST_LED, params, LED_POLL_TICKS, 0, text);
  if (state < 0) return -1;
  if (chscreen_valueIs(text, "chip", "started")) {
    console_line(T_LED_CHIP_STARTED, value("chip_start_ms"));
    if (has("chip_heap_bytes")) {
      console_line(T_LED_CHIP_HEAP, value("chip_heap_bytes"));
    }
  }
  if (state == MDTEST_JOB_DONE) return 1;
  console_blank();
  if (chscreen_valueIs(text, "chip", "no answer")) {
    console_line(T_LED_NO_CHIP);
    console_line(T_LED_NO_CHIP_DETAIL, value("chip_error"));
    console_line(T_LED_NO_CHIP_ADVICE);
    verdict(OUT_FAIL, R_CHIP_SILENT);
  } else {
    verdict(OUT_FAIL, R_NONE);
    console_line(T_LED_CHIP_ERROR);
    console_line(T_LED_CHIP_ERROR_DETAIL, value("error"), value("led_error"));
    console_line(T_LED_CHIP_ERROR_ADVICE);
  }
  return 0;
}

static int yes(const char *question) {
  console_line(question);
  for (;;) {
    int key = console_key();
    if (key == 'y' || key == 'n') {
      console_line(key == 'y' ? T_LED_ANSWER_YES : T_LED_ANSWER_NO);
      return key == 'y';
    }
  }
}

void devscreen_led(void) {
  console_blank();
  console_line(T_LED_TITLE);
  console_line(T_LED_STARTING);
  begin();
  int step = ledStep(MDTEST_LED_ON);
  if (step < 0) return;
  if (step == 0) {
    ledStep(MDTEST_LED_RELEASE);
    return;
  }
  int on = yes(T_LED_ASK_ON);
  int off = 0, blinking = 0;
  step = ledStep(MDTEST_LED_OFF);
  if (step > 0) {
    off = yes(T_LED_ASK_OFF);
    unsigned short number;
    const unsigned short blink[4] = {MDTEST_LED_BLINK, 0, 0, 0};
    CmdAnswer a;
    step = -1;
    if (chscreen_start(MDTEST_TEST_LED, blink, &number) &&
        chscreen_status(MDTEST_TEST_LED, number, &a)) {
      blinking = yes(T_LED_ASK_BLINK);
      chscreen_cancel();
      step = 1;
    }
  }
  if (step < 0) return;
  /* As the firmware started: off, the chip powered down. */
  ledStep(MDTEST_LED_RELEASE);
  if (step == 0) return;
  console_blank();
  if (on && off && blinking) {
    console_line(T_LED_PASS);
    verdict(OUT_PASS, R_NONE);
    return;
  }
  verdict(OUT_FAIL, R_LED_UNSEEN);
  console_line(T_LED_FAIL);
  console_line(T_LED_FAIL_DETAIL, on ? T_YES : T_NO, off ? T_YES : T_NO,
               blinking ? T_YES : T_NO);
  console_line(T_LED_FAIL_ADVICE);
}

/* The card test's live view: each phase as it begins. */
static unsigned short cardPhase;

static int cardWatch(const CmdAnswer *a) {
  unsigned short phase = a->data[MDTEST_JOB_ANS_LIVE];
  if (phase == cardPhase) return 0;
  if (phase == MDTEST_SD_PHASE_WRITE) console_line(T_CARD_WRITING, MDTEST_SD_FILE);
  if (phase == MDTEST_SD_PHASE_READ) console_line(T_CARD_READING);
  cardPhase = phase;
  return 0;
}

static void cardFailure(void) {
  console_blank();
  if (chscreen_valueIs(text, "card", "no answer")) {
    console_line(T_CARD_NONE);
    console_line(T_CARD_NONE_DETAIL);
    console_line(T_CARD_NONE_ADVICE);
    verdict(OUT_FAIL, R_CARD_NONE);
  } else if (chscreen_valueIs(text, "card", "standard capacity")) {
    console_line(T_CARD_OLD);
    console_line(T_CARD_OLD_DETAIL);
    console_line(T_CARD_OLD_ADVICE);
    verdict(OUT_FAIL, R_CARD_OLD);
  } else if (chscreen_valueIs(text, "card", "failed")) {
    console_line(T_CARD_START);
    console_line(T_CARD_START_ADVICE);
    verdict(OUT_FAIL, R_CARD_START);
  } else if (chscreen_valueIs(text, "filesystem", "none")) {
    console_line(T_CARD_NO_FS);
    console_line(T_CARD_FS_DETAIL, value("mount_error"));
    console_line(T_CARD_NO_FS_ADVICE);
    verdict(OUT_FAIL, R_CARD_FORMAT);
  } else if (chscreen_valueIs(text, "filesystem", "unreadable")) {
    console_line(T_CARD_BAD_FS);
    console_line(T_CARD_FS_DETAIL, value("mount_error"));
    console_line(T_CARD_BAD_FS_ADVICE);
    verdict(OUT_FAIL, R_CARD_FORMAT);
  } else if (chscreen_valueIs(text, "error", "card full")) {
    console_line(T_CARD_FULL);
    console_line(T_CARD_FULL_DETAIL, value("free_mb"),
                 (unsigned)MDTEST_SD_FILE_KB);
    console_line(T_CARD_FULL_ADVICE);
    verdict(OUT_FAIL, R_CARD_FULL);
  } else if (chscreen_valueIs(text, "error", "write protected")) {
    console_line(T_CARD_LOCKED);
    console_line(T_CARD_LOCKED_DETAIL);
    console_line(T_CARD_LOCKED_ADVICE);
    verdict(OUT_FAIL, R_CARD_LOCKED);
  } else if (chscreen_valueIs(text, "error", "data differs")) {
    console_line(T_CARD_DIFFERS);
    console_line(T_CARD_DIFFERS_DETAIL, value("mismatch_at"),
                 value("mismatch_read"), value("mismatch_expected"));
    console_line(T_CARD_DIFFERS_ADVICE);
    verdict(OUT_FAIL, R_CARD_WRITE);
  } else {
    console_line(T_CARD_WRITE);
    console_line(T_CARD_WRITE_DETAIL, value("error"),
                 has("written_kb") ? value("written_kb") : "0",
                 has("fs_error") ? value("fs_error") : "-");
    console_line(T_CARD_WRITE_ADVICE);
    verdict(OUT_FAIL, R_CARD_WRITE);
  }
}

void devscreen_card(void) {
  console_blank();
  console_line(T_CARD_TITLE);
  console_line(T_CARD_ASK);
  begin();
  cardPhase = MDTEST_SD_PHASE_START;
  int state = chscreen_run(MDTEST_TEST_SD_CARD, noParams, CARD_POLL_TICKS,
                           cardWatch, text);
  if (state < 0) return;
  if (chscreen_valueIs(text, "card", "ok")) {
    console_line(T_CARD_TYPE, value("card_type"), value("card_mb"),
                 value("spi_khz"));
    console_line(T_CARD_MAKER, value("maker"), value("oem"), value("product"),
                 value("revision"), value("made"));
    console_line(T_CARD_SERIAL, value("serial"), value("start_ms"));
  }
  if (has("size_mb")) {
    console_line(T_CARD_FORMAT, value("filesystem"), value("size_mb"),
                 value("free_mb"));
    if (has("label") && value("label")[0]) {
      console_line(T_CARD_LABEL, value("label"));
    }
    if (has("apps_folder")) {
      console_line(chscreen_valueIs(text, "apps_folder", "found")
                       ? T_CARD_APPS_FOUND
                       : T_CARD_APPS_MISSING);
    }
  }
  if (has("read_kbs")) {
    console_line(T_CARD_SPEED, value("write_kbs"), value("read_kbs"));
  }
  if (state == MDTEST_JOB_DONE) {
    console_blank();
    console_line(T_CARD_PASS);
    verdict(OUT_PASS, R_NONE);
    if (atol(value("write_kbs")) < CARD_SLOW_KBS ||
        atol(value("read_kbs")) < CARD_SLOW_KBS) {
      verdict(OUT_WARN, R_CARD_SLOW);
      console_line(T_CARD_SLOW);
      console_line(T_CARD_SLOW_DETAIL, (unsigned)CARD_SLOW_KBS);
      console_line(T_CARD_SLOW_ADVICE);
    }
  } else if (state == MDTEST_JOB_FAILED) {
    cardFailure();
  } else {
    console_line(T_JOB_FAILED, MDTEST_TEST_SD_CARD);
    verdict(OUT_FAIL, R_NONE);
  }
  if (has("file_left")) {
    console_line(T_CARD_LEFT);
    console_line(T_CARD_LEFT_DETAIL, value("file_left"));
  }
}

/* The swap test's live view: what to do now, as each phase begins. */
static unsigned short swapPhase;
static int swapCardAtStart;

static int swapWatch(const CmdAnswer *a) {
  unsigned short phase = a->data[MDTEST_JOB_ANS_LIVE];
  if (phase == swapPhase) return 0;
  if (phase == MDTEST_SD_SWAP_OUT) {
    swapCardAtStart = 1;
    console_line(T_SWAP_OUT, (unsigned)MDTEST_SD_SWAP_SECONDS);
  } else if (phase == MDTEST_SD_SWAP_IN) {
    if (swapCardAtStart) {
      console_line(T_SWAP_IS_OUT);
      console_line(T_SWAP_IN, (unsigned)MDTEST_SD_SWAP_SECONDS);
    } else {
      console_line(T_SWAP_EMPTY);
      console_line(T_SWAP_IN_FIRST, (unsigned)MDTEST_SD_SWAP_SECONDS);
    }
  } else if (phase == MDTEST_SD_SWAP_MOUNT) {
    console_line(T_SWAP_BACK);
  }
  swapPhase = phase;
  return 0;
}

void devscreen_cardSwap(void) {
  console_blank();
  console_line(T_SWAP_TITLE);
  console_line(T_SWAP_LOOKING);
  begin();
  swapPhase = MDTEST_SD_SWAP_FIRST;
  swapCardAtStart = 0;
  int state = chscreen_run(MDTEST_TEST_SD_SWAP, noParams, CARD_POLL_TICKS,
                           swapWatch, text);
  if (state < 0) return;
  /* The job can end before a poll shows the card back. */
  if (state == MDTEST_JOB_DONE && swapPhase != MDTEST_SD_SWAP_MOUNT) {
    console_line(T_SWAP_BACK);
  }
  console_blank();
  verdict(OUT_FAIL, R_SWAP);
  if (state == MDTEST_JOB_DONE) {
    console_line(T_SWAP_PASS);
    verdict(OUT_PASS, R_NONE);
  } else if (chscreen_valueIs(text, "error", "card not taken out")) {
    console_line(T_SWAP_NOT_OUT);
    console_line(T_SWAP_WAITED, (unsigned)MDTEST_SD_SWAP_SECONDS);
    console_line(T_SWAP_NOT_OUT_ADVICE);
  } else if (chscreen_valueIs(text, "error", "card not put back")) {
    console_line(T_SWAP_NOT_IN);
    console_line(T_SWAP_WAITED, (unsigned)MDTEST_SD_SWAP_SECONDS);
    console_line(T_SWAP_NOT_IN_ADVICE);
  } else if (chscreen_valueIs(text, "error", "card back but not usable")) {
    console_line(T_SWAP_UNUSABLE);
    console_line(T_SWAP_UNUSABLE_ADVICE);
  } else {
    console_line(T_JOB_FAILED, MDTEST_TEST_SD_SWAP);
  }
}

int devscreen_exit(long mch) {
  console_blank();
  console_line(T_EXIT_TITLE);
  if (!chscreen_open(mch) ||
      cmd_send(MDTEST_CMD_EXIT, 0, 0, 0) != CMD_OK) {
    console_line(T_EXIT_FAIL);
    console_line(T_EXIT_FAIL_ADVICE);
    return 0;
  }
  /* The cartridge port is Booster's from here: nothing more is sent. */
  console_setMirror(0);
  console_line(T_EXIT_DONE);
  console_line(T_EXIT_ADVICE);
  return 1;
}

/* The Wi-Fi test's live view: each step as it begins. */
static unsigned short wifiPhase;

static int wifiWatch(const CmdAnswer *a) {
  unsigned short phase = a->data[MDTEST_JOB_ANS_LIVE];
  if (phase == wifiPhase) return 0;
  /* A poll can miss a short phase: each one between is said too. */
  while (wifiPhase < phase) {
    wifiPhase++;
    if (wifiPhase == MDTEST_WIFI_PHASE_SCAN) console_line(T_WIFI_SCAN);
    if (wifiPhase == MDTEST_WIFI_PHASE_JOIN) console_line(T_WIFI_JOIN);
    if (wifiPhase == MDTEST_WIFI_PHASE_ADDRESS) console_line(T_WIFI_ADDRESS);
    if (wifiPhase == MDTEST_WIFI_PHASE_ROUTER) console_line(T_WIFI_ROUTER);
    if (wifiPhase == MDTEST_WIFI_PHASE_DNS) console_line(T_WIFI_DNS, MDTEST_WIFI_HOST);
    if (wifiPhase == MDTEST_WIFI_PHASE_WEB) console_line(T_WIFI_WEB, MDTEST_WIFI_HOST);
  }
  return 0;
}

/* The firmware's labels for a signal, in dBm (network.c). */
static const char *signalLabel(long rssi) {
  if (rssi >= -30) return "excellent";
  if (rssi >= -40) return "very good";
  if (rssi >= -50) return "good";
  if (rssi >= -60) return "OK";
  if (rssi >= -67) return "fair";
  if (rssi >= -70) return "weak";
  if (rssi >= -80) return "very weak";
  return "unusable";
}

/* net=RSSI|CHANNEL|SECURITY|NAME lines, the strongest first. */
static void wifiNetworks(void) {
  for (const char *line = text; *line;) {
    const char *end = strchr(line, '\n');
    size_t n = end ? (size_t)(end - line) : strlen(line);
    if (strncmp(line, "net=", 4) == 0 && n < 80) {
      char copy[80];
      memcpy(copy, line + 4, n - 4);
      copy[n - 4] = '\0';
      char *field[4] = {copy, 0, 0, 0};
      for (int i = 1; i < 4; i++) {
        field[i] = strchr(field[i - 1], '|');
        if (!field[i]) break;
        *field[i]++ = '\0';
      }
      if (field[3]) {
        console_privateLine(field[3], T_WIFI_NET, field[0], field[1], field[2],
                            field[3]);
      }
    }
    if (!end) break;
    line = end + 1;
  }
}

static void wifiFailure(void) {
  const char *ssid = has("ssid") ? value("ssid") : "";
  console_blank();
  if (chscreen_valueIs(text, "error", "chip silent")) {
    console_line(T_WIFI_CHIP_FAIL);
    console_line(T_WIFI_CHIP_FAIL_DETAIL);
    console_line(T_WIFI_CHIP_FAIL_ADVICE);
    verdict(OUT_FAIL, R_CHIP_SILENT);
  } else if (chscreen_valueIs(text, "error", "scan failed")) {
    console_line(T_WIFI_SCAN_FAIL);
    console_line(T_WIFI_SCAN_FAIL_DETAIL, value("scan_error"));
    console_line(T_WIFI_SCAN_FAIL_ADVICE);
    verdict(OUT_FAIL, R_NONE);
  } else if (chscreen_valueIs(text, "error", "no network configured")) {
    console_line(T_WIFI_NONE);
    console_line(T_WIFI_NONE_DETAIL);
    console_line(T_WIFI_NONE_ADVICE);
    verdict(OUT_WARN, R_WIFI_NONE);
  } else if (chscreen_valueIs(text, "error", "network not found")) {
    console_line(T_WIFI_NOT_FOUND);
    console_privateLine(ssid, T_WIFI_NOT_FOUND_DETAIL, ssid);
    console_line(T_WIFI_NOT_FOUND_ADVICE);
    verdict(OUT_FAIL, R_WIFI_NOT_FOUND);
  } else if (chscreen_valueIs(text, "error", "wrong password")) {
    console_line(T_WIFI_PASSWORD);
    console_privateLine(ssid, T_WIFI_PASSWORD_DETAIL, ssid);
    console_line(T_WIFI_PASSWORD_ADVICE);
    verdict(OUT_FAIL, R_WIFI_JOIN);
  } else if (chscreen_valueIs(text, "error", "no address")) {
    console_line(T_WIFI_NO_ADDRESS);
    console_line(T_WIFI_NO_ADDRESS_DETAIL);
    console_line(T_WIFI_NO_ADDRESS_ADVICE);
    verdict(OUT_FAIL, R_WIFI_JOIN);
  } else if (chscreen_valueIs(text, "error", "no router") ||
             chscreen_valueIs(text, "error", "router silent")) {
    console_line(T_WIFI_ROUTER_FAIL);
    console_line(T_WIFI_ROUTER_FAIL_DETAIL, value("gateway"));
    console_line(T_WIFI_ROUTER_FAIL_ADVICE);
    verdict(OUT_FAIL, R_WIFI_NET);
  } else if (chscreen_valueIs(text, "error", "name not found") ||
             chscreen_valueIs(text, "error", "dns silent")) {
    console_line(T_WIFI_DNS_FAIL);
    console_line(T_WIFI_DNS_FAIL_DETAIL, MDTEST_WIFI_HOST, value("dns"),
                 value("error"));
    console_line(T_WIFI_DNS_FAIL_ADVICE);
    verdict(OUT_FAIL, R_WIFI_NET);
  } else if (chscreen_valueIs(text, "error", "out of memory")) {
    console_line(T_WIFI_MEMORY);
    console_line(T_WIFI_MEMORY_DETAIL, value("error"));
    console_line(T_WIFI_MEMORY_ADVICE);
    verdict(OUT_FAIL, R_NONE);
  } else if (has("server")) {
    console_line(T_WIFI_WEB_FAIL);
    console_line(T_WIFI_WEB_FAIL_DETAIL, MDTEST_WIFI_HOST, value("server"),
                 value("error"));
    console_line(T_WIFI_WEB_FAIL_ADVICE);
    verdict(OUT_FAIL, R_WIFI_NET);
  } else {
    console_line(T_WIFI_JOIN_FAIL);
    console_privateLine(ssid, T_WIFI_JOIN_FAIL_DETAIL, ssid, value("error"));
    console_line(T_WIFI_JOIN_FAIL_ADVICE);
    verdict(OUT_FAIL, R_WIFI_JOIN);
  }
}

void devscreen_wifi(void) {
  console_blank();
  console_line(T_WIFI_TITLE);
  console_line(T_WIFI_CHIP);
  begin();
  wifiPhase = MDTEST_WIFI_PHASE_CHIP;
  int state = chscreen_run(MDTEST_TEST_WIFI, noParams, WIFI_POLL_TICKS,
                           wifiWatch, text);
  if (state < 0) return;
  if (chscreen_valueIs(text, "radio", "ok")) {
    console_line(T_WIFI_RADIO, value("mac"), value("chip_ms"));
    if (has("chip_firmware")) {
      console_line(T_WIFI_FIRMWARE, value("chip_firmware"), value("country"));
    } else {
      console_line(T_WIFI_COUNTRY, value("country"));
    }
  }
  if (has("networks")) {
    console_line(T_WIFI_HEARD, value("networks"), value("hidden"));
    wifiNetworks();
    const char *ssid = has("ssid") ? value("ssid") : 0;
    if (ssid && chscreen_valueIs(text, "configured_seen", "yes")) {
      long rssi = atol(value("configured_rssi"));
      console_privateLine(ssid, T_WIFI_SEEN, ssid, value("configured_rssi"),
                          signalLabel(rssi));
    } else if (ssid) {
      console_privateLine(ssid, T_WIFI_NOT_SEEN, ssid);
    }
  }
  if (chscreen_valueIs(text, "join", "ok")) {
    long rssi = has("rssi") ? atol(value("rssi")) : 0;
    console_line(T_WIFI_JOINED, value("join_ms"),
                 has("rssi") ? value("rssi") : "?",
                 has("rssi") ? signalLabel(rssi) : "?");
    console_line(T_WIFI_ADDR, value("address"), value("mask"));
    console_line(T_WIFI_GATEWAY, value("gateway"), value("dns"),
                 value("addressing"));
  }
  if (has("router_us")) console_line(T_WIFI_ROUTER_OK, value("router_us"));
  if (has("dns_ms")) {
    console_line(T_WIFI_DNS_OK, MDTEST_WIFI_HOST, value("server"),
                 value("dns_ms"));
  }
  if (has("web_ms")) console_line(T_WIFI_WEB_OK, value("web_status"),
                                  value("web_ms"));
  int notSetUp = chscreen_valueIs(text, "wifi", "none");
  if (state == MDTEST_JOB_DONE && notSetUp) {
    /* Not a fault: the radio and the scan were tested, joining cannot be. */
    console_blank();
    console_line(T_WIFI_NOT_SET_UP);
    console_line(T_WIFI_NOT_SET_UP_DETAIL, value("networks"));
    console_line(T_WIFI_NOT_SET_UP_ADVICE);
    verdict(OUT_WARN, R_WIFI_NONE);
  } else if (state == MDTEST_JOB_DONE) {
    console_blank();
    console_line(T_WIFI_PASS);
    verdict(OUT_PASS, R_NONE);
  } else if (state == MDTEST_JOB_FAILED) {
    wifiFailure();
  } else {
    console_line(T_JOB_FAILED, MDTEST_TEST_WIFI);
    verdict(OUT_FAIL, R_NONE);
  }
  if (chscreen_valueIs(text, "networks", "0")) {
    console_line(T_WIFI_NOTHING);
    console_line(T_WIFI_NOTHING_ADVICE);
  }
  if (has("rssi") && atol(value("rssi")) < WIFI_WEAK_DBM) {
    if (state == MDTEST_JOB_DONE) verdict(OUT_WARN, R_WIFI_WEAK);
    console_line(T_WIFI_WEAK);
    console_line(T_WIFI_WEAK_DETAIL, value("rssi"));
    console_line(T_WIFI_WEAK_ADVICE);
  }
  if (chscreen_valueIs(text, "addressing", "static rejected")) {
    console_line(T_WIFI_STATIC);
    console_line(T_WIFI_STATIC_DETAIL, value("static_reason"));
    console_line(T_WIFI_STATIC_ADVICE);
  }
}

/* The Bluetooth test's live view: each step as it begins. */
static unsigned short btPhase;

static int btWatch(const CmdAnswer *a) {
  unsigned short phase = a->data[MDTEST_JOB_ANS_LIVE];
  while (btPhase < phase) {
    btPhase++;
    if (btPhase == MDTEST_BT_PHASE_CLASSIC) {
      console_line(T_BT_CLASSIC, (unsigned)MDTEST_BT_CLASSIC_SECONDS);
    }
    if (btPhase == MDTEST_BT_PHASE_LE) {
      console_line(T_BT_LE, (unsigned)MDTEST_BT_LE_SECONDS);
    }
  }
  return 0;
}

/* dev=RSSI|KIND|ADDRESS|NAME lines, the strongest first. */
static void btDevices(void) {
  for (const char *line = text; *line;) {
    const char *end = strchr(line, '\n');
    size_t n = end ? (size_t)(end - line) : strlen(line);
    if (strncmp(line, "dev=", 4) == 0 && n < 80) {
      char copy[80];
      memcpy(copy, line + 4, n - 4);
      copy[n - 4] = '\0';
      char *field[4] = {copy, 0, 0, 0};
      for (int i = 1; i < 4; i++) {
        field[i] = strchr(field[i - 1], '|');
        if (!field[i]) break;
        *field[i]++ = '\0';
      }
      if (field[3]) {
        console_privateLine(field[3], T_BT_DEVICE, field[0], field[1],
                            field[2], field[3]);
      }
    }
    if (!end) break;
    line = end + 1;
  }
}

void devscreen_bluetooth(void) {
  console_blank();
  console_line(T_BT_TITLE);
  console_line(T_BT_STARTING);
  begin();
  btPhase = MDTEST_BT_PHASE_START;
  int state = chscreen_run(MDTEST_TEST_BLUETOOTH, noParams, BT_POLL_TICKS,
                           btWatch, text);
  if (state < 0) return;
  if (chscreen_valueIs(text, "bt", "ok")) {
    console_line(T_BT_CONTROLLER, value("version"), value("address"),
                 value("start_ms"));
    long maker = atol(value("manufacturer"));
    console_line(T_BT_FIRMWARE, value("firmware"),
                 maker == BT_BROADCOM  ? "Broadcom"
                 : maker == BT_CYPRESS ? "Cypress"
                                       : value("manufacturer"));
  }
  if (has("devices")) {
    console_line(T_BT_HEARD, value("devices"),
                 chscreen_valueIs(text, "devices_more", "yes") ? " or more" : "",
                 value("classic"), value("le"));
    btDevices();
  }
  console_blank();
  if (state == MDTEST_JOB_DONE) {
    console_line(T_BT_PASS);
    verdict(OUT_PASS, R_NONE);
    if (chscreen_valueIs(text, "devices", "0")) {
      verdict(OUT_WARN, R_BT_NOTHING);
      console_line(T_BT_NOTHING);
      console_line(T_BT_NOTHING_ADVICE);
    }
  } else if (chscreen_valueIs(text, "error", "chip silent")) {
    console_line(T_BT_CHIP_FAIL);
    console_line(T_BT_CHIP_FAIL_DETAIL, value("chip_error"));
    console_line(T_BT_CHIP_FAIL_ADVICE);
    verdict(OUT_FAIL, R_CHIP_SILENT);
  } else if (chscreen_valueIs(text, "error", "controller silent") ||
             chscreen_valueIs(text, "error", "controller failed")) {
    console_line(T_BT_START_FAIL);
    console_line(T_BT_START_FAIL_DETAIL, value("error"));
    console_line(T_BT_START_FAIL_ADVICE);
    verdict(OUT_FAIL, R_BT_START);
  } else if (state == MDTEST_JOB_FAILED) {
    console_line(T_BT_SCAN_FAIL);
    console_line(T_BT_SCAN_FAIL_DETAIL, value("error"),
                 has("scan_error") ? value("scan_error") : "-");
    console_line(T_BT_SCAN_FAIL_ADVICE);
    verdict(OUT_FAIL, R_BT_SCAN);
  } else {
    console_line(T_JOB_FAILED, MDTEST_TEST_BLUETOOTH);
    verdict(OUT_FAIL, R_NONE);
  }
}
