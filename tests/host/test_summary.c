// units: app/atarist/src/summary.c
/* The guided run's summary (app/atarist/src/summary.c): for each row of the
 * fault catalogue the run can provoke, the results it gives and the
 * conclusion the summary must draw from them; a silent radio chip said
 * once; a stop and an unknown failure said as such. */
#include <stdio.h>
#include <string.h>

#include "summary.h"
#include "test.h"
#include "texts.h"

static char said[4096];

static void put(const char *line, void *ctx) {
  (void)ctx;
  strncat(said, line, sizeof(said) - strlen(said) - 2);
  strcat(said, "\n");
}

static void summarise(const Results *r) {
  said[0] = '\0';
  summary_write(r, put, 0);
}

static int has(const char *text) { return strstr(said, text) != 0; }

static int count(const char *text) {
  int n = 0;
  for (const char *at = said; (at = strstr(at, text)) != 0; at++) n++;
  return n;
}

/* A good device as far as the automatic tests go. */
static void good(Results *r) {
  summary_clear(r);
  for (int i = AREA_LINES; i <= AREA_BLUETOOTH; i++) r->outcome[i] = OUT_PASS;
}

static void set(Results *r, Area area, Outcome o, Reason reason) {
  r->outcome[area] = o;
  r->reason[area] = reason;
}

static void allGood(void) {
  Results r;
  good(&r);
  summarise(&r);
  CHECK(has("  Lines         PASS\n"));
  CHECK(has("  Bluetooth     PASS\n"));
  CHECK(has("  SELECT        not run\n"));
  CHECK(has("* " T_C_ALL_GOOD));
  CHECK(has("* " T_C_HANDS_NOT_RUN));

  for (int i = AREA_SELECT; i < AREA_COUNT; i++) r.outcome[i] = OUT_PASS;
  summarise(&r);
  CHECK(has("* " T_C_ALL_GOOD));
  CHECK(!has(T_C_HANDS_NOT_RUN));
}

/* The line test failing locks everything after it: one finding. */
static void lines(void) {
  Results r;
  summary_clear(&r);
  for (int i = 0; i < AREA_COUNT; i++) r.outcome[i] = OUT_LOCKED;
  set(&r, AREA_LINES, OUT_FAIL, R_LINES);
  summarise(&r);
  CHECK(has("  Lines         FAIL\n"));
  CHECK(has("  Wi-Fi         LOCKED\n"));
  CHECK(has("* " T_C_LINES));
  CHECK_EQ(count("* "), 1);
}

static void commands(void) {
  Results r;
  summary_clear(&r);
  for (int i = 0; i < AREA_COUNT; i++) r.outcome[i] = OUT_LOCKED;
  r.outcome[AREA_LINES] = OUT_PASS;
  set(&r, AREA_COMMANDS, OUT_FAIL, R_CMD_CACHE);
  summarise(&r);
  CHECK(has("* " T_C_CMD_CACHE));
  CHECK(!has(T_C_HANDS_NOT_RUN));
  set(&r, AREA_COMMANDS, OUT_FAIL, R_CMD);
  summarise(&r);
  CHECK(has("* " T_C_CMD));
}

