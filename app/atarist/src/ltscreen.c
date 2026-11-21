#include "ltscreen.h"

#include <stdio.h>

#include "console.h"
#include "diagnose.h"
#include "hw.h"
#include "mdtest.h"
#include "texts.h"

#define WINDOW ((const volatile uint16_t *)MDTEST_ROM4_ADDRESS)
#define HZ_200 hw_ticks
/* Microseconds a read takes on an 8 MHz 68000, the pattern computed with
 * it: measured in Hatari's cycle-exact ST. */
#define US_PER_READ 60UL

static const char *const testNames[LT_TESTS] = {
    T_TEST_SEQ_WORDS,     T_TEST_SEQ_BYTES,    T_TEST_RANDOM_WORDS,
    T_TEST_RANDOM_BYTES,  T_TEST_ADDRESS_WALK, T_TEST_SAME_ADDRESS,
    T_TEST_FAR_ADDRESSES, T_TEST_AFTER_RAM,    T_TEST_IRQ_OFF};

static const char *const bankNames[2] = {T_BANK_ROM4, T_BANK_ROM3};
/* The table: a row per test, a column per bank. */
static const int cellColumn[2] = {21, 42};
#define NAME_COLUMN 2
#define CELL_WIDTH 20
static const int selectGpio[2] = {22, 26};

static DgStats stats;
static unsigned short runNumber;

static int keyPressed(void) { return console_keyPressed(); }

unsigned long ltscreen_estimate(const LtSizes *sizes) {
  unsigned long reads = 0;
  for (int t = 0; t < LT_TESTS; t++) reads += 2 * lt_reads((LtTest)t, sizes);
  return reads / (1000000UL / US_PER_READ) + 1;
}

/* One frame of ROM3 reads, the template's way: each word is the low half of
 * an address read in ROM3, offset from its middle. One way: no answer. */
static void sendFrame(unsigned short command, const unsigned short *words,
                      int count) {
  const volatile unsigned char *rom3 =
      (const volatile unsigned char *)(MDTEST_ROM3_ADDRESS + 0x8000UL);
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
}

static void showFirstErrors(const DgBank *b) {
  for (int i = 0; i < b->firstCount && i < 3; i++) {
    const DgError *e = &b->first[i];
    console_line(e->bytes == 1 ? T_FIND_FIRST_BYTE : T_FIND_FIRST,
                 (unsigned long)e->address, e->expected, e->read);
  }
}

static void showFinding(const DgFinding *f, const DgBank *b, int bank) {
  switch (f->kind) {
    case DG_NO_ANSWER:
      console_line(T_FIND_NO_ANSWER, bankNames[bank]);
      console_line(T_FIND_NO_ANSWER_DETAIL);
      console_line(T_FIND_NO_ANSWER_ADVICE, bankNames[bank], selectGpio[bank]);
      return;
    case DG_DATA_LOW:
    case DG_DATA_HIGH:
      console_line(f->kind == DG_DATA_LOW ? T_FIND_DATA_LOW : T_FIND_DATA_HIGH,
                   f->a);
      console_line(T_FIND_DATA_DETAIL, 6 + f->a, (unsigned long)f->errors,
                   (unsigned long)f->reads);
      console_line(T_FIND_DATA_ADVICE, f->a);
      break;
    case DG_DATA_PAIR:
      console_line(T_FIND_DATA_PAIR, f->a, f->b);
      console_line(T_FIND_PAIR_ADVICE);
      break;
    case DG_ADDR_LOW:
    case DG_ADDR_HIGH:
      console_line(f->kind == DG_ADDR_LOW ? T_FIND_ADDR_LOW : T_FIND_ADDR_HIGH,
                   f->a);
      console_line(T_FIND_ADDR_DETAIL, 6 + f->a);
      console_line(T_FIND_ADDR_ADVICE, f->a);
      break;
    case DG_ADDR_TIED:
      console_line(T_FIND_ADDR_TIED, f->a, f->b);
      console_line(T_FIND_PAIR_ADVICE);
      break;
    case DG_BANK_SWAP:
      console_line(T_FIND_BANK_SWAP);
      console_line(T_FIND_BANK_SWAP_ADVICE);
      break;
    case DG_PIN_DEAD:
      console_line(T_FIND_PIN_DEAD, 6 + f->a);
      console_line(T_FIND_PIN_DETAIL, f->a, f->a);
      console_line(T_FIND_PIN_ADVICE, 6 + f->a);
      break;
    case DG_LANE:
      console_line(T_FIND_LANE, f->a ? T_FIND_LANE_ODD : T_FIND_LANE_EVEN);
      console_line(T_FIND_LANE_ADVICE, f->a ? "D0-D7" : "D8-D15");
      break;
    case DG_INTERMITTENT:
      console_line(T_FIND_INTERMITTENT, (unsigned long)f->errors,
                   (unsigned long)f->reads);
      console_line(T_FIND_INTERMITTENT_ADVICE);
      break;
    default:
      console_line(T_FIND_UNCLASSIFIED);
      console_line(T_FIND_UNCLASSIFIED_ADVICE);
      break;
  }
  if (b->unstable) console_line(T_FIND_UNSTABLE, (unsigned long)b->unstable);
  showFirstErrors(b);
}

static int sameFinding(const DgFinding *x, const DgFinding *y) {
  return x->kind == y->kind && x->a == y->a && x->b == y->b;
}

