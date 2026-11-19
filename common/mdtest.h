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

#endif  // MDTEST_H
