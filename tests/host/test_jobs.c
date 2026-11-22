// units: rp/src/jobs.c common/mdtest.c
/* The job runner (rp/src/jobs.c): one job at a time, a repeated start
 * answered with the same job, unknown tests refused, progress, cancel, and
 * the result as "key=value" lines; the SELECT and LED tests over stand-in
 * hardware; and the window's text and version helpers the commands use
 * (common/mdtest.h). */
#include <string.h>

#include "cardtest.h"
#include "devtests.h"
#include "jobs.h"
#include "mdtest.h"
#include "test.h"

// The hardware, as each test sets it (devtests.h).
static bool selectLevel;
static int chipAnswer;   // devtests_ledChipUp(): 1 started, 0 up, <0 dead
static int ledAnswer;    // devtests_ledSet()
static int ledSets;
static bool ledLevel;
static int released;

void devtests_info(void) { jobs_resultLine("uid=E6614103E7"); }
bool devtests_selectLevel(void) { return selectLevel; }
int devtests_ledChipUp(uint32_t *startMs, int32_t *heapBytes) {
  *startMs = 320;
  *heapBytes = 12000;
  int answer = chipAnswer;
  if (chipAnswer == 1) chipAnswer = 0;
  return answer;
}
int devtests_ledSet(bool on) {
  ledSets++;
  ledLevel = on;
  return ledAnswer;
}
void devtests_ledRelease(void) { released++; }
void devtests_poll(void) {}

// The card tests: jobs.c only steps them (cardtest.c runs under EmuMD).
static uint16_t cardBegun;
static uint16_t cardParam;
static int cardSteps;
static int cardStepsToEnd;
static uint16_t cardEnd;
static int cardAborts;

void cardtest_begin(uint16_t test, const uint16_t params[4], uint32_t nowMs) {
  (void)nowMs;
  cardBegun = test;
  cardParam = params[0];
  cardSteps = 0;
}

uint16_t cardtest_step(uint32_t nowMs, uint16_t *progress,
                       uint16_t live[MDTEST_JOB_LIVE_WORDS]) {
  (void)nowMs;
  cardSteps++;
  *progress = (uint16_t)cardSteps;
  live[0] = (uint16_t)(cardSteps + 10);
  if (cardSteps < cardStepsToEnd) return MDTEST_JOB_RUNNING;
  jobs_resultLine("card=ok");
  return cardEnd;
}

void cardtest_abort(void) { cardAborts++; }

// The Wi-Fi test, the same way (wifitest.c runs under EmuMD).
static int wifiBegun;
static int wifiSteps;
static int wifiAborts;

void wifitest_begin(uint32_t nowMs) {
  (void)nowMs;
  wifiBegun++;
  wifiSteps = 0;
}

uint16_t wifitest_step(uint32_t nowMs, uint16_t *progress,
                       uint16_t live[MDTEST_JOB_LIVE_WORDS]) {
  (void)nowMs;
  wifiSteps++;
  *progress = (uint16_t)(wifiSteps * 10);
  live[0] = (uint16_t)wifiSteps;
  if (wifiSteps < 2) return MDTEST_JOB_RUNNING;
  jobs_resultLine("web=ok");
  return MDTEST_JOB_DONE;
}

void wifitest_abort(void) { wifiAborts++; }

// The Bluetooth test, the same way (bttest.c runs under EmuMD).
static int btBegun;
static int btAborts;

void bttest_begin(uint32_t nowMs) {
  (void)nowMs;
  btBegun++;
}

uint16_t bttest_step(uint32_t nowMs, uint16_t *progress,
                     uint16_t live[MDTEST_JOB_LIVE_WORDS]) {
  (void)nowMs;
  (void)live;
  *progress = 100;
  jobs_resultLine("bt=ok");
  return MDTEST_JOB_DONE;
}

void bttest_abort(void) { btAborts++; }

// The report on the card, the same way (reportsave.c runs under EmuMD).
static int reportBegun;

