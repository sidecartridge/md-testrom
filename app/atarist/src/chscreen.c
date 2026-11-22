#include "chscreen.h"

#include <string.h>

#include "commands.h"
#include "console.h"
#include "hw.h"
#include "mdtest.h"
#include "texts.h"

/* The protocol this program speaks; a test build overrides it to show a
 * mismatch. */
#ifndef PROGRAM_PROTOCOL_VERSION
#define PROGRAM_PROTOCOL_VERSION MDTEST_PROTOCOL_VERSION
#endif

#define HZ_200 hw_ticks

#define HELLO_BURST 500UL
#define ECHO_ROUNDS 10000UL
#define ECHO_SMALL_WORDS 64U
#define JOB_SECONDS 2U
#define JOB_POLL_EVERY 20UL
#define JOB_POLL_TICKS 50UL /* 250 ms between polls of a plain job */
/* How long a job's status may go unanswered: a step that holds the RP's
 * loop, such as starting the radio chip, before the device counts as
 * silent. */
#define JOB_SILENCE_TICKS (200UL * 5)

static unsigned long bootSeen;
static int restarted;
static unsigned long restartFrom, restartTo;
static unsigned long wrongAnswers;
static unsigned short echoWords[MDTEST_ECHO_MAX_WORDS];
static unsigned short jobSeq;

static void sawBoot(const CmdAnswer *a) {
  if (bootSeen && a->boot != bootSeen && !restarted) {
    restarted = 1;
    restartFrom = bootSeen;
    restartTo = a->boot;
  }
  bootSeen = a->boot;
}

static int hello(long mch, CmdAnswer *a) {
  unsigned short words[3] = {PROGRAM_PROTOCOL_VERSION,
                             (unsigned short)((unsigned long)mch >> 16),
                             (unsigned short)mch};
  if (cmd_send(MDTEST_CMD_HELLO, words, 3, a) != CMD_OK) return 0;
  sawBoot(a);
  return 1;
}

/* show: the hello's lines on the screen (and so in the report). */
static int openChannel(long mch, int show) {
  CmdAnswer a;
  bootSeen = 0;
  restarted = 0;
  if (!cmd_selected() && cmd_select() != CMD_OK) {
    console_line(T_CH_NO_SELECT);
    console_line(T_CH_NO_SELECT_DETAIL);
    console_line(T_CH_NO_SELECT_ADVICE);
    return 0;
  }
  if (!hello(mch, &a)) {
    console_line(T_CH_NO_HELLO);
    console_line(T_CH_NO_HELLO_DETAIL);
    console_line(T_CH_NO_HELLO_ADVICE);
    return 0;
  }
  unsigned short firmware = a.data[0];
  int update = mdtest_updateAdvice(PROGRAM_PROTOCOL_VERSION, firmware);
  if (update != MDTEST_UPDATE_NOTHING) {
    console_line(T_CH_VERSION);
    console_line(T_CH_VERSION_DETAIL, (unsigned)PROGRAM_PROTOCOL_VERSION,
                 firmware);
    console_line(update == MDTEST_UPDATE_FIRMWARE ? T_FOUND_UPDATE_FIRMWARE
                                                  : T_FOUND_UPDATE_PROGRAM);
    return 0;
  }
  /* From here every line also goes to the RP. */
  console_setMirror(cmd_log);
  if (show) {
    console_line(T_CH_HELLO);
    console_line(T_CH_HELLO_DETAIL, firmware, a.boot);
  }
  return 1;
}

/* The hello is shown when the channel opens, not before every test that
 * finds it open already. */
int chscreen_open(long mch) { return openChannel(mch, !cmd_selected()); }

int chscreen_greet(long mch) { return openChannel(mch, 1); }

