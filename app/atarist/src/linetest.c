#include "linetest.h"

#include "mdtest.h"

const LtSizes lt_quick = {16384UL, 64UL, 4096UL};
/* v0.0.3's long run: a million random reads. */
const LtSizes lt_long = {1000000UL, 10000UL, 65536UL};

#define POLL_EVERY 4096U
#define BANK_BASE(bank) ((uint32_t)(bank) * MDTEST_BANK_WORDS)

/* A byte as the 68000 reads it: the even address is the word's high byte. */
#ifdef __m68k__
#define READ_BYTE(window, offset) (((const volatile uint8_t *)(window))[offset])
#else
static uint8_t hostByte(const volatile uint16_t *window, uint32_t offset) {
  uint16_t w = window[offset >> 1];
  return (uint8_t)((offset & 1) ? (w & 0xFF) : (w >> 8));
}
#define READ_BYTE(window, offset) hostByte(window, offset)
#endif

static uint8_t expectedByte(uint32_t offset) {
  uint16_t w = mdtest_patternWord((uint16_t)(offset >> 1));
  return (uint8_t)((offset & 1) ? (w & 0xFF) : (w >> 8));
}

static int inHeader(uint32_t index) {
  return index >= MDTEST_HEADER_INDEX &&
         index < MDTEST_HEADER_INDEX + MDTEST_HEADER_WORDS;
}

/* A 16-bit xorshift: shifts only (the 68000 has no 32-bit multiply). */
static uint16_t seed = 0xACE1U;
static uint32_t nextRandom(void) {
  seed ^= (uint16_t)(seed << 7);
  seed ^= (uint16_t)(seed >> 9);
  seed ^= (uint16_t)(seed << 8);
  return seed;
}

/* Something to write between reads: a cycle on the RAM bus. */
static volatile uint16_t ramTarget;

static void irqOff(uint16_t *saved) {
#ifdef __m68k__
  uint16_t sr;
  __asm__ volatile("move.w %%sr,%0\n\tori.w #0x0700,%%sr" : "=d"(sr)::"cc");
  *saved = sr;
#else
  *saved = 0;
#endif
}

static void irqBack(uint16_t saved) {
#ifdef __m68k__
  __asm__ volatile("move.w %0,%%sr" ::"d"(saved) : "cc");
#else
  (void)saved;
#endif
}

static int checkWord(DgStats *stats, const volatile uint16_t *window,
                     uint32_t index) {
  uint16_t expected = mdtest_patternWord((uint16_t)index);
  uint16_t read = window[index];
  if (read != expected) {
    dg_wordError(stats, (uint16_t)index, expected, read);
    return 1;
  }
  return 0;
}

static void checkByte(DgStats *stats, const volatile uint16_t *window,
                      uint32_t offset) {
  uint8_t expected = expectedByte(offset);
  uint8_t read = READ_BYTE(window, offset);
  if (read != expected) dg_byteError(stats, offset, expected, read);
}

uint32_t lt_reads(LtTest test, const LtSizes *sizes) {
  switch (test) {
    case LT_SEQ_WORDS:
    case LT_IRQ_OFF:
      return MDTEST_BANK_WORDS;
    case LT_SEQ_BYTES:
      return 2UL * MDTEST_BANK_WORDS;
    case LT_RANDOM_WORDS:
    case LT_RANDOM_BYTES:
      return sizes->randomReads;
    case LT_ADDRESS_WALK:
      return 32UL * sizes->walkRounds;
    default:
      return sizes->paceReads;
  }
}