void reportsave_begin(uint32_t nowMs) {
  (void)nowMs;
  reportBegun++;
}

uint16_t reportsave_step(uint32_t nowMs, uint16_t *progress,
                         uint16_t live[MDTEST_JOB_LIVE_WORDS]) {
  (void)nowMs;
  (void)live;
  *progress = 100;
  jobs_resultLine("file=MDTEST-REPORT-00.TXT");
  return MDTEST_JOB_DONE;
}

void reportsave_abort(void) {}

static int resultHas(const char *line) {
  const JobStatus *s = jobs_status();
  size_t n = strlen(line);
  const char *r = jobs_result();
  for (size_t i = 0; i + n <= s->resultLength; i++) {
    if ((i == 0 || r[i - 1] == '\n') && memcmp(&r[i], line, n) == 0 &&
        r[i + n] == '\n') {
      return 1;
    }
  }
  return 0;
}

static uint16_t seq = 100;

static void start(uint16_t test, uint16_t p0, uint32_t nowMs) {
  uint16_t number;
  const uint16_t params[4] = {p0, 0, 0, 0};
  CHECK_EQ(jobs_start(test, ++seq, params, nowMs, &number), MDTEST_STATUS_OK);
}

// One press and release: done, with its length; bounces under 30 ms are not
// presses.
static void selectPress(void) {
  jobs_reset();
  selectLevel = false;
  start(MDTEST_TEST_SELECT, 5, 0);
  CHECK(jobs_selectOwnsPress(0));
  jobs_poll(0);
  jobs_poll(1000);
  selectLevel = true;  // a bounce
  jobs_poll(1010);
  selectLevel = false;
  jobs_poll(1020);
  jobs_poll(1100);
  CHECK_EQ(jobs_status()->live[0], 0);
  selectLevel = true;
  jobs_poll(2000);
  jobs_poll(2040);
  CHECK_EQ(jobs_status()->live[0], 1);
  jobs_poll(2400);
  CHECK_EQ(jobs_status()->live[1], 400);
  CHECK_EQ(jobs_status()->state, MDTEST_JOB_RUNNING);
  selectLevel = false;
  jobs_poll(2500);
  jobs_poll(2540);
  CHECK_EQ(jobs_status()->state, MDTEST_JOB_DONE);
  // The release it ended on may reach select.c a moment later: still its.
  CHECK(jobs_selectOwnsPress(2600));
  CHECK(!jobs_selectOwnsPress(3040));
  CHECK_EQ(jobs_status()->live[2], 1);
  CHECK(resultHas("presses=1"));
  CHECK(resultHas("press_ms=500"));
}

static void selectNoPress(void) {
  jobs_reset();
  selectLevel = false;
  start(MDTEST_TEST_SELECT, 5, 0);
  jobs_poll(0);
  jobs_poll(2500);
  CHECK_EQ(jobs_status()->progress, 50);
  jobs_poll(5000);
  CHECK_EQ(jobs_status()->state, MDTEST_JOB_FAILED);
  CHECK(resultHas("error=no press"));
}

// Pressed from the start: stuck after 2 s; released sooner, that press is
// not counted (its start was not seen) and the next one is.
static void selectStuck(void) {
  jobs_reset();
  selectLevel = true;
  start(MDTEST_TEST_SELECT, 0, 0);
  jobs_poll(0);
  jobs_poll(1999);
  CHECK_EQ(jobs_status()->state, MDTEST_JOB_RUNNING);
  jobs_poll(2000);
  CHECK_EQ(jobs_status()->state, MDTEST_JOB_FAILED);
  CHECK(resultHas("error=pressed from the start"));

  jobs_reset();
  selectLevel = true;
  start(MDTEST_TEST_SELECT, 0, 0);
  jobs_poll(0);
  selectLevel = false;
  jobs_poll(500);
  jobs_poll(600);
  CHECK_EQ(jobs_status()->state, MDTEST_JOB_RUNNING);
  CHECK_EQ(jobs_status()->live[2], 0);
  selectLevel = true;
  jobs_poll(1000);
  jobs_poll(1100);
  selectLevel = false;
  jobs_poll(1300);
  jobs_poll(1400);
  CHECK_EQ(jobs_status()->state, MDTEST_JOB_DONE);
  CHECK(resultHas("press_ms=300"));
}

