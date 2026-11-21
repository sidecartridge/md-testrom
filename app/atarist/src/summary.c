#include "summary.h"

#include <stdio.h>

#include "texts.h"

typedef void (*Put)(const char *line, void *ctx);

static const char *const areaNames[AREA_COUNT] = {
    T_AREA_LINES, T_AREA_COMMANDS,  T_AREA_DEVICE, T_AREA_CARD,     T_AREA_WIFI,
    T_AREA_BLUETOOTH, T_AREA_SELECT, T_AREA_LED,   T_AREA_CARD_SWAP};

static const char *outcomeName(Outcome o) {
  switch (o) {
    case OUT_PASS:
      return T_OUT_PASS;
    case OUT_WARN:
      return T_OUT_WARN;
    case OUT_FAIL:
      return T_OUT_FAIL;
    case OUT_SKIP:
      return T_OUT_SKIP;
    case OUT_LOCKED:
      return T_OUT_LOCKED;
    case OUT_STOPPED:
      return T_OUT_STOPPED;
    default:
      return T_OUT_NOT_RUN;
  }
}

void summary_clear(Results *r) {
  for (int i = 0; i < AREA_COUNT; i++) {
    r->outcome[i] = OUT_NOT_RUN;
    r->reason[i] = R_NONE;
  }
}

/* What a reason means and what to do: the fault catalogue's rows. 0 for a
 * reason with nothing to say. */
static int finding(const Results *r, Reason reason, const char **conclusion,
                   const char **advice) {
  switch (reason) {
    case R_LOST:
      *conclusion = T_C_LOST, *advice = T_A_LOST;
      return 1;
    case R_LINES:
      *conclusion = T_C_LINES, *advice = T_A_LINES;
      return 1;
    case R_CMD_CACHE:
      *conclusion = T_C_CMD_CACHE, *advice = T_A_CMD_CACHE;
      return 1;
    case R_CMD:
      *conclusion = T_C_CMD, *advice = T_A_CMD;
      return 1;
    case R_DEV_CRASH:
      *conclusion = T_C_DEV_CRASH, *advice = T_A_DEV_CRASH;
      return 1;
    case R_DEV_TEMP:
      *conclusion = T_C_DEV_TEMP, *advice = T_A_DEV_TEMP;
      return 1;
    case R_DEV_SETTINGS:
      *conclusion = T_C_DEV_SETTINGS, *advice = T_A_DEV_SETTINGS;
      return 1;
    case R_CARD_NONE:
      *conclusion = T_C_CARD_NONE, *advice = T_A_CARD_NONE;
      return 1;
    case R_CARD_START:
      *conclusion = T_C_CARD_START, *advice = T_A_CARD_START;
      return 1;
    case R_CARD_OLD:
      *conclusion = T_C_CARD_OLD, *advice = T_A_CARD_OLD;
      return 1;
    case R_CARD_FORMAT:
      *conclusion = T_C_CARD_FORMAT, *advice = T_A_CARD_FORMAT;
      return 1;
    case R_CARD_FULL:
      *conclusion = T_C_CARD_FULL, *advice = T_A_CARD_FULL;
      return 1;
    case R_CARD_LOCKED:
      *conclusion = T_C_CARD_LOCKED, *advice = T_A_CARD_LOCKED;
      return 1;
    case R_CARD_WRITE:
      *conclusion = T_C_CARD_WRITE, *advice = T_A_CARD_WRITE;
      return 1;
    case R_CARD_SLOW:
      *conclusion = T_C_CARD_SLOW, *advice = T_A_CARD_SLOW;
      return 1;
    case R_CHIP_SILENT:
      *conclusion = T_C_CHIP, *advice = T_A_CHIP;
      return 1;
    case R_WIFI_NONE:
      *conclusion = T_C_WIFI_NONE, *advice = T_A_WIFI_NONE;
      return 1;
    case R_WIFI_NOT_FOUND:
      *conclusion = T_C_WIFI_NOT_FOUND, *advice = T_A_WIFI_NOT_FOUND;
      return 1;
    case R_WIFI_JOIN:
      *conclusion = T_C_WIFI_JOIN, *advice = T_A_WIFI_JOIN;
      return 1;
    case R_WIFI_NET:
      *conclusion = T_C_WIFI_NET, *advice = T_A_WIFI_NET;
      return 1;
    case R_WIFI_WEAK:
      *conclusion = T_C_WIFI_WEAK, *advice = T_A_WIFI_WEAK;
      return 1;
    case R_BT_START: {
      /* The chip's two halves told apart: Wi-Fi working puts the fault on
       * its Bluetooth side. */
      Outcome wifi = r->outcome[AREA_WIFI];
      *conclusion = wifi == OUT_PASS || wifi == OUT_WARN ? T_C_BT_HALF
                                                         : T_C_BT_START;
      *advice = T_A_BT_START;
      return 1;
    }
    case R_BT_SCAN:
      *conclusion = T_C_BT_SCAN, *advice = T_A_BT_SCAN;
      return 1;
    case R_BT_NOTHING:
      *conclusion = T_C_BT_NOTHING, *advice = T_A_BT_NOTHING;
      return 1;
    case R_SEL_NONE:
      *conclusion = T_C_SEL_NONE, *advice = T_A_SEL_NONE;
      return 1;
    case R_SEL_STUCK:
      *conclusion = T_C_SEL_STUCK, *advice = T_A_SEL_STUCK;
      return 1;
    case R_LED_UNSEEN:
      *conclusion = T_C_LED, *advice = T_A_LED;
      return 1;
    case R_SWAP:
      *conclusion = T_C_SWAP, *advice = T_A_SWAP;
      return 1;
    default:
      return 0;
  }
}

