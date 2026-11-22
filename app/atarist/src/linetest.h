/*
 * The line test's reads: v0.0.3's read tests and the signals' own checks,
 * on the computed window. Plain C over a window pointer, so the host tests
 * run the same code against a window with a fault injected.
 */
#ifndef LINETEST_H
#define LINETEST_H

#include <stdint.h>

#include "diagnose.h"

typedef enum {
  LT_SEQ_WORDS,
  LT_SEQ_BYTES,
  LT_RANDOM_WORDS,
  LT_RANDOM_BYTES,
  LT_ADDRESS_WALK,
  LT_SAME_ADDRESS,   /* back to back, the same address */
  LT_FAR_ADDRESSES,  /* alternating far-apart addresses */
  LT_AFTER_RAM,      /* each read right after a write to RAM */
  LT_IRQ_OFF,        /* sequential words with interrupts off */
  LT_TESTS
} LtTest;

typedef struct {
  uint32_t randomReads; /* per bank */
  uint32_t walkRounds;  /* per line */
  uint32_t paceReads;   /* per bank, each pace test */
} LtSizes;

extern const LtSizes lt_quick;
extern const LtSizes lt_long;

/* Polled every few thousand reads; true stops the test. */
typedef int (*LtStop)(void);

/*
 * Run one test on one bank of the window (the ST's $FA0000). Errors go into
 * stats; returns 0, or 1 when stop() asked to stop.
 */
int lt_run(LtTest test, int bank, const volatile uint16_t *window,
           const LtSizes *sizes, DgStats *stats, LtStop stop);

/* Reads the test makes on one bank (for the screen's estimate). */
uint32_t lt_reads(LtTest test, const LtSizes *sizes);

/* The random tests' seed (0: the default). */
void lt_seed(uint16_t value);

#ifndef __m68k__
/* Host tests: called with the window byte offset of every read. */
extern void (*lt_hostRead)(uint32_t byteOffset);
#endif

#endif