/* One echo of count words, checked; 1 when answered and right. */
static int echo(unsigned long round, unsigned short count) {
  for (unsigned short k = 0; k < count; k++) {
    echoWords[k] = (unsigned short)(round * 31UL + k * 0x9E37U);
  }
  CmdAnswer a;
  if (cmd_send(MDTEST_CMD_ECHO, echoWords, count, &a) != CMD_OK) return 0;
  sawBoot(&a);
  if (a.command != MDTEST_CMD_ECHO || a.status != MDTEST_STATUS_OK ||
      a.length != count) {
    wrongAnswers++;
    return 0;
  }
  const volatile unsigned short *result = cmd_result();
  for (unsigned short k = 0; k < count; k++) {
    if (result[k] != echoWords[k]) {
      wrongAnswers++;
      return 0;
    }
  }
  return 1;
}

static unsigned short echoSize(unsigned long round) {
  if (round % 100UL == 0) return MDTEST_ECHO_MAX_WORDS;
  return (unsigned short)((round * 7UL) % ECHO_SMALL_WORDS);
}

static int counters(unsigned long values[6]) {
  CmdAnswer a;
  if (cmd_send(MDTEST_CMD_COUNTERS, 0, 0, &a) != CMD_OK) return 0;
  sawBoot(&a);
  const volatile unsigned short *r = cmd_result();
  for (int i = 0; i < 6; i++) {
    values[i] = ((unsigned long)r[2 * i] << 16) | r[2 * i + 1];
  }
  return 1;
}

static int jobStart(unsigned short test, const unsigned short params[4],
                    unsigned short *number) {
  unsigned short words[6] = {test,      ++jobSeq,  params[0],
                             params[1], params[2], params[3]};
  CmdAnswer a;
  if (cmd_send(MDTEST_CMD_JOB_START, words, 6, &a) != CMD_OK) return 0;
  sawBoot(&a);
  *number = a.data[0];
  return a.status == MDTEST_STATUS_OK;
}

/* The job's status; 1 when answered. */
static int jobStatus(CmdAnswer *a) {
  if (cmd_send(MDTEST_CMD_JOB_STATUS, 0, 0, a) != CMD_OK) return 0;
  sawBoot(a);
  return 1;
}

/* The job's status, waiting out a step that holds the RP's loop (a chip
 * starting): 1 when answered; 0 once the RP restarted or stayed silent, said
 * on the screen. */
static int jobStatusPatient(unsigned short test, unsigned short number,
                            CmdAnswer *a) {
  unsigned long start = HZ_200;
  while (!jobStatus(a)) {
    if (!cmd_selected()) {
      /* The RP started again: ROM3 is test data, the job is gone. */
      console_line(T_JOB_RESTARTED, test);
      console_line(T_JOB_RESTARTED_DETAIL);
      return 0;
    }
    if (HZ_200 - start >= JOB_SILENCE_TICKS) {
      console_line(T_JOB_FAILED, test);
      console_line(T_JOB_FAILED_DETAIL, number, 0);
      return 0;
    }
  }
  if (bootSeen && restarted) {
    console_line(T_JOB_RESTARTED, test);
    console_line(T_CH_RESTARTED_DETAIL, restartFrom, restartTo);
    return 0;
  }
  return 1;
}

static void printResult(const char *text) {
  static char copy[CH_RESULT_BYTES];
  strcpy(copy, text);
  char *line = copy;
  for (char *p = copy; *p; p++) {
    if (*p == '\n') {
      *p = '\0';
      if (*line) console_line(T_JOB_RESULT, line);
      line = p + 1;
    }
  }
  if (*line) console_line(T_JOB_RESULT, line);
}

int chscreen_start(unsigned short test, const unsigned short params[4],
                   unsigned short *number) {
  if (jobStart(test, params, number)) return 1;
  console_line(T_JOB_FAILED, test);
  console_line(T_JOB_FAILED_DETAIL, 0, 0);
  return 0;
}

int chscreen_status(unsigned short test, unsigned short number,
                    CmdAnswer *a) {
  return jobStatusPatient(test, number, a);
}