/* One area at a time, each catalogue row its conclusion. */
static void rows(void) {
  static const struct {
    Area area;
    Outcome outcome;
    Reason reason;
    const char *conclusion;
  } cases[] = {
      {AREA_DEVICE, OUT_WARN, R_DEV_CRASH, T_C_DEV_CRASH},
      {AREA_DEVICE, OUT_WARN, R_DEV_SETTINGS, T_C_DEV_SETTINGS},
      {AREA_CARD, OUT_FAIL, R_CARD_NONE, T_C_CARD_NONE},
      {AREA_CARD, OUT_FAIL, R_CARD_START, T_C_CARD_START},
      {AREA_CARD, OUT_FAIL, R_CARD_FORMAT, T_C_CARD_FORMAT},
      {AREA_CARD, OUT_FAIL, R_CARD_FULL, T_C_CARD_FULL},
      {AREA_CARD, OUT_FAIL, R_CARD_WRITE, T_C_CARD_WRITE},
      {AREA_CARD, OUT_WARN, R_CARD_SLOW, T_C_CARD_SLOW},
      {AREA_WIFI, OUT_WARN, R_WIFI_NONE, T_C_WIFI_NONE},
      {AREA_WIFI, OUT_FAIL, R_WIFI_NOT_FOUND, T_C_WIFI_NOT_FOUND},
      {AREA_WIFI, OUT_FAIL, R_WIFI_JOIN, T_C_WIFI_JOIN},
      {AREA_WIFI, OUT_FAIL, R_WIFI_NET, T_C_WIFI_NET},
      {AREA_WIFI, OUT_WARN, R_WIFI_WEAK, T_C_WIFI_WEAK},
      {AREA_BLUETOOTH, OUT_FAIL, R_BT_SCAN, T_C_BT_SCAN},
      {AREA_BLUETOOTH, OUT_WARN, R_BT_NOTHING, T_C_BT_NOTHING},
      {AREA_SELECT, OUT_FAIL, R_SEL_NONE, T_C_SEL_NONE},
      {AREA_SELECT, OUT_FAIL, R_SEL_STUCK, T_C_SEL_STUCK},
      {AREA_LED, OUT_FAIL, R_LED_UNSEEN, T_C_LED},
      {AREA_CARD_SWAP, OUT_FAIL, R_SWAP, T_C_SWAP},
      {AREA_CARD, OUT_FAIL, R_LOST, T_C_LOST},
  };
  for (unsigned i = 0; i < sizeof(cases) / sizeof(cases[0]); i++) {
    Results r;
    good(&r);
    set(&r, cases[i].area, cases[i].outcome, cases[i].reason);
    summarise(&r);
    if (!has(cases[i].conclusion)) printf("  case %u: %s\n", i, said);
    CHECK(has(cases[i].conclusion));
    CHECK(!has(T_C_ALL_GOOD));
    CHECK_EQ(count("* "), cases[i].area >= AREA_SELECT ? 1 : 2);
  }
}

/* A silent chip stops Wi-Fi, Bluetooth and the LED: one finding. */
static void silentChip(void) {
  Results r;
  good(&r);
  set(&r, AREA_WIFI, OUT_FAIL, R_CHIP_SILENT);
  set(&r, AREA_BLUETOOTH, OUT_SKIP, R_CHIP_SILENT);
  set(&r, AREA_SELECT, OUT_PASS, R_NONE);
  set(&r, AREA_LED, OUT_SKIP, R_CHIP_SILENT);
  set(&r, AREA_CARD_SWAP, OUT_PASS, R_NONE);
  summarise(&r);
  CHECK(has("  Bluetooth     SKIP\n"));
  CHECK_EQ(count(T_C_CHIP), 1);
  CHECK_EQ(count("* "), 1);
}

/* Bluetooth not starting: with Wi-Fi working it is the chip's Bluetooth
 * side; with Wi-Fi failing too it says only that it does not start. */
static void bluetoothHalf(void) {
  Results r;
  good(&r);
  set(&r, AREA_BLUETOOTH, OUT_FAIL, R_BT_START);
  summarise(&r);
  CHECK(has(T_C_BT_HALF));
  set(&r, AREA_WIFI, OUT_FAIL, R_WIFI_JOIN);
  summarise(&r);
  CHECK(!has(T_C_BT_HALF));
  CHECK(has(T_C_BT_START));
}

static void stoppedAndUnknown(void) {
  Results r;
  good(&r);
  set(&r, AREA_WIFI, OUT_STOPPED, R_NONE);
  r.outcome[AREA_BLUETOOTH] = OUT_NOT_RUN;
  summarise(&r);
  CHECK(has("  Wi-Fi         stopped\n"));
  CHECK(has(T_C_STOPPED));
  CHECK(!has(T_C_ALL_GOOD));

  good(&r);
  set(&r, AREA_CARD, OUT_FAIL, R_NONE);
  summarise(&r);
  CHECK(has(T_C_UNKNOWN));
  CHECK(!has(T_C_ALL_GOOD));
}

int main(void) {
  allGood();
  lines();
  commands();
  rows();
  silentChip();
  bluetoothHalf();
  stoppedAndUnknown();
  TEST_END();
}
