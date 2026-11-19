/**
 * File: mdtest.h
 * Author: Diego Parrilla Santamaría
 * Date: November 2026
 * Copyright: 2026 - GOODDATA LABS SL
 * Description: The cartridge window as both sides see it: its layout, the
 *              header the ST reads with plain reads, and the test pattern.
 *              Plain C, shared by the RP firmware, the ST program, the host
 *              tests and the tools; no offset of the window is written
 *              anywhere else.
 *
 * The window is 128 KB, 65,536 16-bit words, word index 0 to 0xFFFF: ROM4
 * ($FA0000) holds indexes 0 to 0x7FFF and ROM3 ($FB0000) 0x8000 to 0xFFFF,
 * so bit 15 of the index is the bank. Every word is the pattern's, except the
 * header's 32 words.
 */

#ifndef MDTEST_H
#define MDTEST_H

#include <stdint.h>

#define MDTEST_WINDOW_WORDS 0x10000U
#define MDTEST_WINDOW_BYTES (MDTEST_WINDOW_WORDS * 2U)
#define MDTEST_BANK_WORDS 0x8000U
#define MDTEST_ROM4_ADDRESS 0xFA0000ul
#define MDTEST_ROM3_ADDRESS 0xFB0000ul

// The header: 64 bytes in ROM4, just below the block the command channel
// borrows at $FA2000 once ROM3 is selected. Not at the start of the window:
// the address-line tests read the words one or two address bits away from
// index 0 and from 0xFFFF, and the header's indexes (0x0FE0 to 0x0FFF, seven
// to twelve bits set) are none of them.
#define MDTEST_HEADER_INDEX 0x0FE0U
#define MDTEST_HEADER_WORDS 32U

// Word offsets inside the header. Strings are ASCII, two characters a word,
// the first in the high byte (the order the 68000 reads), padded with zeros.
#define MDTEST_HDR_ID 0U            // 4 words: MDTEST_ID
#define MDTEST_HDR_LAYOUT 4U        // MDTEST_LAYOUT_VERSION
#define MDTEST_HDR_STATE 5U         // MDTEST_STATE_*
#define MDTEST_HDR_CHECKSUM 6U      // the header's words sum to 0 (mod 2^16)
#define MDTEST_HDR_RESERVED 7U      // 0
#define MDTEST_HDR_VERSION 8U       // 8 words: the firmware's version
#define MDTEST_HDR_VERSION_WORDS 8U
#define MDTEST_HDR_BUILD 16U        // 16 words: the firmware's build ID
#define MDTEST_HDR_BUILD_WORDS 16U

#define MDTEST_ID "MDTESTWN"
#define MDTEST_LAYOUT_VERSION 1U

// ROM3 as test data (from boot), or selected as the command channel.
#define MDTEST_STATE_DATA 0U
#define MDTEST_STATE_COMMANDS 1U

// Areas the command channel borrows in ROM4 while ROM3 is selected; test data
// until then. The template's shared block keeps chandler.h's offsets.
#define MDTEST_COMMAND_BLOCK_INDEX (0x2000U / 2U)  // $FA2000, chandler.h
#define MDTEST_RESULT_BUFFER_INDEX (0x4000U / 2U)  // $FA4000
#define MDTEST_RESULT_BUFFER_WORDS (0x1000U / 2U)  // 4 KB

// The ST's frames on ROM3, in the template's protocol (header $ABCD, the
// command, the payload size in bytes, a 4-byte token, the payload, the
// checksum). While ROM3 is test data the firmware answers none of them: an
// answer is written into the window, which would corrupt the data under test.
#define MDTEST_APP_ID 0x0DU
// One way, at the end of every line test: words verdict, class, the first
// error's address (high word, low word), the error count (saturated at
// $FFFF), the run's sequence number.
#define MDTEST_CMD_LINE_VERDICT ((MDTEST_APP_ID << 8) | 0x01U)
#define MDTEST_VERDICT_WORDS 6U
#define MDTEST_VERDICT_PASS 0U
#define MDTEST_VERDICT_FAIL 1U
#define MDTEST_VERDICT_STOPPED 2U
// The verdict's class: what the line test's errors point to
// (app/atarist/src/diagnose.h says what each one means).
#define MDTEST_CLASS_NONE 0U
#define MDTEST_CLASS_NO_ANSWER 1U
#define MDTEST_CLASS_DATA_LOW 2U
#define MDTEST_CLASS_DATA_HIGH 3U
#define MDTEST_CLASS_DATA_PAIR 4U
#define MDTEST_CLASS_ADDR_LOW 5U
#define MDTEST_CLASS_ADDR_HIGH 6U
#define MDTEST_CLASS_ADDR_TIED 7U
#define MDTEST_CLASS_BANK_SWAP 8U
#define MDTEST_CLASS_PIN_DEAD 9U
#define MDTEST_CLASS_LANE 10U
#define MDTEST_CLASS_INTERMITTENT 11U
#define MDTEST_CLASS_UNCLASSIFIED 12U

