/**
 * File: channel.c
 * Author: Diego Parrilla Santamaría
 * Date: November 2026
 * Copyright: 2026 - GOODDATA LABS SL
 * Description: ROM3 as test data or as the command channel (channel.h).
 */

#include "channel.h"

#include <string.h>

#include "chandler.h"
#include "commemul.h"
#include "debug.h"
#include "jobs.h"
#include "reportsave.h"
#include "mdtest.h"
#include "pico/stdlib.h"
#include "tprotocol.h"

#if defined(EMUMD) && EMUMD
#include <stdio.h>
#include <stdlib.h>
#endif

// After an unselect is answered, the time the ST has to read the answer
// before the borrowed areas become test data again.
#define UNSELECT_GRACE_US 20000U
// How often the EmuMD bench looks for a request file.
#define REQUEST_FILE_POLL_US 200000U
#define BENCH_RESTART 255U

#define LOG_LINES 16U
#define LOG_CHARS 96U

static uint16_t *window;
static bool selected = false;
static uint32_t unselectAtUs = 0;
static bool unselectPending = false;
// An exit to Booster, answered: taken once the ST has had time to read it.
static bool exitPending = false;
static bool exitNow = false;
static uint32_t exitAtUs = 0;
static uint32_t bootId = 0;
static uint16_t answers = 0;
static uint32_t dataStateAnswers = 0;

// The last line-test verdict the ST sent, for the bench (swd.py reads it by
// symbol); the sequence number tells a new one from the last.
typedef struct {
  uint16_t words[MDTEST_VERDICT_WORDS];
  uint32_t count;
} LineVerdict;
volatile LineVerdict lineVerdict;
// Frames seen while ROM3 is data, and those with a bad checksum.
volatile uint32_t dataStateFrames;
volatile uint32_t dataStateChecksumErrors;
// Selects and unselects since the RP started.
volatile uint32_t channelSelects;
volatile uint32_t channelUnselects;

// The program's screen, mirrored line by line (MDTEST_CMD_LOG): the last
// LOG_LINES, channelLogCount the total. swd.py reads them by symbol.
char channelLog[LOG_LINES][LOG_CHARS];
volatile uint32_t channelLogCount;

// What the bench asks the program to do next (MDTEST_REQUEST_*, the test,
// four parameters): swd.py writes the words, the code last; answered once.
volatile uint16_t channelRequest[6];

// The sequence number served in every answer record (MDTEST_ANS_SEQ), the
// last one run, and the request it handed out: a LOG or REQUEST the ST
// resends after a lost answer changes nothing (common/mdtest.h).
static uint16_t servedSeq = 1;
static uint16_t lastSeq;
static uint16_t lastRequest[6];

// True when the command carrying seq is new: it runs, and the next number is
// served (never 0, the value nothing ran with).
static bool newSeq(uint16_t seq) {
  if (seq == lastSeq) return false;
  lastSeq = seq;
  if (++servedSeq == 0) servedSeq = 1;
  return true;
}

static const uint16_t selectMagic[MDTEST_SELECT_WORDS] = MDTEST_SELECT_MAGIC;

static uint16_t *answerRecord(void) { return &window[MDTEST_ANSWER_INDEX]; }

static void answer(uint16_t command, uint16_t status, uint16_t length,
                   const uint16_t *data, unsigned int count) {
  uint16_t *rec = answerRecord();
  if (bootId == 0) bootId = time_us_32() | 1U;
  rec[MDTEST_ANS_COMMAND] = command;
  rec[MDTEST_ANS_STATUS] = status;
  rec[MDTEST_ANS_BOOT] = (uint16_t)(bootId >> 16);
  rec[MDTEST_ANS_BOOT + 1] = (uint16_t)bootId;
  rec[MDTEST_ANS_COUNT] = ++answers;
  rec[MDTEST_ANS_LENGTH] = length;
  rec[MDTEST_ANS_SEQ] = servedSeq;
  for (unsigned int i = 0; i < MDTEST_ANS_DATA_WORDS; i++) {
    rec[MDTEST_ANS_DATA + i] = (i < count) ? data[i] : 0;
  }
}

// Areas the channel borrows: the template's block (token, seed, shared
// variables), the answer record, the result buffer.
static void clearBorrowed(void) {
  memset(&window[MDTEST_COMMAND_BLOCK_INDEX], 0,
         (MDTEST_ANSWER_INDEX + MDTEST_ANSWER_WORDS -
          MDTEST_COMMAND_BLOCK_INDEX) * 2U);
  memset(&window[MDTEST_RESULT_BUFFER_INDEX], 0,
         MDTEST_RESULT_BUFFER_WORDS * 2U);
}