void chscreen_cancel(void) { cmd_send(MDTEST_CMD_JOB_CANCEL, 0, 0, 0); }

int chscreen_run(unsigned short test, const unsigned short params[4],
                 unsigned long pollTicks, ChWatch watch, char *text) {
  unsigned short number;
  CmdAnswer a;
  text[0] = '\0';
  if (!chscreen_start(test, params, &number)) return -1;
  for (;;) {
    unsigned long start = HZ_200;
    while (HZ_200 - start < pollTicks) {
    }
    if (!jobStatusPatient(test, number, &a)) return -1;
    if (a.data[MDTEST_JOB_ANS_STATE] != MDTEST_JOB_RUNNING) break;
    if (watch && watch(&a)) chscreen_cancel();
  }
  unsigned short length = a.length;
  if (length > CH_RESULT_BYTES - 1) length = CH_RESULT_BYTES - 1;
  cmd_resultText(text, length);
  text[length] = '\0';
  return a.data[MDTEST_JOB_ANS_STATE];
}

const char *chscreen_value(const char *text, const char *key) {
  size_t n = strlen(key);
  for (const char *line = text; *line;) {
    if (strncmp(line, key, n) == 0 && line[n] == '=') return line + n + 1;
    const char *end = strchr(line, '\n');
    if (!end) break;
    line = end + 1;
  }
  return 0;
}

int chscreen_valueIs(const char *text, const char *key, const char *value) {
  const char *v = chscreen_value(text, key);
  size_t n = strlen(value);
  return v && strncmp(v, value, n) == 0 && (v[n] == '\n' || v[n] == '\0');
}

int chscreen_job(unsigned short test, const unsigned short params[4]) {
  static char text[CH_RESULT_BYTES];
  int state = chscreen_run(test, params, JOB_POLL_TICKS, 0, text);
  if (state < 0) return 0;
  printResult(text);
  if (state != MDTEST_JOB_DONE) {
    console_line(T_JOB_FAILED, test);
    console_line(T_JOB_FAILED_DETAIL, 0, (unsigned)state);
    return 0;
  }
  return 1;
}

