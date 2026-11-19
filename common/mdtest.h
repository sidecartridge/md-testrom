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

// ---------------------------------------------------------------------------
// The command channel: ROM3 selected (MDTEST_STATE_COMMANDS).
//
// The program sends commands as frames of ROM3 reads; the firmware answers
// each by writing the frame's token at $FA2004 (the template's handshake),
// after it has written the answer record at $FA2100 and, for the commands
// that return data, the result buffer at $FA4000. Those areas, and the
// template's block at $FA2000, are test data again once ROM3 is unselected.
#define MDTEST_PROTOCOL_VERSION 1U

// ROM3 becomes the command channel. Accepted while ROM3 is data only with
// these eight words as its payload (after the token): no read pattern of the
// line test forms it (tests/host/test_select.c). Answered; the header's
// state word then says MDTEST_STATE_COMMANDS.
#define MDTEST_CMD_SELECT ((MDTEST_APP_ID << 8) | 0x02U)
#define MDTEST_SELECT_WORDS 8U
#define MDTEST_SELECT_MAGIC \
  {0x4D44U, 0x5445U, 0x5354U, 0x2053U, 0x454CU, 0x4543U, 0x5420U, 0x5233U}
// Back to data: answered, then the whole window is the pattern again and the
// state word says MDTEST_STATE_DATA (the program waits for it).
#define MDTEST_CMD_UNSELECT ((MDTEST_APP_ID << 8) | 0x03U)
// Words: the program's protocol version, its machine (_MCH, high word then
// low). Answer: MDTEST_ANS_DATA = the firmware's protocol version, then its
// capabilities (MDTEST_CAP_*, high word then low). The firmware's version and
// build ID are in the header.
#define MDTEST_CMD_HELLO ((MDTEST_APP_ID << 8) | 0x04U)
// Words: any, up to MDTEST_ECHO_MAX_WORDS. Answer: the same words in the
// result buffer, their count in MDTEST_ANS_LENGTH.
#define MDTEST_CMD_ECHO ((MDTEST_APP_ID << 8) | 0x05U)
// Words: the test (MDTEST_TEST_*), a sequence number, four parameters.
// Answer: MDTEST_ANS_DATA = the job's number. A start while a job runs is
// refused (MDTEST_STATUS_BUSY, the running job's number); a start with the
// last start's sequence number answers that job again and starts nothing.
#define MDTEST_CMD_JOB_START ((MDTEST_APP_ID << 8) | 0x06U)
// Answer: MDTEST_ANS_DATA = the job's number, state (MDTEST_JOB_*), progress
// (0 to 100), test; once it has ended its result is in the result buffer,
// MDTEST_ANS_LENGTH bytes of "key=value" lines.
#define MDTEST_CMD_JOB_STATUS ((MDTEST_APP_ID << 8) | 0x07U)
#define MDTEST_CMD_JOB_CANCEL ((MDTEST_APP_ID << 8) | 0x08U)
// Words: the sequence number (MDTEST_ANS_SEQ), the length in bytes, then the
// text, two characters a word. One line of the program's screen, mirrored to
// the RP. Answered at once.
#define MDTEST_CMD_LOG ((MDTEST_APP_ID << 8) | 0x09U)
// Words: the sequence number (MDTEST_ANS_SEQ). Answer: MDTEST_ANS_DATA = what
// the bench asks the program to do next (MDTEST_REQUEST_*), the test, four
// parameters. A request is handed out once.
//
// LOG and REQUEST change the RP's state, so a resend after a lost answer must
// not run twice. The RP serves the next sequence number in every answer
// record; the program reads it once before sending and every retry carries
// it; the RP runs a command whose number is not the last one it ran, and only
// then serves a new number. A repeat is answered again (REQUEST with the same
// request) and changes nothing.
#define MDTEST_CMD_REQUEST ((MDTEST_APP_ID << 8) | 0x0AU)
// Answer: the RP's command counters in the result buffer, MDTEST_COUNTERS_*
// order, each a long (high word first).
#define MDTEST_CMD_COUNTERS ((MDTEST_APP_ID << 8) | 0x0BU)