/* A cell of the table, padded so it clears what it replaces. */
static void cell(int row, int bank, const char *text) {
  char padded[CELL_WIDTH + 1];
  snprintf(padded, sizeof(padded), "%-*s", CELL_WIDTH, text);
  console_at(cellColumn[bank], row, padded);
}

LtVerdict ltscreen_run(const LtSizes *sizes, unsigned long estimateSeconds) {
  int stopped = 0;
  char text[CELL_WIDTH + 1];
  dg_reset(&stats);
  runNumber++;
  console_page(T_LINE_TITLE);
  if (estimateSeconds < 120) {
    console_line(T_LINE_QUICK, estimateSeconds);
  } else {
    console_line(T_LINE_LONG, estimateSeconds / 60);
  }
  console_status(T_LINE_STATUS, 0);

  /* The table, drawn once; each test's cell filled as it ends. Every row is
   * also recorded as a line, as the report and the host's log have it. */
  int top = console_bodyRow() + 1;
  console_at(NAME_COLUMN, top, T_LINE_HEAD_TEST);
  for (int bank = 0; bank < 2; bank++) {
    console_at(cellColumn[bank], top, bankNames[bank]);
  }
  for (int t = 0; t < LT_TESTS; t++) {
    console_at(NAME_COLUMN, top + 1 + t, testNames[t]);
  }
  console_bodyFrom(top + 2 + LT_TESTS);

  for (int t = 0; t < LT_TESTS && !stopped; t++) {
    int row = top + 1 + t;
    for (int bank = 0; bank < 2 && !stopped; bank++) {
      cell(row, bank, T_LINE_CELL_RUNNING);
      uint32_t before = stats.bank[bank].errors;
      unsigned long start = HZ_200;
      stopped = lt_run((LtTest)t, bank, WINDOW, sizes, &stats, keyPressed);
      unsigned long ticks = HZ_200 - start;
      uint32_t errors = stats.bank[bank].errors - before;
      if (stopped) {
        console_record(T_LINE_ROW_STOPPED, testNames[t], bankNames[bank]);
        cell(row, bank, T_OUT_STOPPED);
      } else if (errors) {
        console_record(T_LINE_ROW_ERRORS, testNames[t], bankNames[bank],
                       (unsigned long)errors);
        snprintf(text, sizeof(text), T_LINE_CELL_ERRORS,
                 (unsigned long)errors);
        cell(row, bank, text);
      } else if (t == LT_IRQ_OFF) {
        /* The 200 Hz timer stops with the interrupts. */
        console_record(T_LINE_ROW_NOTIME, testNames[t], bankNames[bank]);
        cell(row, bank, T_OUT_PASS);
      } else {
        console_record(T_LINE_ROW, testNames[t], bankNames[bank], "PASS",
                       ticks / 200, (ticks % 200) / 20);
        snprintf(text, sizeof(text), T_LINE_CELL_PASS, ticks / 200,
                 (ticks % 200) / 20);
        cell(row, bank, text);
      }
    }
  }

  DgFinding f[2] = {dg_classify(&stats.bank[0]), dg_classify(&stats.bank[1])};
  unsigned short verdict[MDTEST_VERDICT_WORDS] = {0};
  LtVerdict result;
  if (f[0].kind == DG_NONE && f[1].kind == DG_NONE && !stopped) {
    console_line(T_GATE_PASS);
    verdict[0] = MDTEST_VERDICT_PASS;
    result = LT_PASS;
  } else {
    console_blank();
    if (f[0].kind == DG_NO_ANSWER && f[1].kind == DG_NO_ANSWER) {
      console_line(T_FIND_NONE_AT_ALL);
      console_line(T_FIND_NONE_AT_ALL_ADVICE);
    } else if (f[0].kind != DG_NONE && f[1].kind != DG_NONE &&
               sameFinding(&f[0], &f[1]) && f[0].kind != DG_NO_ANSWER) {
      console_line(T_FIND_BANK, T_BOTH_BANKS);
      showFinding(&f[0], &stats.bank[0], 0);
    } else {
      for (int bank = 0; bank < 2; bank++) {
        if (f[bank].kind == DG_NONE) continue;
        console_line(T_FIND_BANK, bankNames[bank]);
        showFinding(&f[bank], &stats.bank[bank], bank);
      }
    }
    if (stopped) console_line(T_GATE_STOPPED);
    console_line(T_GATE_FAIL);
    int bank = f[0].kind != DG_NONE ? 0 : 1;
    const DgBank *b = &stats.bank[bank];
    verdict[0] = stopped ? MDTEST_VERDICT_STOPPED : MDTEST_VERDICT_FAIL;
    verdict[1] = (unsigned short)f[bank].kind;
    if (b->firstCount) {
      verdict[2] = (unsigned short)(b->first[0].address >> 16);
      verdict[3] = (unsigned short)b->first[0].address;
    }
    uint32_t errors = stats.bank[0].errors + stats.bank[1].errors;
    verdict[4] = errors > 0xFFFF ? 0xFFFF : (unsigned short)errors;
    result = stopped ? LT_STOPPED : LT_FAIL;
  }
  verdict[5] = runNumber;
  sendFrame(MDTEST_CMD_LINE_VERDICT, verdict, MDTEST_VERDICT_WORDS);
  return result;
}
