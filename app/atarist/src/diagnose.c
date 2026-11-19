#include "diagnose.h"

#include <string.h>

#include "mdtest.h"

/* A class explains the errors when it accounts for this share of them. */
#define EXPLAINS(part, whole) ((part) * 10U >= (whole) * 9U)
/* A line is stuck, not flickering, when it is wrong on this share of the
 * reads that exercise it. */
#define STUCK(part, whole) ((part) * 10U >= (whole) * 3U)

static int bitCount(uint32_t v) {
  int n = 0;
  for (; v; v &= v - 1) n++;
  return n;
}

static int inHeader(uint32_t index) {
  return index >= MDTEST_HEADER_INDEX &&
         index < MDTEST_HEADER_INDEX + MDTEST_HEADER_WORDS;
}

void dg_reset(DgStats *s) { memset(s, 0, sizeof(*s)); }

void dg_countWords(DgStats *s, int bank, uint32_t count) {
  DgBank *b = &s->bank[bank];
  b->reads += count;
  for (int n = 0; n < 16; n++) b->bitReads[n] += count;
}

void dg_countBytes(DgStats *s, int bank, int lane, uint32_t count) {
  DgBank *b = &s->bank[bank];
  b->reads += count;
  for (int n = 0; n < 8; n++) b->bitReads[lane ? n : n + 8] += count;
}

static void keep(DgBank *b, uint32_t address, uint16_t expected, uint16_t read,
                 int bytes) {
  if (b->firstCount < DG_FIRST_ERRORS) {
    DgError *e = &b->first[b->firstCount++];
    e->address = address;
    e->expected = expected;
    e->read = read;
    e->bytes = (uint8_t)bytes;
  }
}

static void dataBits(DgBank *b, uint16_t expected, uint16_t read,
                     uint16_t mask) {
  uint16_t diff = (uint16_t)((expected ^ read) & mask);
  for (int n = 0; n < 16; n++) {
    if (!(diff & (1U << n))) continue;
    if (expected & (1U << n)) {
      b->bitLow[n]++;
    } else {
      b->bitHigh[n]++;
    }
  }
}

void dg_wordError(DgStats *s, uint16_t index, uint16_t expected,
                  uint16_t read) {
  DgBank *b = &s->bank[index >> 15];
  b->errors++;
  b->wordErrors++;
  keep(b, MDTEST_ROM4_ADDRESS + 2UL * index, expected, read, 2);
  if (read == 0xFFFF || read == 0x0000) {
    /* Open bus: no address's word, no pin's. */
    b->openBus++;
    b->dataErrors++;
    dataBits(b, expected, read, 0xFFFF);
    return;
  }

  /* One pin dead: its address line and its data line both read low. Where
   * that address line was low anyway it looks like a data fault alone. */
  int strict = 0;
  int analyse = b->pinAnalysed < DG_PIN_ANALYSE;
  if (analyse) b->pinAnalysed++;
  for (int n = 1; n < 16 && analyse; n++) {
    uint16_t from = (uint16_t)(index & ~(1U << (n - 1)));
    if (read == (uint16_t)(mdtest_patternWord(from) & ~(1U << n))) {
      b->pinMatch[n]++;
      if (from != index) {
        b->pinStrict[n]++;
        strict = 1;
      }
    }
  }
  /* Another address's word: the pattern names it. */
  uint16_t source = mdtest_patternIndex(read);
  uint16_t diff = (uint16_t)(source ^ index);
  int lines = bitCount(diff);
  if (lines >= 1 && lines <= 2 && !inHeader(source)) {
    b->substituted++;
    for (int k = 0; k < 16; k++) {
      if (!(diff & (1U << k))) continue;
      if (source & (1U << k)) {
        b->lineHigh[k]++;
      } else {
        b->lineLow[k]++;
      }
      for (int c = 0; c < 16; c++) {
        if (c != k && !!(source & (1U << k)) == !!(index & (1U << c))) {
          b->follows[k][c]++;
        }
      }
    }
    return;
  }
  if (strict) return;
  b->dataErrors++;
  dataBits(b, expected, read, 0xFFFF);
}