static void led(void) {
  jobs_reset();
  chipAnswer = 1;
  ledAnswer = 0;
  start(MDTEST_TEST_LED, MDTEST_LED_ON, 0);
  jobs_poll(0);
  CHECK_EQ(jobs_status()->state, MDTEST_JOB_DONE);
  CHECK(resultHas("chip=started"));
  CHECK(resultHas("chip_start_ms=320"));
  CHECK(resultHas("led=on"));
  CHECK(ledLevel);
  CHECK_EQ(jobs_status()->live[0], 1);

  start(MDTEST_TEST_LED, MDTEST_LED_OFF, 10);
  jobs_poll(10);
  CHECK(resultHas("chip=up"));
  CHECK(resultHas("led=off"));
  CHECK(!ledLevel);

  // Blink: toggles every 500 ms until cancelled.
  ledSets = 0;
  start(MDTEST_TEST_LED, MDTEST_LED_BLINK, 1000);
  jobs_poll(1000);
  jobs_poll(1499);
  CHECK_EQ(ledSets, 1);
  jobs_poll(1500);
  CHECK_EQ(ledSets, 2);
  CHECK(!ledLevel);
  jobs_poll(2000);
  CHECK(ledLevel);
  CHECK_EQ(jobs_status()->state, MDTEST_JOB_RUNNING);
  jobs_cancel();
  CHECK_EQ(jobs_status()->state, MDTEST_JOB_CANCELLED);

  // Reads back otherwise: the chip answers, the LED path does not.
  ledAnswer = 1;
  start(MDTEST_TEST_LED, MDTEST_LED_ON, 3000);
  jobs_poll(3000);
  CHECK_EQ(jobs_status()->state, MDTEST_JOB_FAILED);
  CHECK(resultHas("error=led reads back wrong"));

  released = 0;
  start(MDTEST_TEST_LED, MDTEST_LED_RELEASE, 4000);
  jobs_poll(4000);
  CHECK_EQ(released, 1);
  CHECK(resultHas("chip=off"));

  // The chip does not start: no LED set at all.
  chipAnswer = -1;
  ledSets = 0;
  start(MDTEST_TEST_LED, MDTEST_LED_ON, 5000);
  jobs_poll(5000);
  CHECK_EQ(jobs_status()->state, MDTEST_JOB_FAILED);
  CHECK(resultHas("chip=no answer"));
  CHECK_EQ(ledSets, 0);

  start(MDTEST_TEST_LED, 9, 6000);
  jobs_poll(6000);
  CHECK(resultHas("error=bad mode"));
}

static void deviceInfo(void) {
  jobs_reset();
  start(MDTEST_TEST_DEVICE_INFO, 0, 0);
  jobs_poll(0);
  CHECK_EQ(jobs_status()->state, MDTEST_JOB_DONE);
  CHECK(resultHas("test=device"));
  CHECK(resultHas("uid=E6614103E7"));
}

