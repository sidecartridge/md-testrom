/*
 * The line test on the screen: runs every read test on both banks, names
 * what the errors point to, and sends the verdict to the bench.
 */
#ifndef LTSCREEN_H
#define LTSCREEN_H

#include "linetest.h"

typedef enum { LT_PASS, LT_FAIL, LT_STOPPED } LtVerdict;

/* Supervisor mode. */
LtVerdict ltscreen_run(const LtSizes *sizes, unsigned long estimateSeconds);

/* Seconds a pass takes on an 8 MHz 68000, roughly. */
unsigned long ltscreen_estimate(const LtSizes *sizes);

#endif