int lt_run(LtTest test, int bank, const volatile uint16_t *window,
           const LtSizes *sizes, DgStats *stats, LtStop stop) {
  const uint32_t base = BANK_BASE(bank);
  uint32_t i;
  uint32_t words = 0;
  uint32_t bytes[2] = {0, 0};
  int stopped = 0;

  switch (test) {
    case LT_SEQ_WORDS:
      for (i = 0; i < MDTEST_BANK_WORDS && !stopped; i++) {
        if (inHeader(base + i)) continue;
        checkWord(stats, window, base + i);
        words++;
        if ((i % POLL_EVERY) == 0 && stop && stop()) stopped = 1;
      }
      break;

    case LT_IRQ_OFF: {
      uint16_t sr;
      irqOff(&sr);
      for (i = 0; i < MDTEST_BANK_WORDS; i++) {
        if (inHeader(base + i)) continue;
        checkWord(stats, window, base + i);
        words++;
      }
      irqBack(sr);
      break;
    }

    case LT_SEQ_BYTES:
      /* Both bytes of each word, the word's pattern computed once. */
      for (i = 0; i < MDTEST_BANK_WORDS && !stopped; i++) {
        if (inHeader(base + i)) continue;
        uint16_t w = mdtest_patternWord((uint16_t)(base + i));
        uint32_t offset = 2UL * (base + i);
        uint8_t high = READ_BYTE(window, offset);
        uint8_t low = READ_BYTE(window, offset + 1);
        if (high != (uint8_t)(w >> 8)) {
          dg_byteError(stats, offset, (uint8_t)(w >> 8), high);
        }
        if (low != (uint8_t)w) dg_byteError(stats, offset + 1, (uint8_t)w, low);
        bytes[0]++;
        bytes[1]++;
        if ((i % POLL_EVERY) == 0 && stop && stop()) stopped = 1;
      }
      break;

    case LT_RANDOM_WORDS:
      for (i = 0; i < sizes->randomReads && !stopped; i++) {
        uint32_t index = base + (nextRandom() & (MDTEST_BANK_WORDS - 1));
        if (inHeader(index)) continue;
        checkWord(stats, window, index);
        words++;
        if ((i % POLL_EVERY) == 0 && stop && stop()) stopped = 1;
      }
      break;

    case LT_RANDOM_BYTES:
      for (i = 0; i < sizes->randomReads && !stopped; i++) {
        uint32_t offset =
            2UL * base + (nextRandom() & (2UL * MDTEST_BANK_WORDS - 1));
        if (inHeader(offset >> 1)) continue;
        checkByte(stats, window, offset);
        bytes[offset & 1]++;
        if ((i % POLL_EVERY) == 0 && stop && stop()) stopped = 1;
      }
      break;

    case LT_ADDRESS_WALK:
      /* Each line alone high, then alone low, within the bank: A1 to A15. */
      for (i = 0; i < sizes->walkRounds && !stopped; i++) {
        for (int k = 0; k < 15; k++) {
          uint32_t one = 1UL << k;
          checkWord(stats, window, base + one);
          checkWord(stats, window, base + ((MDTEST_BANK_WORDS - 1) & ~one));
          words += 2;
        }
        checkWord(stats, window, base);
        checkWord(stats, window, base + MDTEST_BANK_WORDS - 1);
        words += 2;
        if ((i % 64) == 0 && stop && stop()) stopped = 1;
      }
      break;

    case LT_SAME_ADDRESS:
      /* Back to back: the second read must equal the first. */
      for (i = 0; i < sizes->paceReads && !stopped; i += 8) {
        uint32_t index = base + (nextRandom() & (MDTEST_BANK_WORDS - 1));
        if (inHeader(index)) continue;
        uint16_t first = window[index];
        for (int r = 0; r < 7; r++) {
          uint16_t again = window[index];
          if (again != first) dg_unstable(stats, (uint16_t)index);
        }
        if (first != mdtest_patternWord((uint16_t)index)) {
          dg_wordError(stats, (uint16_t)index,
                       mdtest_patternWord((uint16_t)index), first);
        }
        words += 8;
        if ((i % POLL_EVERY) == 0 && stop && stop()) stopped = 1;
      }
      break;

    case LT_FAR_ADDRESSES:
      /* An address, then its complement: every address line changes. */
      for (i = 0; i < sizes->paceReads && !stopped; i += 2) {
        uint32_t low = nextRandom() & (MDTEST_BANK_WORDS - 1);
        uint32_t high = (MDTEST_BANK_WORDS - 1) & ~low;
        if (inHeader(base + low) || inHeader(base + high)) continue;
        checkWord(stats, window, base + low);
        checkWord(stats, window, base + high);
        words += 2;
        if ((i % POLL_EVERY) == 0 && stop && stop()) stopped = 1;
      }
      break;

    case LT_AFTER_RAM:
      for (i = 0; i < sizes->paceReads && !stopped; i++) {
        uint32_t index = base + (nextRandom() & (MDTEST_BANK_WORDS - 1));
        if (inHeader(index)) continue;
        ramTarget = (uint16_t)i;
        checkWord(stats, window, index);
        words++;
        if ((i % POLL_EVERY) == 0 && stop && stop()) stopped = 1;
      }
      break;

    default:
      break;
  }
  if (words) dg_countWords(stats, bank, words);
  if (bytes[0]) dg_countBytes(stats, bank, 0, bytes[0]);
  if (bytes[1]) dg_countBytes(stats, bank, 1, bytes[1]);
  return stopped;
}