static void restoreBorrowed(void) {
  mdtest_restoreWords(window, MDTEST_COMMAND_BLOCK_INDEX,
                      MDTEST_ANSWER_INDEX + MDTEST_ANSWER_WORDS -
                          MDTEST_COMMAND_BLOCK_INDEX);
  mdtest_restoreWords(window, MDTEST_RESULT_BUFFER_INDEX,
                      MDTEST_RESULT_BUFFER_WORDS);
}

// ---------------------------------------------------------------------------
// ROM3 as data: nothing is answered but the select frame.

static bool isSelectFrame(const TransmissionProtocol *frame) {
  if (frame->command_id != MDTEST_CMD_SELECT ||
      frame->payload_size != 4U + 2U * MDTEST_SELECT_WORDS) {
    return false;
  }
  const uint16_t *words = (const uint16_t *)frame->payload + 2;
  return memcmp(words, selectMagic, sizeof(selectMagic)) == 0;
}

static void selectChannel(uint32_t token) {
  clearBorrowed();
  answer(MDTEST_CMD_SELECT, MDTEST_STATUS_OK, 0, NULL, 0);
  mdtest_setState(window, MDTEST_STATE_COMMANDS);
  selected = true;
  channelSelects++;
  // The template's handshake, as chandler answers: the token, then a seed
  // that differs from it.
  dataStateAnswers++;
  TPROTO_SET_RANDOM_TOKEN64(
      (uintptr_t)window + CHANDLER_RANDOM_TOKEN_OFFSET,
      (((uint64_t)(bootId ^ dataStateAnswers)) << 32) | token);
  DPRINTF("ROM3 selected as the command channel\n");
}

static void onDataStateFrame(const TransmissionProtocol *frame) {
  dataStateFrames++;
  if (isSelectFrame(frame)) {
    selectChannel(TPROTO_GET_RANDOM_TOKEN(frame->payload));
    return;
  }
  if (frame->command_id != MDTEST_CMD_LINE_VERDICT ||
      frame->payload_size < 4U + 2U * MDTEST_VERDICT_WORDS) {
    return;
  }
  const uint16_t *words = (const uint16_t *)frame->payload + 2;  // the token
  for (unsigned int i = 0; i < MDTEST_VERDICT_WORDS; i++) {
    lineVerdict.words[i] = words[i];
  }
  lineVerdict.count++;
  DPRINTF("Line test verdict %u: class %u, first $%04X%04X, errors %u, run %u\n",
          words[0], words[1], words[2], words[3], words[4], words[5]);
}

static void onDataStateChecksumError(const TransmissionProtocol *frame) {
  (void)frame;
  dataStateChecksumErrors++;
}

static void consumeDataStateSample(uint16_t sample) {
  tprotocol_parse((uint16_t)(sample ^ CHANDLER_ADDRESS_HIGH_BIT),
                  onDataStateFrame, onDataStateChecksumError);
}

// ---------------------------------------------------------------------------
// ROM3 selected: the app's commands, answered by chandler.

static void putLog(const uint16_t *words, uint16_t length) {
  char *line = channelLog[channelLogCount % LOG_LINES];
  if (length > LOG_CHARS - 1U) length = LOG_CHARS - 1U;
  mdtest_getText(words, line, length);
  line[length] = '\0';
  channelLogCount++;
  DPRINTF("ST| %s\n", line);
}

// chandler.c's counters.
extern uint32_t chandlerHandled;
extern uint32_t chandlerDropped;
extern uint32_t chandlerRepeated;
extern uint32_t chandlerChecksumErrors;

static void counters(void) {
  uint32_t values[MDTEST_COUNTERS_WORDS / 2U] = {
      chandlerHandled,        chandlerDropped,
      chandlerRepeated,       chandlerChecksumErrors,
      commemul_getOverruns(), channelLogCount};
  uint16_t *out = &window[MDTEST_RESULT_BUFFER_INDEX];
  for (unsigned int i = 0; i < MDTEST_COUNTERS_WORDS / 2U; i++) {
    out[2 * i] = (uint16_t)(values[i] >> 16);
    out[2 * i + 1] = (uint16_t)values[i];
  }
  answer(MDTEST_CMD_COUNTERS, MDTEST_STATUS_OK, MDTEST_COUNTERS_WORDS, NULL,
         0);
}

