// units: common/mdtest.c app/atarist/src/linetest.c app/atarist/src/diagnose.c
/* The line test's own code (app/atarist/src/linetest.c and diagnose.c) run
 * against a window with each fault the debug firmware can serve injected:
 * the diagnosis must name it, bank by bank. */
#include <string.h>

#include "diagnose.h"
#include "linetest.h"
#include "mdtest.h"
#include "test.h"

static uint16_t window[MDTEST_WINDOW_WORDS];

static void runAll(DgStats *stats) {
  dg_reset(stats);
  for (int bank = 0; bank < 2; bank++) {
    for (int t = 0; t < LT_TESTS; t++) {
      lt_run((LtTest)t, bank, window, &lt_quick, stats, NULL);
    }
  }
}

static void expect(const char *what, unsigned int kind, unsigned int a,
                   unsigned int b, int bank, int dgKind, int dgA, int dgB) {
  DgStats stats;
  mdtest_fillWindow(window, "v2.0.0", "test");
  CHECK_EQ(mdtest_injectFault(window, kind, a, b), 0);
  runAll(&stats);
  DgFinding f = dg_classify(&stats.bank[bank]);
  if (f.kind != dgKind || f.a != dgA || f.b != dgB) {
    fprintf(stderr, "%s, bank %d: finding %d (%d, %d), %lu errors in %lu reads\n",
            what, bank, f.kind, f.a, f.b, (unsigned long)f.errors,
            (unsigned long)f.reads);
  }
  CHECK_EQ(f.kind, dgKind);
  CHECK_EQ(f.a, dgA);
  CHECK_EQ(f.b, dgB);
}

int main(void) {
  for (int bank = 0; bank < 2; bank++) {
    expect("no fault", MDTEST_FAULT_NONE, 0, 0, bank, DG_NONE, 0, 0);
    expect("D3 low", MDTEST_FAULT_DATA_LOW, 3, 0, bank, DG_DATA_LOW, 3, 0);
    expect("D12 high", MDTEST_FAULT_DATA_HIGH, 12, 0, bank, DG_DATA_HIGH, 12, 0);
    expect("D2/D9 swapped", MDTEST_FAULT_DATA_SWAP, 2, 9, bank, DG_DATA_PAIR, 2, 9);
    expect("A5 low", MDTEST_FAULT_ADDR_LOW, 4, 0, bank, DG_ADDR_LOW, 5, 0);
    expect("A8 follows A3", MDTEST_FAULT_ADDR_TIED, 2, 7, bank, DG_ADDR_TIED, 8, 3);
    expect("scattered", MDTEST_FAULT_SCATTER, 97, 0, bank, DG_INTERMITTENT, 0, 0);
    expect("pin 6 dead", MDTEST_FAULT_PIN_DEAD, 6, 0, bank, DG_PIN_DEAD, 6, 0);
  }
  expect("ROM3 serves ROM4", MDTEST_FAULT_ROM3_IS_ROM4, 0, 0, 0, DG_NONE, 0, 0);
  expect("ROM3 serves ROM4", MDTEST_FAULT_ROM3_IS_ROM4, 0, 0, 1, DG_BANK_SWAP, 0, 0);

  // ROM3 reads open bus (EmuMD, or a dead !ROM3): that bank says so, the
  // other passes.
  {
    DgStats stats;
    mdtest_fillWindow(window, "v2.0.0", "test");
    for (unsigned int i = MDTEST_BANK_WORDS; i < MDTEST_WINDOW_WORDS; i++) window[i] = 0xFFFF;
    runAll(&stats);
    CHECK_EQ(dg_classify(&stats.bank[0]).kind, DG_NONE);
    CHECK_EQ(dg_classify(&stats.bank[1]).kind, DG_NO_ANSWER);
  }

  // A refused fault leaves the window whole.
  mdtest_fillWindow(window, "v2.0.0", "test");
  CHECK_EQ(mdtest_injectFault(window, 99, 0, 0), -1);
  CHECK_EQ(mdtest_injectFault(window, MDTEST_FAULT_DATA_LOW, 16, 0), -1);
  CHECK_EQ(window[5], mdtest_patternWord(5));

  TEST_END();
}