void dg_byteError(DgStats *s, uint32_t offset, uint8_t expected, uint8_t read) {
  DgBank *b = &s->bank[(offset >> 16) & 1];
  int lane = (int)(offset & 1);
  int shift = lane ? 0 : 8;
  b->errors++;
  b->laneErrors[lane]++;
  keep(b, MDTEST_ROM4_ADDRESS + offset, expected, read, 1);
  if (read == 0xFF || read == 0x00) b->openBus++;
  b->dataErrors++;
  dataBits(b, (uint16_t)(expected << shift), (uint16_t)(read << shift),
           (uint16_t)(0xFF << shift));
}

void dg_unstable(DgStats *s, uint16_t index) {
  DgBank *b = &s->bank[index >> 15];
  b->unstable++;
}

static DgFinding finding(int kind, int a, int b, const DgBank *bank) {
  DgFinding f = {kind, a, b, bank->errors, bank->reads};
  return f;
}

/* The data bit wrong in the most errors, and how wrong. */
static int worstBit(const DgBank *b, uint32_t *count) {
  int best = -1;
  *count = 0;
  for (int n = 0; n < 16; n++) {
    uint32_t c = b->bitLow[n] + b->bitHigh[n];
    if (c > *count) {
      *count = c;
      best = n;
    }
  }
  return best;
}

DgFinding dg_classify(const DgBank *b) {
  uint32_t e = b->errors;
  if (e == 0) return finding(DG_NONE, 0, 0, b);

  /* Nothing answers: open bus on almost every read. */
  if (EXPLAINS(b->openBus, e) && STUCK(e, b->reads)) {
    return finding(DG_NO_ANSWER, 0, 0, b);
  }

  /* Other addresses' words: an address line or the bank. Judged on word
   * errors: a byte cannot name the address it came from. */
  uint32_t w = b->wordErrors;
  if (w && EXPLAINS(b->substituted, w)) {
    int line = -1;
    uint32_t most = 0;
    for (int k = 0; k < 16; k++) {
      uint32_t c = b->lineLow[k] + b->lineHigh[k];
      if (c > most) {
        most = c;
        line = k;
      }
    }
    if (line >= 0 && EXPLAINS(most, b->substituted)) {
      if (line == 15) return finding(DG_BANK_SWAP, 0, 0, b);
      /* Index bit k is A(k+1). */
      if (b->lineHigh[line] == 0) return finding(DG_ADDR_LOW, line + 1, 0, b);
      if (b->lineLow[line] == 0) return finding(DG_ADDR_HIGH, line + 1, 0, b);
      for (int c = 0; c < 15; c++) {
        if (c != line && EXPLAINS(b->follows[line][c], most)) {
          return finding(DG_ADDR_TIED, line + 1, c + 1, b);
        }
      }
    }
    return finding(DG_UNCLASSIFIED, 0, 0, b);
  }

  /* A dead RP pin: its data line and its address line together. */
  for (int n = 1; n < 16; n++) {
    if (b->pinStrict[n] && EXPLAINS(b->pinMatch[n], b->pinAnalysed)) {
      return finding(DG_PIN_DEAD, n, 0, b);
    }
  }

  /* Data lines. */
  if (EXPLAINS(b->dataErrors, e)) {
    uint32_t worst;
    int n = worstBit(b, &worst);
    int others = 0;
    int second = -1;
    for (int m = 0; m < 16; m++) {
      if (m != n && b->bitLow[m] + b->bitHigh[m]) {
        others++;
        second = m;
      }
    }
    /* Byte reads on one lane only. */
    if (b->laneErrors[0] + b->laneErrors[1] == e && others > 1 &&
        (b->laneErrors[0] == 0 || b->laneErrors[1] == 0)) {
      return finding(DG_LANE, b->laneErrors[1] ? 1 : 0, 0, b);
    }
    if (n >= 0 && others == 0 && STUCK(worst, b->bitReads[n] / 2)) {
      if (b->bitHigh[n] == 0) return finding(DG_DATA_LOW, n, 0, b);
      if (b->bitLow[n] == 0) return finding(DG_DATA_HIGH, n, 0, b);
    }
    if (n >= 0 && others == 1 && STUCK(worst, b->bitReads[n] / 2)) {
      return finding(DG_DATA_PAIR, n < second ? n : second,
                     n < second ? second : n, b);
    }
  }

  /* Few errors with no pattern: they come and go. */
  if (!STUCK(e, b->reads)) return finding(DG_INTERMITTENT, 0, 0, b);
  return finding(DG_UNCLASSIFIED, 0, 0, b);
}