static void say(Put put, void *ctx, const char *format, const char *text) {
  char line[160];
  snprintf(line, sizeof(line), format, text);
  put(line, ctx);
}

void summary_write(const Results *r, Put put, void *ctx) {
  char line[160];
  put("", ctx);
  put(T_SUM_TITLE, ctx);
  for (int i = 0; i < AREA_COUNT; i++) {
    snprintf(line, sizeof(line), T_SUM_AREA, areaNames[i],
             outcomeName(r->outcome[i]));
    put(line, ctx);
  }

  put("", ctx);
  put(T_SUM_FINDINGS, ctx);
  /* Each reason once (a silent chip is one finding, however many areas it
   * stopped), in the areas' order. */
  int said[R_SWAP + 1] = {0};
  int findings = 0, unexplained = 0, stopped = 0, handsRun = 0;
  for (int i = 0; i < AREA_COUNT; i++) {
    Outcome o = r->outcome[i];
    if (o == OUT_STOPPED) stopped = 1;
    if (i >= AREA_SELECT && o != OUT_NOT_RUN) handsRun = 1;
    if (o != OUT_FAIL && o != OUT_WARN && o != OUT_SKIP) continue;
    Reason reason = r->reason[i];
    const char *conclusion, *advice;
    if (!finding(r, reason, &conclusion, &advice)) {
      if (o == OUT_FAIL) unexplained = 1;
      continue;
    }
    if (said[reason]) continue;
    said[reason] = 1;
    say(put, ctx, T_SUM_FINDING, conclusion);
    say(put, ctx, T_SUM_ADVICE, advice);
    findings++;
  }
  if (unexplained) {
    say(put, ctx, T_SUM_FINDING, T_C_UNKNOWN);
    say(put, ctx, T_SUM_ADVICE, T_A_UNKNOWN);
    findings++;
  }
  if (stopped) {
    say(put, ctx, T_SUM_FINDING, T_C_STOPPED);
    say(put, ctx, T_SUM_ADVICE, T_A_STOPPED);
    return;
  }
  if (findings == 0) {
    say(put, ctx, T_SUM_FINDING, T_C_ALL_GOOD);
    say(put, ctx, T_SUM_ADVICE, T_A_ALL_GOOD);
  }
  if (!handsRun && r->outcome[AREA_COMMANDS] == OUT_PASS) {
    say(put, ctx, T_SUM_FINDING, T_C_HANDS_NOT_RUN);
    say(put, ctx, T_SUM_ADVICE, T_A_HANDS_NOT_RUN);
  }
}