// Faults a debug firmware serves on request, to check the line test names
// them (mdtest_injectFault()). Lines are window index bits: 0 to 14 are
// A1 to A15, 15 is the bank (the !ROM4 and !ROM3 selects).
#define MDTEST_FAULT_NONE 0U
#define MDTEST_FAULT_DATA_LOW 1U      // data bit a always 0
#define MDTEST_FAULT_DATA_HIGH 2U     // data bit a always 1
#define MDTEST_FAULT_DATA_SWAP 3U     // data bits a and b swapped
#define MDTEST_FAULT_ADDR_LOW 4U      // address line a always 0
#define MDTEST_FAULT_ADDR_TIED 5U     // address line b follows line a
#define MDTEST_FAULT_SCATTER 6U       // one word in a wrong, one bit each
#define MDTEST_FAULT_ROM3_IS_ROM4 7U  // ROM3 serves ROM4's words
#define MDTEST_FAULT_PIN_DEAD 8U      // GPIO of data bit a / line a: both low

/**
 * @brief The test pattern's word at a window index.
 *
 * A bijection of the 16-bit index: no two words of the window are equal, so
 * a word read from the wrong address names the address it came from, and
 * every data bit takes both values in each bank and on each byte lane.
 * Index 0 gives 0, so the window never starts with a cartridge magic.
 */
static inline uint16_t mdtest_patternWord(uint16_t index) {
  uint16_t x = index;
  x ^= (uint16_t)(x >> 8);
  x = (uint16_t)(x * 0x6B4DU);
  x ^= (uint16_t)(x >> 7);
  x = (uint16_t)(x * 0x9E37U);
  x ^= (uint16_t)(x >> 8);
  return x;
}

/**
 * @brief The window index whose pattern word is this word: the inverse of
 *        mdtest_patternWord(). A word read from the wrong address names the
 *        address it came from.
 */
static inline uint16_t mdtest_patternIndex(uint16_t word) {
  // Undo each step in reverse: a right xorshift by k is undone by repeating
  // it until every bit is restored; a product by its inverse modulo 2^16.
  uint16_t value = word;
  uint16_t undone = value;
  for (int i = 0; i < 2; i++) undone = (uint16_t)(value ^ (undone >> 8));
  value = (uint16_t)(undone * 0x7787U);  // 0x9E37^-1
  undone = value;
  for (int i = 0; i < 3; i++) undone = (uint16_t)(value ^ (undone >> 7));
  value = (uint16_t)(undone * 0xC585U);  // 0x6B4D^-1
  undone = value;
  for (int i = 0; i < 2; i++) undone = (uint16_t)(value ^ (undone >> 8));
  return undone;
}

/**
 * @brief Write the whole window: the pattern, then the header.
 *
 * @param window MDTEST_WINDOW_WORDS words, in the order the ST reads them.
 * @param version The firmware's version, cut to MDTEST_HDR_VERSION_WORDS * 2.
 * @param build The build ID, cut to MDTEST_HDR_BUILD_WORDS * 2.
 */
void mdtest_fillWindow(uint16_t *window, const char *version,
                       const char *build);

/**
 * @brief Rewrite the header's state word and its checksum.
 */
void mdtest_setState(uint16_t *window, uint16_t state);

/**
 * @brief Turn a freshly filled window into what a faulty bus would deliver.
 *
 * Acts on the served copy only, never on the engine. kind is one of
 * MDTEST_FAULT_*; a and b its bit or line numbers (or N for SCATTER).
 * Returns 0, or -1 for a kind or argument out of range (window unchanged).
 */
int mdtest_injectFault(uint16_t *window, unsigned int kind, unsigned int a,
                       unsigned int b);

#endif  // MDTEST_H
