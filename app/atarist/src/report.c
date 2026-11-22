#include "report.h"

#include <stdio.h>
#include <string.h>

#include "chscreen.h"
#include "commands.h"
#include "console.h"
#include "hw.h"
#include "mdtest.h"
#include "texts.h"

#define REPORT_COLUMNS 81 /* lines of 80 characters at most */
#define REPORT_LINE_BYTES 200
#define POLL_TICKS 10UL /* 50 ms between looks at the job */
#define BUSY_TRIES 400  /* a chunk refused this often: the card has hung */

typedef struct {
  char data[MDTEST_REPORT_CHUNK_BYTES];
  unsigned short length;
  unsigned short job;
  int error;
} Writer;

static unsigned short noParams[4];

static void wait(unsigned long ticks) {
  unsigned long start = hw_ticks;
  while (hw_ticks - start < ticks) {
  }
}

/* Network and device names go into a report only if the user agrees. */
static int askKeepNames(void) {
  console_line(T_SAVE_ASK_NAMES);
  for (;;) {
    int key = console_key();
    if (key == 'y' || key == 'n') {
      console_line(key == 'y' ? T_SAVE_NAMES_KEPT : T_SAVE_NAMES_LEFT_OUT);
      return key == 'y';
    }
  }
}

/* A report line, with a name replaced unless it is kept. */
static const char *reportLine(int index, int keepNames) {
  static char out[REPORT_LINE_BYTES];
  const char *line = console_reportLine(index);
  int start;
  int length = console_reportPrivate(index, &start);
  if (keepNames || length == 0) return line;
  snprintf(out, sizeof(out), "%.*s%s%s", start, line, T_NETWORK_LEFT_OUT,
           line + start + length);
  return out;
}

/* One chunk to the firmware, sent again while it is still writing the last
 * (the job writes from the device's main loop). */
static int sendChunk(Writer *w) {
  for (int tries = 0; tries < BUSY_TRIES; tries++) {
    CmdAnswer a;
    if (cmd_reportData(w->data, w->length, &a) != CMD_OK) return 0;
    if (a.status == MDTEST_STATUS_OK) {
      w->length = 0;
      return 1;
    }
    if (a.status != MDTEST_STATUS_BUSY) return 0;
    wait(1);
  }
  return 0;
}

/* A line of the file, with its CR LF; chunks sent as they fill. */
static void putFile(const char *line, void *ctx) {
  Writer *w = ctx;
  if (w->error) return;
  unsigned short n = (unsigned short)strlen(line);
  if (w->length + n + 2 > sizeof(w->data) && !sendChunk(w)) {
    w->error = 1;
    return;
  }
  memcpy(w->data + w->length, line, n);
  w->length += n;
  w->data[w->length++] = '\r';
  w->data[w->length++] = '\n';
}

/* The job's status until it is ready for the text (1) or has ended (0, with
 * its result in text). */
static int waitForJob(unsigned short job, int ready, char *text) {
  for (;;) {
    CmdAnswer a;
    if (!chscreen_status(MDTEST_TEST_REPORT, job, &a)) return -1;
    if (a.data[MDTEST_JOB_ANS_STATE] != MDTEST_JOB_RUNNING) {
      unsigned short length = a.length;
      if (length > CH_RESULT_BYTES - 1) length = CH_RESULT_BYTES - 1;
      cmd_resultText(text, length);
      text[length] = '\0';
      return a.data[MDTEST_JOB_ANS_STATE] == MDTEST_JOB_DONE ? 0 : 2;
    }
    if (ready && a.data[MDTEST_JOB_ANS_LIVE] == MDTEST_REPORT_PHASE_READY) {
      return 1;
    }
    wait(POLL_TICKS);
  }
}

static void saveFailed(const char *why) {
  console_line(T_SAVE_FAILED);
  console_line(T_SAVE_FAILED_DETAIL, why);
  console_line(T_SAVE_ADVICE);
}

void report_save(const char *gate, int channelOpen, long mch) {
  static char text[CH_RESULT_BYTES];
  static Writer w;
  if (!channelOpen) {
    console_line(T_SAVE_LOCKED);
    console_line(T_SAVE_LOCKED_DETAIL);
    console_line(T_SAVE_LOCKED_ADVICE);
    return;
  }

  /* The lines to write, counted now: the outcome is printed after. */
  int lines = console_reportLines();
  int keepNames = 1;
  for (int i = 0; i < lines; i++) {
    int start;
    if (console_reportPrivate(i, &start)) {
      keepNames = askKeepNames();
      break;
    }
  }
  console_line(T_SAVING);
  if (!chscreen_open(mch)) return;
  memset(&w, 0, sizeof(w));
  if (!chscreen_start(MDTEST_TEST_REPORT, noParams, &w.job)) return;
  int state = waitForJob(w.job, 1, text);
  if (state < 0) return;
  if (state != 1) {
    const char *error = chscreen_value(text, "error");
    saveFailed(error ? error : T_SAVE_NO_DEVICE);
    return;
  }

  putFile(T_REPORT_FORMAT, &w);
  putFile(gate, &w);
  for (int i = 0; i < lines && !w.error; i++) {
    console_wrap(reportLine(i, keepNames), REPORT_COLUMNS, putFile, &w);
  }
  if (console_reportDropped() && !w.error) {
    char full[REPORT_COLUMNS];
    snprintf(full, sizeof(full), T_REPORT_FULL, console_reportDropped());
    putFile(full, &w);
  }
  if (!w.error && w.length && !sendChunk(&w)) w.error = 1;
  if (w.error || !sendChunk(&w)) { /* the empty chunk ends the file */
    chscreen_cancel();
    saveFailed(T_SAVE_NO_DEVICE);
    return;
  }
  state = waitForJob(w.job, 0, text);
  if (state < 0) return;
  if (state != 0) {
    const char *error = chscreen_value(text, "error");
    saveFailed(error ? error : T_SAVE_NO_DEVICE);
    return;
  }
  char name[32];
  const char *file = chscreen_value(text, "file");
  snprintf(name, sizeof(name), "%s", file ? file : "?");
  char *end = strchr(name, '\n');
  if (end) *end = '\0';
  console_line(T_SAVED, name);
}