static void jobStatus(void) {
  const JobStatus *job = jobs_status();
  uint16_t length = 0;
  if (job->state != MDTEST_JOB_RUNNING && job->resultLength) {
    length = job->resultLength;
    if (length > MDTEST_RESULT_BUFFER_WORDS * 2U) {
      length = MDTEST_RESULT_BUFFER_WORDS * 2U;
    }
    mdtest_putText(&window[MDTEST_RESULT_BUFFER_INDEX], jobs_result(),
                   length);
  }
  uint16_t data[MDTEST_JOB_ANS_LIVE + MDTEST_JOB_LIVE_WORDS];
  data[MDTEST_JOB_ANS_NUMBER] = job->number;
  data[MDTEST_JOB_ANS_STATE] = job->state;
  data[MDTEST_JOB_ANS_PROGRESS] = job->progress;
  data[MDTEST_JOB_ANS_TEST] = job->test;
  for (unsigned int i = 0; i < MDTEST_JOB_LIVE_WORDS; i++) {
    data[MDTEST_JOB_ANS_LIVE + i] = job->live[i];
  }
  answer(MDTEST_CMD_JOB_STATUS, MDTEST_STATUS_OK, length, data,
         MDTEST_JOB_ANS_LIVE + MDTEST_JOB_LIVE_WORDS);
}

static void onCommand(TransmissionProtocol *frame, uint16_t *words) {
  uint16_t command = frame->command_id;
  if ((command >> 8) != MDTEST_APP_ID) return;
  unsigned int count =
      frame->payload_size >= 4U ? (frame->payload_size - 4U) / 2U : 0U;

  switch (command) {
    case MDTEST_CMD_SELECT:
      // Selected already: say so again.
      answer(command, MDTEST_STATUS_OK, 0, NULL, 0);
      break;
    case MDTEST_CMD_UNSELECT:
      answer(command, MDTEST_STATUS_OK, 0, NULL, 0);
      unselectPending = true;
      unselectAtUs = time_us_32() + UNSELECT_GRACE_US;
      break;
    case MDTEST_CMD_HELLO: {
      const uint16_t data[3] = {MDTEST_PROTOCOL_VERSION,
                                (uint16_t)(MDTEST_CAPS >> 16),
                                (uint16_t)MDTEST_CAPS};
      answer(command, MDTEST_STATUS_OK, 0, data, 3);
      if (count >= 3U) {
        DPRINTF("Hello: program protocol %u, machine $%04X%04X\n", words[0],
                words[1], words[2]);
      }
      break;
    }
    case MDTEST_CMD_ECHO:
      if (count > MDTEST_ECHO_MAX_WORDS) {
        answer(command, MDTEST_STATUS_BAD, 0, NULL, 0);
        break;
      }
      memcpy(&window[MDTEST_RESULT_BUFFER_INDEX], words, count * 2U);
      answer(command, MDTEST_STATUS_OK, (uint16_t)count, NULL, 0);
      break;
    case MDTEST_CMD_JOB_START: {
      if (count < 6U) {
        answer(command, MDTEST_STATUS_BAD, 0, NULL, 0);
        break;
      }
      uint16_t number = 0;
      uint16_t status = jobs_start(words[0], words[1], &words[2],
                                   to_ms_since_boot(get_absolute_time()),
                                   &number);
      answer(command, status, 0, &number, 1);
      break;
    }
    case MDTEST_CMD_JOB_STATUS:
      jobStatus();
      break;
    case MDTEST_CMD_JOB_CANCEL:
      jobs_cancel();
      answer(command, MDTEST_STATUS_OK, 0, NULL, 0);
      break;
    case MDTEST_CMD_LOG:
      if (count >= 2U && words[1] <= (count - 2U) * 2U) {
        if (newSeq(words[0])) putLog(&words[2], words[1]);
        answer(command, MDTEST_STATUS_OK, 0, NULL, 0);
      } else {
        answer(command, MDTEST_STATUS_BAD, 0, NULL, 0);
      }
      break;
    case MDTEST_CMD_REQUEST:
      if (count < 1U) {
        answer(command, MDTEST_STATUS_BAD, 0, NULL, 0);
        break;
      }
      if (newSeq(words[0])) {
        for (unsigned int i = 0; i < 6U; i++) lastRequest[i] = channelRequest[i];
        channelRequest[0] = MDTEST_REQUEST_NONE;
      }
      answer(command, MDTEST_STATUS_OK, 0, lastRequest, 6);
      break;
    case MDTEST_CMD_COUNTERS:
      counters();
      break;
    case MDTEST_CMD_REPORT_DATA: {
      const JobStatus *job = jobs_status();
      if (count < 2U || words[1] > (count - 2U) * 2U ||
          job->test != MDTEST_TEST_REPORT || job->state != MDTEST_JOB_RUNNING) {
        answer(command, MDTEST_STATUS_BAD, 0, NULL, 0);
        break;
      }
      // A resend of a chunk already taken is answered again and changes
      // nothing; a new one waits until the last is written.
      if (words[0] != lastSeq && !reportsave_ready(words[1])) {
        answer(command, MDTEST_STATUS_BUSY, 0, NULL, 0);
        break;
      }
      if (newSeq(words[0])) reportsave_take(&words[2], words[1]);
      answer(command, MDTEST_STATUS_OK, 0, NULL, 0);
      break;
    }
    case MDTEST_CMD_EXIT:
      answer(command, MDTEST_STATUS_OK, 0, NULL, 0);
      jobs_cancel();
      exitPending = true;
      exitAtUs = time_us_32() + UNSELECT_GRACE_US;
      break;
    default:
      answer(command, MDTEST_STATUS_BAD, 0, NULL, 0);
      break;
  }
}

