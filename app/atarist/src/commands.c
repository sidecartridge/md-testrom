#include "commands.h"

#include <string.h>

#include "hw.h"

#define WINDOW ((const volatile unsigned short *)MDTEST_ROM4_ADDRESS)
#define HZ_200 hw_ticks
#define MSTE_SPEED_CACHE (*(volatile unsigned char *)0xFF8E21)

/* The template's block: the token the firmware echoes, then its seed. */
#define TOKEN (*(const volatile unsigned long *)(MDTEST_ROM4_ADDRESS + 0x2004UL))
#define SEED (*(const volatile unsigned long *)(MDTEST_ROM4_ADDRESS + 0x2008UL))

/* Ticks of 5 ms an attempt waits for its answer: the template's spin count
 * lasts about as long on an 8 MHz ST. Then the attempts after the first (the
 * template's CMD_RETRIES_COUNT). */
#define ANSWER_TICKS 50UL
#define RETRIES 3
/* How long an unselect may take to give the window back. */
#define UNSELECT_TICKS 200UL

static int megaSteMachine;
static CmdStats stats;
static int logging;

void cmd_init(long mch, int megaSte) {
  (void)mch;
  megaSteMachine = megaSte;
}

CmdStats *cmd_stats(void) { return &stats; }

/* One frame: each word the low half of an address read in ROM3, offset from
 * its middle. */
static int attempt(unsigned short command, const unsigned short *words,
                   int count) {
  const volatile unsigned char *rom3 =
      (const volatile unsigned char *)(MDTEST_ROM3_ADDRESS + 0x8000UL);
  unsigned long token = SEED;
  unsigned short size = (unsigned short)(4 + 2 * count);
  unsigned short sum = (unsigned short)(command + size);
  (void)rom3[(short)0xABCD];
  (void)rom3[(short)command];
  (void)rom3[(short)size];
  sum = (unsigned short)(sum + (unsigned short)token);
  (void)rom3[(short)(unsigned short)token];
  sum = (unsigned short)(sum + (unsigned short)(token >> 16));
  (void)rom3[(short)(unsigned short)(token >> 16)];
  for (int i = 0; i < count; i++) {
    sum = (unsigned short)(sum + words[i]);
    (void)rom3[(short)words[i]];
  }
  (void)rom3[(short)sum];

  unsigned long start = HZ_200;
  do {
    /* A token match alone is not an answer: the seed must have moved too
     * (the template's two-phase commit). */
    if (TOKEN == token && SEED != token) return CMD_OK;
  } while (HZ_200 - start < ANSWER_TICKS);
  return CMD_NO_ANSWER;
}

static void readAnswer(CmdAnswer *a) {
  const volatile unsigned short *rec = &WINDOW[MDTEST_ANSWER_INDEX];
  a->command = rec[MDTEST_ANS_COMMAND];
  a->status = rec[MDTEST_ANS_STATUS];
  a->boot = ((unsigned long)rec[MDTEST_ANS_BOOT] << 16) |
            rec[MDTEST_ANS_BOOT + 1];
  a->count = rec[MDTEST_ANS_COUNT];
  a->length = rec[MDTEST_ANS_LENGTH];
  for (int i = 0; i < (int)MDTEST_ANS_DATA_WORDS; i++) {
    a->data[i] = rec[MDTEST_ANS_DATA + i];
  }
}

/* The sequence number LOG and REQUEST carry (common/mdtest.h), read once
 * before the command so its retries carry the same; the Mega STE's cache off,
 * as for the answer itself: a stale copy would repeat the last number. */
static unsigned short servedSeq(void) {
  unsigned char cache = 0;
  if (megaSteMachine) {
    cache = MSTE_SPEED_CACHE;
    MSTE_SPEED_CACHE = (unsigned char)(cache & ~1);
  }
  unsigned short seq = WINDOW[MDTEST_ANSWER_INDEX + MDTEST_ANS_SEQ];
  if (megaSteMachine) MSTE_SPEED_CACHE = cache;
  return seq;
}

void cmd_sendOneWay(unsigned short command, const unsigned short *words,
                    int count) {
  const volatile unsigned char *rom3 =
      (const volatile unsigned char *)(MDTEST_ROM3_ADDRESS + 0x8000UL);
  /* Through a Mega STE's cache a byte read fetches its whole word, which
   * asserts !UDS, the RP's bit 0: every odd word of the frame would arrive
   * even, and the header would not match. */
  unsigned char cache = 0;
  if (megaSteMachine) {
    cache = MSTE_SPEED_CACHE;
    MSTE_SPEED_CACHE = (unsigned char)(cache & ~1);
  }
  unsigned short size = (unsigned short)(4 + 2 * count);
  unsigned short sum = (unsigned short)(command + size);
  (void)rom3[(short)0xABCD];
  (void)rom3[(short)command];
  (void)rom3[(short)size];
  (void)rom3[0];  /* the token: none, nothing answers */
  (void)rom3[0];
  for (int i = 0; i < count; i++) {
    sum = (unsigned short)(sum + words[i]);
    (void)rom3[(short)words[i]];
  }
  (void)rom3[(short)sum];
  if (megaSteMachine) MSTE_SPEED_CACHE = cache;
}

