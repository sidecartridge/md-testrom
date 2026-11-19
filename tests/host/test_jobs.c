// units: rp/src/jobs.c common/mdtest.c
/* The job runner (rp/src/jobs.c): one job at a time, a repeated start
 * answered with the same job, unknown tests refused, progress, cancel, and
 * the result as "key=value" lines; and the window's text and version
 * helpers the commands use (common/mdtest.h). */
#include <string.h>

#include "jobs.h"
#include "mdtest.h"
#include "test.h"

static const uint16_t twoSeconds[4] = {2, 0, 0, 0};

static void jobs(void) {
  uint16_t number = 99;
  jobs_reset();
  CHECK_EQ(jobs_status()->state, MDTEST_JOB_IDLE);
  CHECK_EQ(jobs_status()->number, 0);

  CHECK_EQ(jobs_start(42, 1, twoSeconds, 0, &number), MDTEST_STATUS_UNKNOWN);
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
  text();
  restore();
  versions();
  TEST_END();
}
