/*
 * From mismatches to a named fault. Plain C: the ST program and the host
 * tests compile it.
 *
 * The RP's pins GPIO 6 to 21 carry the cartridge port's D0 to D15 and, at
 * other moments of the cycle, its A0 to A15 (A0 is never driven: the 68000
 * has no A0 on the bus). So GPIO 6+n is both data line Dn and address line
 * An, and window index bit k is address line A(k+1); index bit 15 is the
 * bank, chosen by the !ROM4 and !ROM3 selects.
 */
#ifndef DIAGNOSE_H
#define DIAGNOSE_H

#include <stdint.h>

#include "mdtest.h"

#define DG_FIRST_ERRORS 6
/* The dead-pin check costs 15 pattern words an error: the first ones tell. */
#define DG_PIN_ANALYSE 2048U

typedef struct {
  uint32_t address; /* the ST's address */
  uint16_t expected;
  uint16_t read;
  uint8_t bytes; /* 1 or 2 */
} DgError;

/* Every read and every error of one bank. */
typedef struct {
  uint32_t reads;     /* words and bytes */
  uint32_t errors;
  uint32_t wordErrors;   /* only a word names the address it came from */
  uint32_t bitReads[16]; /* reads that carried data bit n (words: all) */
  uint32_t bitLow[16];   /* data bit n read 0, should be 1 */
  uint32_t bitHigh[16];  /* data bit n read 1, should be 0 */
  uint32_t dataErrors;   /* errors that are not another address's word */
  uint32_t substituted;  /* word errors: another address's word */
  uint32_t lineLow[16];  /* ... from an address with index bit k clear */
  uint32_t lineHigh[16]; /* ... from an address with index bit k set */
  uint32_t follows[16][16]; /* line k wrong, and equal to index bit c */
  uint32_t pinAnalysed;  /* word errors the pin check looked at */
  uint32_t pinMatch[16]; /* fits pin n dead: D(n) low, A(n) low */
  uint32_t pinStrict[16]; /* ... and A(n) mattered: no data fault fits */
  uint32_t openBus;      /* errors that read $FFFF or $0000 */
  uint32_t laneErrors[2]; /* byte errors: [0] even (D8-D15), [1] odd */
  uint32_t unstable;     /* the same address read twice, two values */
  int firstCount;
  DgError first[DG_FIRST_ERRORS];
} DgBank;

typedef struct {
  DgBank bank[2]; /* ROM4, ROM3 */
} DgStats;

/* What the errors point to, most specific first. The values are the verdict
 * frame's classes (common/mdtest.h). */
enum {
  DG_NONE = MDTEST_CLASS_NONE,
  DG_NO_ANSWER = MDTEST_CLASS_NO_ANSWER,       /* the bank reads open bus */
  DG_DATA_LOW = MDTEST_CLASS_DATA_LOW,         /* data line a always 0 */
  DG_DATA_HIGH = MDTEST_CLASS_DATA_HIGH,       /* data line a always 1 */
  DG_DATA_PAIR = MDTEST_CLASS_DATA_PAIR,       /* data lines a, b together */
  DG_ADDR_LOW = MDTEST_CLASS_ADDR_LOW,         /* A(a) always 0 */
  DG_ADDR_HIGH = MDTEST_CLASS_ADDR_HIGH,       /* A(a) always 1 */
  DG_ADDR_TIED = MDTEST_CLASS_ADDR_TIED,       /* A(a) follows A(b) */
  DG_BANK_SWAP = MDTEST_CLASS_BANK_SWAP,       /* the selects mixed up */
  DG_PIN_DEAD = MDTEST_CLASS_PIN_DEAD,         /* GPIO 6+a: D(a) and A(a) */
  DG_LANE = MDTEST_CLASS_LANE,                 /* a = 0 even, 1 odd bytes */
  DG_INTERMITTENT = MDTEST_CLASS_INTERMITTENT, /* errors come and go */
  DG_UNCLASSIFIED = MDTEST_CLASS_UNCLASSIFIED  /* the raw table is shown */
};

typedef struct {
  int kind;
  int a;
  int b;
  uint32_t errors;
  uint32_t reads;
} DgFinding;

void dg_reset(DgStats *s);

/* A word read at a window index (0 to $FFFF) that is not its pattern word. */
void dg_wordError(DgStats *s, uint16_t index, uint16_t expected, uint16_t read);

/* A byte read at a window byte offset (0 to $1FFFF) that is wrong. */
void dg_byteError(DgStats *s, uint32_t offset, uint8_t expected, uint8_t read);

/* The same address read twice, two different values. */
void dg_unstable(DgStats *s, uint16_t index);

/* Count reads: bank 0 or 1, words or bytes (lane 0 even, 1 odd). */
void dg_countWords(DgStats *s, int bank, uint32_t count);
void dg_countBytes(DgStats *s, int bank, int lane, uint32_t count);

/* What one bank's errors point to. */
DgFinding dg_classify(const DgBank *b);

#endif