static void unselect(void) {
  unselectPending = false;
  jobs_cancel();
  restoreBorrowed();
  mdtest_setState(window, MDTEST_STATE_DATA);
  selected = false;
  channelUnselects++;
  DPRINTF("ROM3 is test data again\n");
}

#if defined(EMUMD) && EMUMD
// EmuMD's bench: the file MDTEST_REQUEST_FILE names, when it appears, holds
// a request ("CODE TEST P0 P1 P2 P3"); it is read and deleted. Code
// BENCH_RESTART is the firmware's own: channel_benchRestart().
static void pollRequestFile(void) {
  static uint32_t lastUs = 0;
  static const char *path = NULL;
  static bool looked = false;
  if (!looked) {
    path = getenv("MDTEST_REQUEST_FILE");
    looked = true;
  }
  if (!path || time_us_32() - lastUs < REQUEST_FILE_POLL_US) return;
  lastUs = time_us_32();
  FILE *f = fopen(path, "r");
  if (!f) return;
  unsigned int v[6] = {0, 0, 0, 0, 0, 0};
  int n = fscanf(f, "%u %u %u %u %u %u", &v[0], &v[1], &v[2], &v[3], &v[4],
                 &v[5]);
  fclose(f);
  remove(path);
  if (n < 1) return;
  if (v[0] == BENCH_RESTART) {
    channel_benchRestart();
    return;
  }
  for (unsigned int i = 1; i < 6U; i++) channelRequest[i] = (uint16_t)v[i];
  channelRequest[0] = (uint16_t)v[0];
  DPRINTF("Bench request %u (test %u)\n", v[0], v[1]);
}
#endif

#if (defined(_DEBUG) && (_DEBUG != 0)) || (defined(EMUMD) && EMUMD)
void channel_benchRestart(void) {
  if (selected) unselect();
  unselectPending = false;
  exitPending = false;
  jobs_reset();
  bootId = 0;
  answers = 0;
  servedSeq = 1;
  lastSeq = 0;
  channelRequest[0] = MDTEST_REQUEST_NONE;
  DPRINTF("Restarted, as the ST sees it\n");
}
#endif

#if defined(_DEBUG) && (_DEBUG != 0)
void channel_benchSelect(bool on) {
  if (on && !selected) {
    clearBorrowed();
    answer(MDTEST_CMD_SELECT, MDTEST_STATUS_OK, 0, NULL, 0);
    mdtest_setState(window, MDTEST_STATE_COMMANDS);
    selected = true;
    channelSelects++;
    DPRINTF("ROM3 selected by the probe\n");
  } else if (!on && selected) {
    unselect();
  }
}
#endif

void channel_init(uint16_t *servedWindow) {
  window = servedWindow;
  jobs_reset();
  chandler_addCB(onCommand);
}

void channel_poll(void) {
  if (selected) {
    chandler_loop();
    jobs_poll(to_ms_since_boot(get_absolute_time()));
    if (unselectPending && (int32_t)(time_us_32() - unselectAtUs) >= 0) {
      unselect();
    }
    if (exitPending && (int32_t)(time_us_32() - exitAtUs) >= 0) {
      exitPending = false;
      exitNow = true;
    }
  } else {
    commemul_poll(consumeDataStateSample);
  }
#if defined(EMUMD) && EMUMD
  pollRequestFile();
#endif
}

bool channel_selected(void) { return selected; }

bool channel_exitRequested(void) { return exitNow; }