ChVerdict chscreen_test(long mch) {
  console_page(T_CH_TITLE);
  bootSeen = 0;
  restarted = 0;
  wrongAnswers = 0;
  if (!chscreen_greet(mch)) return CH_NO_CHANNEL;

  CmdStats *s = cmd_stats();
  CmdStats before = *s;
  unsigned long rpBefore[6], rpAfter[6];
  int haveCounters = counters(rpBefore);
  unsigned long answeredFrom = s->answered;

  /* Back to back, no pause between commands. */
  unsigned long wrongBefore = wrongAnswers + s->unanswered;
  CmdAnswer a;
  for (unsigned long i = 0; i < HELLO_BURST; i++) {
    if (hello(mch, &a) && a.data[0] != MDTEST_PROTOCOL_VERSION) wrongAnswers++;
  }
  unsigned long errors = wrongAnswers + s->unanswered - wrongBefore;
  if (errors) {
    console_line(T_CH_ROW_ERRORS, T_CH_HELLOS, HELLO_BURST, errors);
  } else {
    console_line(T_CH_ROW, T_CH_HELLOS, HELLO_BURST, "PASS");
  }

  /* Every size, from none to the most a frame carries. */
  wrongBefore = wrongAnswers + s->unanswered;
  for (unsigned long i = 0; i < ECHO_ROUNDS; i++) echo(i, echoSize(i));
  errors = wrongAnswers + s->unanswered - wrongBefore;
  if (errors) {
    console_line(T_CH_ROW_ERRORS, T_CH_ECHOES, ECHO_ROUNDS, errors);
  } else {
    console_line(T_CH_ROW, T_CH_ECHOES, ECHO_ROUNDS, "PASS");
  }

  /* Commands while a job runs. */
  wrongBefore = wrongAnswers + s->unanswered;
  unsigned long during = 0;
  unsigned short number;
  const unsigned short params[4] = {JOB_SECONDS, 0, 0, 0};
  if (jobStart(MDTEST_TEST_DUMMY, params, &number)) {
    for (unsigned long i = 1;; i++) {
      echo(i, echoSize(i));
      during++;
      if (i % JOB_POLL_EVERY) continue;
      if (!jobStatus(&a) || a.data[1] != MDTEST_JOB_RUNNING) break;
    }
    if (a.data[1] != MDTEST_JOB_DONE) wrongAnswers++;
  } else {
    wrongAnswers++;
  }
  errors = wrongAnswers + s->unanswered - wrongBefore;
  if (errors) {
    console_line(T_CH_ROW_ERRORS, T_CH_DURING_JOB, during, errors);
  } else {
    console_line(T_CH_ROW, T_CH_DURING_JOB, during, "PASS");
  }

  /* The RP's view: it answered what the program saw answered (the first
   * counters command included, the second not), with no checksum error and
   * no overrun. */
  unsigned long seen = s->answered - answeredFrom + 1;
  int countersOk = 0;
  if (haveCounters && counters(rpAfter)) {
    unsigned long rpAnswered =
        rpAfter[MDTEST_COUNTERS_HANDLED] - rpBefore[MDTEST_COUNTERS_HANDLED];
    unsigned long checksum = rpAfter[MDTEST_COUNTERS_CHECKSUM] -
                             rpBefore[MDTEST_COUNTERS_CHECKSUM];
    unsigned long overruns = rpAfter[MDTEST_COUNTERS_OVERRUNS] -
                             rpBefore[MDTEST_COUNTERS_OVERRUNS];
    /* A command retried after an answer that came too late is answered
     * twice: the RP may count up to one more per retry. */
    unsigned long retried = s->retries - before.retries;
    countersOk = rpAnswered >= seen && rpAnswered <= seen + retried &&
                 checksum == 0 && overruns == 0;
    console_line(T_CH_ROW, T_CH_COUNTERS, rpAnswered,
                 countersOk ? "PASS" : "FAIL");
    if (!countersOk) {
      console_line(T_CH_COUNTERS_DETAIL, rpAnswered, seen);
      console_line(T_CH_COUNTERS_ERRORS, checksum, overruns);
    }
  } else {
    console_line(T_CH_ROW, T_CH_COUNTERS, 0UL, "FAIL");
  }

  unsigned long sent = s->sent - before.sent;
  unsigned long retries = s->retries - before.retries;
  unsigned long unanswered = s->unanswered - before.unanswered;
  console_blank();
  if (restarted) {
    console_line(T_CH_RESTARTED);
    console_line(T_CH_RESTARTED_DETAIL, restartFrom, restartTo);
    console_line(T_CH_LOCKED);
    return CH_UNRELIABLE;
  }
  if (unanswered || wrongAnswers) {
    console_line(T_CH_UNRELIABLE);
    console_line(T_CH_UNRELIABLE_DETAIL, unanswered, wrongAnswers);
    console_line(T_CH_LOCKED);
    return CH_UNRELIABLE;
  }
  if (retries) {
    console_line(T_CH_RETRIES);
    console_line(T_CH_RETRIES_DETAIL, retries, sent);
    console_line(T_CH_RETRIES_FIRST, s->firstRetried, s->firstRetriedWords);
    console_line(T_CH_RETRIES_ADVICE);
    return CH_RETRIES;
  }
  if (!countersOk) {
    /* Answered everything, but the RP counted otherwise: a frame it took
     * for two, or a checksum error the retries hid. */
    console_line(T_CH_UNRELIABLE);
    console_line(T_CH_LOCKED);
    return CH_UNRELIABLE;
  }
  console_line(T_CH_CLEAN);
  return CH_CLEAN;
}