// The card tests run as jobs: begun with their parameters, stepped until
// they end, their live words passed on, and told when cancelled.
static void card(void) {
  jobs_reset();
  cardStepsToEnd = 3;
  cardEnd = MDTEST_JOB_DONE;
  start(MDTEST_TEST_SD_CARD, 0, 0);
  CHECK_EQ(cardBegun, MDTEST_TEST_SD_CARD);
  jobs_poll(1);
  CHECK_EQ(jobs_status()->state, MDTEST_JOB_RUNNING);
  CHECK_EQ(jobs_status()->live[0], 11);
  jobs_poll(2);
  jobs_poll(3);
  CHECK_EQ(jobs_status()->state, MDTEST_JOB_DONE);
  CHECK_EQ(jobs_status()->progress, 100);
  CHECK(resultHas("card=ok"));

  cardStepsToEnd = 1;
  cardEnd = MDTEST_JOB_FAILED;
  start(MDTEST_TEST_SD_SWAP, 45, 10);
  CHECK_EQ(cardBegun, MDTEST_TEST_SD_SWAP);
  CHECK_EQ(cardParam, 45);
  jobs_poll(11);
  CHECK_EQ(jobs_status()->state, MDTEST_JOB_FAILED);

  cardStepsToEnd = 100;
  cardAborts = 0;
  start(MDTEST_TEST_SD_CARD, 0, 20);
  jobs_poll(21);
  jobs_cancel();
  CHECK_EQ(cardAborts, 1);
  CHECK_EQ(jobs_status()->state, MDTEST_JOB_CANCELLED);
  CHECK(resultHas("cancelled=yes"));

  // Cancelling another test does not touch the card.
  start(MDTEST_TEST_DUMMY, 5, 30);
  jobs_cancel();
  CHECK_EQ(cardAborts, 1);
}

static void wifi(void) {
  jobs_reset();
  wifiBegun = 0;
  start(MDTEST_TEST_WIFI, 0, 0);
  CHECK_EQ(wifiBegun, 1);
  jobs_poll(1);
  CHECK_EQ(jobs_status()->state, MDTEST_JOB_RUNNING);
  CHECK_EQ(jobs_status()->live[0], 1);
  CHECK_EQ(jobs_status()->progress, 10);
  jobs_poll(2);
  CHECK_EQ(jobs_status()->state, MDTEST_JOB_DONE);
  CHECK(resultHas("web=ok"));

  wifiAborts = 0;
  start(MDTEST_TEST_WIFI, 0, 10);
  jobs_cancel();
  CHECK_EQ(wifiAborts, 1);
  CHECK_EQ(jobs_status()->state, MDTEST_JOB_CANCELLED);

  // Past the last test: unknown.
  uint16_t number = 0;
  const uint16_t none[4] = {0, 0, 0, 0};
  CHECK_EQ(jobs_start(MDTEST_TEST_LAST + 1U, 200, none, 20, &number),
           MDTEST_STATUS_UNKNOWN);
}

static void bluetooth(void) {
  jobs_reset();
  btBegun = 0;
  start(MDTEST_TEST_BLUETOOTH, 0, 0);
  CHECK_EQ(btBegun, 1);
  jobs_poll(1);
  CHECK_EQ(jobs_status()->state, MDTEST_JOB_DONE);
  CHECK(resultHas("bt=ok"));
  btAborts = 0;
  start(MDTEST_TEST_BLUETOOTH, 0, 10);
  jobs_cancel();
  CHECK_EQ(btAborts, 1);
}

static const uint16_t twoSeconds[4] = {2, 0, 0, 0};

