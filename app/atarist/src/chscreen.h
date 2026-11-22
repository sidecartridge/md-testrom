/*
 * The command channel on the screen: select ROM3 and say hello, the
 * channel's own test (the second half of the gate), and a job run to its
 * result.
 */
#ifndef CHSCREEN_H
#define CHSCREEN_H

#include "commands.h"
#include "mdtest.h"

/* A job's result text: the result buffer, and its terminator. */
#define CH_RESULT_BYTES (MDTEST_RESULT_BUFFER_WORDS * 2 + 1)

/* Called at each poll of a running job with its status; nonzero cancels
 * it. */
typedef int (*ChWatch)(const CmdAnswer *a);

typedef enum {
  CH_CLEAN,      /* every command answered at once */
  CH_RETRIES,    /* answered, some after retries: a warning */
  CH_UNRELIABLE, /* unanswered, wrong answers or a restart: locked */
  CH_NO_CHANNEL  /* no select, no hello, or another protocol */
} ChVerdict;

/* Select ROM3 and say hello; from then on every line is mirrored to the
 * RP. 1 when commands work and both sides speak the
 * same protocol; says why not otherwise. The hello's lines are shown only
 * when this selects ROM3 (the channel's test always shows them). */
int chscreen_open(long mch);

/* The same, the hello's lines shown even if ROM3 was already selected (the
 * channel's test, and a channel selected again after the RP restarted). */
int chscreen_greet(long mch);

/* Open the channel, then test it. */
ChVerdict chscreen_test(long mch);

/* Run a test as a job and print its result lines; 1 when it ended done.
 * The channel is open. */
int chscreen_job(unsigned short test, const unsigned short params[4]);

/* Run a test as a job, polled every pollTicks (5 ms), to its end: its state
 * (MDTEST_JOB_*) and its result lines in text (CH_RESULT_BYTES), or -1 when
 * the device stopped answering or restarted, said on the screen. */
int chscreen_run(unsigned short test, const unsigned short params[4],
                 unsigned long pollTicks, ChWatch watch, char *text);

/* The steps of a job run by hand: start (1 when started), status (1 when
 * answered; says why not), cancel. */
int chscreen_start(unsigned short test, const unsigned short params[4],
                   unsigned short *number);
int chscreen_status(unsigned short test, unsigned short number, CmdAnswer *a);
void chscreen_cancel(void);

/* A result line's value ("key=value"), up to its newline; 0 if absent. */
const char *chscreen_value(const char *text, const char *key);
/* 1 when the key's value is exactly this. */
int chscreen_valueIs(const char *text, const char *key, const char *value);

#endif
