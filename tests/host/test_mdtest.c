// units: common/mdtest.c
/* The window's contents (common/mdtest.h): the pattern's properties over all
 * 65,536 words, the header, and the state word. Writes the filled window to
 * build/mdtest_window.bin, which test_mdtest.py checks against an independent
 * Python implementation. */
#include "mdtest.h"

#include <string.h>

#include "test.h"

static uint16_t window[MDTEST_WINDOW_WORDS];
static unsigned char seen[MDTEST_WINDOW_WORDS];

static int bits(unsigned int value) { return __builtin_popcount(value); }

static int inHeader(unsigned int index) {
  return index >= MDTEST_HEADER_INDEX &&
         index < MDTEST_HEADER_INDEX + MDTEST_HEADER_WORDS;
}

int main(void) {
  // A bijection: every value once, so no two addresses hold the same word
  // (one or two address bits apart, or any other distance, either bank).
  int duplicates = 0;
  for (unsigned int i = 0; i < MDTEST_WINDOW_WORDS; i++) {
    uint16_t word = mdtest_patternWord((uint16_t)i);
    if (seen[word]++) duplicates++;
  }
  CHECK_EQ(duplicates, 0);

  // The inverse names every word's index.
  int inverseErrors = 0;
  for (unsigned int i = 0; i < MDTEST_WINDOW_WORDS; i++) {
    if (mdtest_patternIndex(mdtest_patternWord((uint16_t)i)) != i) inverseErrors++;
  }
  CHECK_EQ(inverseErrors, 0);

  // The first word is zero: never a cartridge magic.
  CHECK_EQ(mdtest_patternWord(0), 0);

  // Every data bit takes both values in each bank, on each byte lane.
  for (unsigned int bank = 0; bank < 2; bank++) {
    uint16_t ones = 0;
    uint16_t zeros = 0;
    for (unsigned int i = 0; i < MDTEST_BANK_WORDS; i++) {
      uint16_t word = mdtest_patternWord((uint16_t)(bank * MDTEST_BANK_WORDS + i));
      ones |= word;
      zeros |= (uint16_t)~word;
    }
    CHECK_EQ(ones, 0xFFFF);
    CHECK_EQ(zeros, 0xFFFF);
  }

  // Each address bit flips about half the data bits on average (the rest
  // is pseudo-random): between 6 and 10 of 16.
  for (unsigned int line = 0; line < 16; line++) {
    unsigned long flipped = 0;
    for (unsigned int i = 0; i < MDTEST_WINDOW_WORDS; i++) {
      flipped += (unsigned long)bits(mdtest_patternWord((uint16_t)i) ^
                                     mdtest_patternWord((uint16_t)(i ^ (1U << line))));
    }
    CHECK(flipped >= 6UL * MDTEST_WINDOW_WORDS && flipped <= 10UL * MDTEST_WINDOW_WORDS);
  }

  // The header sits where no address-line test reads: none of its indexes
  // has 0, 1, 2, 14, 15 or 16 bits set.
  for (unsigned int i = 0; i < MDTEST_HEADER_WORDS; i++) {
    int set = bits(MDTEST_HEADER_INDEX + i);
    CHECK(set > 2 && set < 14);
  }
  // ... and outside the areas the command channel borrows.
  CHECK(MDTEST_HEADER_INDEX + MDTEST_HEADER_WORDS <= MDTEST_COMMAND_BLOCK_INDEX);
  CHECK(MDTEST_HEADER_INDEX + MDTEST_HEADER_WORDS <= MDTEST_BANK_WORDS);

  // The filled window: the pattern everywhere but the header.
  mdtest_fillWindow(window, "v2.0.0", "0123456789abcdef0123456789abcdef-cut");
  int patternErrors = 0;
  for (unsigned int i = 0; i < MDTEST_WINDOW_WORDS; i++) {
    if (!inHeader(i) && window[i] != mdtest_patternWord((uint16_t)i)) patternErrors++;
  }
  CHECK_EQ(patternErrors, 0);

  const uint16_t *header = &window[MDTEST_HEADER_INDEX];
  CHECK_EQ(header[MDTEST_HDR_ID], ('M' << 8) | 'D');
  CHECK_EQ(header[MDTEST_HDR_ID + 3], ('W' << 8) | 'N');
  CHECK_EQ(header[MDTEST_HDR_LAYOUT], MDTEST_LAYOUT_VERSION);
  CHECK_EQ(header[MDTEST_HDR_STATE], MDTEST_STATE_DATA);
  CHECK_EQ(header[MDTEST_HDR_VERSION], ('v' << 8) | '2');
  CHECK_EQ(header[MDTEST_HDR_VERSION + 3], 0);
  // The build ID is cut to 32 characters, with no terminator.
  CHECK_EQ(header[MDTEST_HDR_BUILD + MDTEST_HDR_BUILD_WORDS - 1], ('e' << 8) | 'f');

  uint16_t sum = 0;
  for (unsigned int i = 0; i < MDTEST_HEADER_WORDS; i++) sum += header[i];
  CHECK_EQ(sum, 0);

  mdtest_setState(window, MDTEST_STATE_COMMANDS);
  CHECK_EQ(header[MDTEST_HDR_STATE], MDTEST_STATE_COMMANDS);
  sum = 0;
  for (unsigned int i = 0; i < MDTEST_HEADER_WORDS; i++) sum += header[i];
  CHECK_EQ(sum, 0);
  mdtest_setState(window, MDTEST_STATE_DATA);

  // For test_mdtest.py: big-endian, as the ST reads the window.
  mdtest_fillWindow(window, "v2.0.0", "d7ef4fe+debug");
  FILE *out = fopen("build/mdtest_window.bin", "wb");
  CHECK(out != NULL);
  if (out) {
    for (unsigned int i = 0; i < MDTEST_WINDOW_WORDS; i++) {
      fputc(window[i] >> 8, out);
      fputc(window[i] & 0xFF, out);
    }
    fclose(out);
  }

  TEST_END();
}
