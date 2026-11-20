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
/* When a press has lasted this long, the factory reset is near. */
#define SELECT_WARN_MS 5000U
/* The coolest and hottest die temperature that is not worth a word. */
#define TEMP_LOW_C 0
#define TEMP_HIGH_C 70

static char text[CH_RESULT_BYTES];
static const unsigned short noParams[4] = {0, 0, 0, 0};

/* A value, up to its newline, copied (empty when absent). */
static const char *value(const char *key) {
  static char out[4][64];
  static int next;
  char *v = out[next++ & 3];
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
  int state = chscreen_run(MDTEST_TEST_DEVICE_INFO, noParams, INFO_POLL_TICKS,
                           0, text);
  if (state < 0) return;
  if (state != MDTEST_JOB_DONE) {
    console_line(T_JOB_FAILED, MDTEST_TEST_DEVICE_INFO);
    return;
  }
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
    warned = 1;
  } else if (crashed(restart)) {
    console_line(T_INFO_WARN_CRASH);
    console_line(T_INFO_WARN_CRASH_DETAIL, restart);
    console_line(T_INFO_WARN_CRASH_ADVICE);
    warned = 1;
  }
  if (has("temp_c")) {
    int celsius = atoi(value("temp_c"));
    if (celsius < TEMP_LOW_C || celsius > TEMP_HIGH_C) {
      console_line(T_INFO_WARN_TEMP);
      console_line(T_INFO_WARN_TEMP_DETAIL, value("temp_c"), TEMP_LOW_C,
                   TEMP_HIGH_C);
      warned = 1;
    }
  }
  if (!settings || chscreen_valueIs(text, "app_entry", "missing")) {
    console_line(T_INFO_WARN_SETTINGS);
    console_line(T_INFO_WARN_SETTINGS_ADVICE);
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
    return;
  }
  if (chscreen_valueIs(text, "error", "pressed from the start")) {
    console_line(T_SEL_STUCK);
    console_line(T_SEL_STUCK_DETAIL, value("held_ms"));
    console_line(T_SEL_STUCK_ADVICE);
    return;
  }
  console_line(T_SEL_NONE);
  console_line(T_SEL_NONE_DETAIL, (unsigned)MDTEST_SELECT_SECONDS,
               value("error"));
  console_line(T_SEL_NONE_ADVICE);
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
  } else {
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
    return;
  }
  console_line(T_LED_FAIL);
  console_line(T_LED_FAIL_DETAIL, on ? T_YES : T_NO, off ? T_YES : T_NO,
               blinking ? T_YES : T_NO);
  console_line(T_LED_FAIL_ADVICE);
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