static void jobs(void) {
  uint16_t number = 99;
  jobs_reset();
  CHECK_EQ(jobs_status()->state, MDTEST_JOB_IDLE);
  CHECK_EQ(jobs_status()->number, 0);

  CHECK_EQ(jobs_start(42, 1, twoSeconds, 0, &number), MDTEST_STATUS_UNKNOWN);
  CHECK_EQ(jobs_start(0, 1, twoSeconds, 0, &number), MDTEST_STATUS_UNKNOWN);
  CHECK_EQ(jobs_status()->state, MDTEST_JOB_IDLE);

  CHECK_EQ(jobs_start(MDTEST_TEST_DUMMY, 1, twoSeconds, 1000, &number),
           MDTEST_STATUS_OK);
  CHECK_EQ(number, 1);
  CHECK_EQ(jobs_status()->state, MDTEST_JOB_RUNNING);

  // The ST missed the answer and sent the start again: the same job.
  CHECK_EQ(jobs_start(MDTEST_TEST_DUMMY, 1, twoSeconds, 1100, &number),
           MDTEST_STATUS_OK);
  CHECK_EQ(number, 1);
  // Another start while it runs: refused, with the running job's number.
  CHECK_EQ(jobs_start(MDTEST_TEST_DUMMY, 2, twoSeconds, 1200, &number),
           MDTEST_STATUS_BUSY);
  CHECK_EQ(number, 1);

  jobs_poll(2000);
  CHECK_EQ(jobs_status()->state, MDTEST_JOB_RUNNING);
  CHECK_EQ(jobs_status()->progress, 50);
  CHECK_EQ(jobs_status()->resultLength, 0);
  jobs_poll(3000);
  CHECK_EQ(jobs_status()->state, MDTEST_JOB_DONE);
  CHECK_EQ(jobs_status()->progress, 100);
  const char *expected = "test=dummy\nseconds=2\nsteps=2\n";
  CHECK_EQ(jobs_status()->resultLength, strlen(expected));
  CHECK(memcmp(jobs_result(), expected, strlen(expected)) == 0);
  jobs_poll(4000);  // ended: nothing changes
  CHECK_EQ(jobs_status()->resultLength, strlen(expected));

  // A new sequence number starts a new job; cancel ends it.
  CHECK_EQ(jobs_start(MDTEST_TEST_DUMMY, 2, twoSeconds, 5000, &number),
           MDTEST_STATUS_OK);
  CHECK_EQ(number, 2);
  CHECK_EQ(jobs_status()->resultLength, 0);
  jobs_cancel();
  CHECK_EQ(jobs_status()->state, MDTEST_JOB_CANCELLED);
  CHECK(memcmp(jobs_result(), "cancelled=yes\n", 14) == 0);
  jobs_cancel();  // nothing runs: no change
  CHECK_EQ(jobs_status()->state, MDTEST_JOB_CANCELLED);

  // An RP restart forgets every job; numbering starts again.
  jobs_reset();
  CHECK_EQ(jobs_start(MDTEST_TEST_DUMMY, 2, twoSeconds, 0, &number),
           MDTEST_STATUS_OK);
  CHECK_EQ(number, 1);
}

static void text(void) {
  uint16_t words[4];
  char back[8];
  mdtest_putText(words, "abcde", 5);
  CHECK_EQ(words[0], 0x6162);
  CHECK_EQ(words[2], 0x6500);
  mdtest_getText(words, back, 5);
  CHECK(memcmp(back, "abcde", 5) == 0);
}

static void restore(void) {
  static uint16_t window[MDTEST_WINDOW_WORDS];
  mdtest_fillWindow(window, "v2.0.0", "test");
  uint16_t header[MDTEST_HEADER_WORDS];
  memcpy(header, &window[MDTEST_HEADER_INDEX], sizeof(header));
  memset(&window[0x0F00], 0, 0x300 * 2);  // across the header
  mdtest_restoreWords(window, 0x0F00, 0x300);
  int wrong = 0;
  for (uint32_t i = 0x0F00; i < 0x1200; i++) {
    int inHeader = i >= MDTEST_HEADER_INDEX &&
                   i < MDTEST_HEADER_INDEX + MDTEST_HEADER_WORDS;
    if (!inHeader && window[i] != mdtest_patternWord((uint16_t)i)) wrong++;
  }
  CHECK_EQ(wrong, 0);
  // The header's words are the caller's: left as they were (here zeroed).
  CHECK_EQ(window[MDTEST_HEADER_INDEX], 0);
}

static void versions(void) {
  CHECK_EQ(mdtest_updateAdvice(1, 1), MDTEST_UPDATE_NOTHING);
  CHECK_EQ(mdtest_updateAdvice(2, 1), MDTEST_UPDATE_FIRMWARE);
  CHECK_EQ(mdtest_updateAdvice(1, 2), MDTEST_UPDATE_PROGRAM);
}

int main(void) {
  jobs();
  selectPress();
  selectNoPress();
  selectStuck();
  led();
  deviceInfo();
  card();
  wifi();
  bluetooth();
  text();
  restore();
  versions();
  TEST_END();
}