int cmd_send(unsigned short command, const unsigned short *words, int count,
             CmdAnswer *answer) {
  /* A Mega STE's cache answers cartridge reads from old copies: off for the
   * command, the user's setting back after it. */
  unsigned char cache = 0;
  if (megaSteMachine) {
    cache = MSTE_SPEED_CACHE;
    MSTE_SPEED_CACHE = (unsigned char)(cache & ~1);
  }
  int result = CMD_NO_ANSWER;
  stats.sent++;
  for (int i = 0; i <= RETRIES; i++) {
    if (i) {
      if (!stats.retries) {
        stats.firstRetried = command;
        stats.firstRetriedWords = (unsigned short)count;
      }
      stats.retries++;
    }
    if (attempt(command, words, count) == CMD_OK) {
      result = CMD_OK;
      break;
    }
  }
  if (result == CMD_OK) {
    stats.answered++;
    if (answer) readAnswer(answer);
  } else {
    stats.unanswered++;
  }
  if (megaSteMachine) MSTE_SPEED_CACHE = cache;
  return result;
}

int cmd_selected(void) {
  return WINDOW[MDTEST_HEADER_INDEX + MDTEST_HDR_STATE] ==
         MDTEST_STATE_COMMANDS;
}

int cmd_select(void) {
  static const unsigned short magic[MDTEST_SELECT_WORDS] = MDTEST_SELECT_MAGIC;
  if (cmd_send(MDTEST_CMD_SELECT, magic, MDTEST_SELECT_WORDS, NULL) != CMD_OK) {
    return CMD_NO_ANSWER;
  }
  return cmd_selected() ? CMD_OK : CMD_NO_ANSWER;
}

int cmd_unselect(void) {
  if (cmd_send(MDTEST_CMD_UNSELECT, NULL, 0, NULL) != CMD_OK) {
    return CMD_NO_ANSWER;
  }
  unsigned long start = HZ_200;
  while (cmd_selected()) {
    if (HZ_200 - start > UNSELECT_TICKS) return CMD_NO_ANSWER;
  }
  return CMD_OK;
}

const volatile unsigned short *cmd_result(void) {
  return &WINDOW[MDTEST_RESULT_BUFFER_INDEX];
}

void cmd_resultText(char *text, unsigned short length) {
  const volatile unsigned short *words = cmd_result();
  for (unsigned short i = 0; i < length; i++) {
    unsigned short w = words[i / 2];
    text[i] = (char)((i & 1) ? (w & 0xFF) : (w >> 8));
  }
}

void cmd_log(const char *line) {
  /* A line the sending itself would print never loops back here. */
  if (logging) return;
  logging = 1;
  unsigned short words[2 + 48];
  unsigned short length = (unsigned short)strlen(line);
  if (length > 94) length = 94;
  words[0] = servedSeq();
  words[1] = length;
  for (unsigned short i = 0; i < length; i += 2) {
    unsigned short high = (unsigned char)line[i];
    unsigned short low = (i + 1 < length) ? (unsigned char)line[i + 1] : 0;
    words[2 + i / 2] = (unsigned short)((high << 8) | low);
  }
  CmdAnswer a;
  if (cmd_send(MDTEST_CMD_LOG, words, 2 + (length + 1) / 2, &a) != CMD_OK ||
      a.status != MDTEST_STATUS_OK) {
    stats.logsDropped++;
  }
  logging = 0;
}

int cmd_reportData(const char *bytes, unsigned short length, CmdAnswer *a) {
  static unsigned short words[2 + MDTEST_REPORT_CHUNK_BYTES / 2];
  if (length > MDTEST_REPORT_CHUNK_BYTES) length = MDTEST_REPORT_CHUNK_BYTES;
  words[0] = servedSeq();
  words[1] = length;
  for (unsigned short i = 0; i < length; i += 2) {
    unsigned short high = (unsigned char)bytes[i];
    unsigned short low = (i + 1 < length) ? (unsigned char)bytes[i + 1] : 0;
    words[2 + i / 2] = (unsigned short)((high << 8) | low);
  }
  return cmd_send(MDTEST_CMD_REPORT_DATA, words, 2 + (length + 1) / 2, a);
}

int cmd_request(CmdAnswer *a) {
  unsigned short words[1];
  words[0] = servedSeq();
  return cmd_send(MDTEST_CMD_REQUEST, words, 1, a);
}
