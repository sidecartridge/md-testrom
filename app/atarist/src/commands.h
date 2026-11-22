/*
 * The command channel, from the program's side (common/mdtest.h): ROM3
 * selected, each command a frame of ROM3 reads in the template's protocol,
 * answered when the firmware writes the frame's token at $FA2004, after its
 * answer record at $FA2100 and the result buffer at $FA4000.
 *
 * Supervisor mode, interrupts on: the timeout is counted on the 200 Hz
 * timer, so it lasts as long on every CPU.
 */
#ifndef COMMANDS_H
#define COMMANDS_H

#include "mdtest.h"

typedef struct {
  unsigned short command;
  unsigned short status; /* MDTEST_STATUS_* */
  unsigned long boot;    /* the RP's boot ID */
  unsigned short count;  /* answers since the RP started */
  unsigned short length; /* what the result buffer holds */
  unsigned short data[MDTEST_ANS_DATA_WORDS];
} CmdAnswer;

typedef struct {
  unsigned long sent;       /* commands */
  unsigned long answered;   /* commands answered, retried or not */
  unsigned long retries;    /* attempts after a command's first */
  unsigned long unanswered; /* commands with no answer after every retry */
  unsigned long logsDropped;
  unsigned short firstRetried; /* the first command retried, its words */
  unsigned short firstRetriedWords;
} CmdStats;

#define CMD_OK 0
#define CMD_NO_ANSWER (-1)

/* The machine, for the Mega STE's cache and the hello. */
void cmd_init(long mch, int megaSte);

/* Send a command with its words (after the token), retrying; fills answer
 * when it is not NULL. CMD_OK or CMD_NO_ANSWER. */
int cmd_send(unsigned short command, const unsigned short *words, int count,
             CmdAnswer *answer);

/* A frame nothing answers, read while ROM3 is data (the line test's
 * verdict): no token, no retry, the Mega STE's cache off for it. */
void cmd_sendOneWay(unsigned short command, const unsigned short *words,
                    int count);

/* The header's state word: ROM3 selected or data. */
int cmd_selected(void);

/* Select ROM3 (CMD_OK once the header says so), or give it back (CMD_OK once
 * the whole window is the pattern again). */
int cmd_select(void);
int cmd_unselect(void);

/* The result buffer: words, or text (no terminator added past length). */
const volatile unsigned short *cmd_result(void);
void cmd_resultText(char *text, unsigned short length);

/* A line of the screen to the RP; dropped (and counted) unanswered. */
void cmd_log(const char *line);

/* A chunk of the report for the card (MDTEST_CMD_REPORT_DATA; length 0
 * ends it): CMD_OK and the answer, whose status may be BUSY (send again). */
int cmd_reportData(const char *bytes, unsigned short length, CmdAnswer *a);

/* The bench's next request (MDTEST_CMD_REQUEST): CMD_OK and the answer. */
int cmd_request(CmdAnswer *a);

CmdStats *cmd_stats(void);

#endif
