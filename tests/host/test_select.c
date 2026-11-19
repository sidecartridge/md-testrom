// units: rp/src/tprotocol.c common/mdtest.c app/atarist/src/linetest.c app/atarist/src/diagnose.c
/* ROM3 becomes the command channel on one frame (common/mdtest.h). While ROM3
 * is data the firmware parses every ROM3 read the line test makes: none of
 * those reads, in any test, with any seed, may form the select frame. Every
 * ROM3 read goes through the firmware's own parser, as the RP sees it (the
 * byte offset in ROM3, the protocol's word flipped at bit 15). */
#include <string.h>

#include "diagnose.h"
#include "linetest.h"
#include "mdtest.h"
#include "test.h"
#include "tprotocol.h"

host_timer_hw_t host_timer;

static uint16_t window[MDTEST_WINDOW_WORDS];
static const uint16_t magic[MDTEST_SELECT_WORDS] = MDTEST_SELECT_MAGIC;

static unsigned long frames;
static unsigned long selectCommands;
static unsigned long selectFrames;

static void onFrame(const TransmissionProtocol *p) {
  frames++;
  if (p->command_id != MDTEST_CMD_SELECT) return;
  selectCommands++;
  if (p->payload_size == 4U + 2U * MDTEST_SELECT_WORDS &&
      memcmp((const uint16_t *)p->payload + 2, magic, sizeof(magic)) == 0) {
    selectFrames++;
  }
}

static void onChecksumError(const TransmissionProtocol *p) { (void)p; }

// A ROM3 read as the RP's capture parses it; reads come microseconds apart.
static void rp(uint16_t word) {
  host_timer.timerawl += 3;
  tprotocol_parse(word, onFrame, onChecksumError);
}

static void onRead(uint32_t byteOffset) {
  if (byteOffset < 2UL * MDTEST_BANK_WORDS) return;  // ROM4: not captured
  uint16_t rom3Offset = (uint16_t)(byteOffset - 2UL * MDTEST_BANK_WORDS);
  rp((uint16_t)(rom3Offset ^ 0x8000U));
}

static void frame(uint16_t command, const uint16_t *words, int count) {
  uint16_t size = (uint16_t)(4 + 2 * count);
  uint16_t sum = (uint16_t)(command + size);
  rp(PROTOCOL_HEADER);
  rp(command);
  rp(size);
  rp(0);
  rp(0);
  for (int i = 0; i < count; i++) {
    sum = (uint16_t)(sum + words[i]);
    rp(words[i]);
  }
  rp(sum);
}

static void lineTest(const LtSizes *sizes, uint16_t seed) {
  DgStats stats;
  dg_reset(&stats);
  lt_seed(seed);
  for (int bank = 0; bank < 2; bank++) {
    for (int t = 0; t < LT_TESTS; t++) {
      lt_run((LtTest)t, bank, window, sizes, &stats, NULL);
    }
  }
  // The run's one-way verdict, as the program sends it after the reads.
  const uint16_t verdict[MDTEST_VERDICT_WORDS] = {1, 2, 0x00FA, 0x1234,
                                                  0xFFFF, seed};
  frame(MDTEST_CMD_LINE_VERDICT, verdict, MDTEST_VERDICT_WORDS);
}

int main(void) {
  mdtest_fillWindow(window, "v2.0.0", "test");
  lt_hostRead = onRead;

  // The parser does see a select frame: the check below means something.
  frame(MDTEST_CMD_SELECT, magic, MDTEST_SELECT_WORDS);
  CHECK_EQ(selectFrames, 1);
  frames = selectCommands = selectFrames = 0;

  // The quick pass with many seeds, the long one with a few; on a whole
  // window and on one whose bus is faulty (the reads then differ only in
  // the data, not the addresses, but the run takes other paths).
  for (uint16_t seed = 1; seed <= 64; seed++) lineTest(&lt_quick, seed);
  for (uint16_t seed = 0xACE1; seed < 0xACE1 + 3; seed++) {
    lineTest(&lt_long, seed);
  }
  CHECK_EQ(mdtest_injectFault(window, MDTEST_FAULT_ROM3_IS_ROM4, 0, 0), 0);
  lineTest(&lt_quick, 7);

  printf("%lu frames parsed from the line test's reads\n", frames);
  CHECK_EQ(selectCommands, 0);
  CHECK_EQ(selectFrames, 0);
  // The verdicts did parse: the stream reached the parser intact.
  CHECK(frames >= 68);
  TEST_END();
}
