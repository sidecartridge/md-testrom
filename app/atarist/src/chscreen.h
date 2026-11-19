/*
 * The command channel on the screen: select ROM3 and say hello, the
 * channel's own test (the second half of the gate), and a job run to its
 * result.
 */
#ifndef CHSCREEN_H
#define CHSCREEN_H

typedef enum {
  CH_CLEAN,      /* every command answered at once */
  CH_RETRIES,    /* answered, some after retries: a warning */
  CH_UNRELIABLE, /* unanswered, wrong answers or a restart: locked */
  CH_NO_CHANNEL  /* no select, no hello, or another protocol */
} ChVerdict;

/* Select ROM3 and say hello; from then on every line is mirrored to the
 * RP. 1 when commands work and both sides speak the
 * same protocol; says why not otherwise. */
int chscreen_open(long mch);

/* Open the channel, then test it. */
ChVerdict chscreen_test(long mch);

/* Run a test as a job and print its result lines; 1 when it ended done.
 * The channel is open. */
int chscreen_job(unsigned short test, const unsigned short params[4]);

#endif