// The answer record, at $FA2100 (after the template's block): every app
// command's answer, written before its token.
#define MDTEST_ANSWER_INDEX (0x2100U / 2U)
#define MDTEST_ANS_COMMAND 0U   // the command answered
#define MDTEST_ANS_STATUS 1U    // MDTEST_STATUS_*
#define MDTEST_ANS_BOOT 2U      // the RP's boot ID, high word then low
#define MDTEST_ANS_COUNT 4U     // answers since the RP started (low word)
#define MDTEST_ANS_LENGTH 5U    // what the result buffer holds
#define MDTEST_ANS_SEQ 6U       // the sequence number LOG and REQUEST carry next
#define MDTEST_ANS_DATA 7U      // the command's own words
#define MDTEST_ANS_DATA_WORDS 8U
#define MDTEST_ANSWER_WORDS (MDTEST_ANS_DATA + MDTEST_ANS_DATA_WORDS)

#define MDTEST_STATUS_OK 0U
#define MDTEST_STATUS_BAD 1U       // wrong size or parameter
#define MDTEST_STATUS_BUSY 2U      // a job runs
#define MDTEST_STATUS_UNKNOWN 3U   // a test this firmware does not have

// The payload a frame carries is 2048 bytes, the token included.
#define MDTEST_ECHO_MAX_WORDS 1020U

// Capabilities: the tests this build has.
#define MDTEST_CAP_DUMMY 0x00000001UL

// The tests a job runs.
#define MDTEST_TEST_DUMMY 1U  // parameters: seconds; result: seconds, steps

#define MDTEST_JOB_IDLE 0U
#define MDTEST_JOB_RUNNING 1U
#define MDTEST_JOB_DONE 2U
#define MDTEST_JOB_FAILED 3U
#define MDTEST_JOB_CANCELLED 4U

#define MDTEST_REQUEST_NONE 0U
#define MDTEST_REQUEST_RUN 1U        // run the test (a job), print its result
#define MDTEST_REQUEST_LINE_TEST 2U  // unselect, run the line test, select
#define MDTEST_REQUEST_STOP 3U       // leave the bench loop
#define MDTEST_REQUEST_CHANNEL_TEST 4U  // the command channel's own test

#define MDTEST_COUNTERS_HANDLED 0U
#define MDTEST_COUNTERS_DROPPED 1U
#define MDTEST_COUNTERS_REPEATED 2U
#define MDTEST_COUNTERS_CHECKSUM 3U
#define MDTEST_COUNTERS_OVERRUNS 4U
#define MDTEST_COUNTERS_LOGS 5U
#define MDTEST_COUNTERS_WORDS 12U  // 6 longs

// What a protocol mismatch asks the user to update.
#define MDTEST_UPDATE_NOTHING 0
#define MDTEST_UPDATE_FIRMWARE 1
#define MDTEST_UPDATE_PROGRAM 2

static inline int mdtest_updateAdvice(unsigned int program,
                                      unsigned int firmware) {
  if (program == firmware) return MDTEST_UPDATE_NOTHING;
  return program > firmware ? MDTEST_UPDATE_FIRMWARE : MDTEST_UPDATE_PROGRAM;
}

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

/**
 * @brief Put the pattern back in words [first, first + count) of the window
 *        (the header's words get the header's: call mdtest_setState after).
 */
void mdtest_restoreWords(uint16_t *window, uint32_t first, uint32_t count);

/**
 * @brief Text into window words, two characters a word, the first in the
 *        high byte (as the 68000 reads it); an odd length's last low byte 0.
 */
void mdtest_putText(uint16_t *words, const char *text, uint32_t length);

/**
 * @brief Text out of words written as mdtest_putText writes them.
 */
void mdtest_getText(const uint16_t *words, char *text, uint32_t length);

#endif  // MDTEST_H
