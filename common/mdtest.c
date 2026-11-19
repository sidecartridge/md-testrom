/**
 * File: mdtest.c
 * Author: Diego Parrilla Santamaría
 * Date: November 2026
 * Copyright: 2026 - GOODDATA LABS SL
 * Description: The window's contents (mdtest.h), written by the RP firmware.
 */

#include "mdtest.h"

static void putString(uint16_t *words, unsigned int count, const char *text) {
  for (unsigned int i = 0; i < count; i++) {
    uint16_t high = 0;
    uint16_t low = 0;
    if (text && *text) high = (uint8_t)*text++;
    if (text && *text) low = (uint8_t)*text++;
    words[i] = (uint16_t)((high << 8) | low);
  }
}

static void sealHeader(uint16_t *header) {
  uint16_t sum = 0;
  header[MDTEST_HDR_CHECKSUM] = 0;
  for (unsigned int i = 0; i < MDTEST_HEADER_WORDS; i++) sum += header[i];
  header[MDTEST_HDR_CHECKSUM] = (uint16_t)(0U - sum);
}

void mdtest_fillWindow(uint16_t *window, const char *version,
                       const char *build) {
  for (uint32_t i = 0; i < MDTEST_WINDOW_WORDS; i++) {
    window[i] = mdtest_patternWord((uint16_t)i);
  }
  uint16_t *header = &window[MDTEST_HEADER_INDEX];
  putString(&header[MDTEST_HDR_ID], 4, MDTEST_ID);
  header[MDTEST_HDR_LAYOUT] = MDTEST_LAYOUT_VERSION;
  header[MDTEST_HDR_STATE] = MDTEST_STATE_DATA;
  header[MDTEST_HDR_RESERVED] = 0;
  putString(&header[MDTEST_HDR_VERSION], MDTEST_HDR_VERSION_WORDS, version);
  putString(&header[MDTEST_HDR_BUILD], MDTEST_HDR_BUILD_WORDS, build);
  sealHeader(header);
}

void mdtest_setState(uint16_t *window, uint16_t state) {
  uint16_t *header = &window[MDTEST_HEADER_INDEX];
  header[MDTEST_HDR_STATE] = state;
  sealHeader(header);
}

void mdtest_restoreWords(uint16_t *window, uint32_t first, uint32_t count) {
  for (uint32_t i = first; i < first + count && i < MDTEST_WINDOW_WORDS; i++) {
    if (i >= MDTEST_HEADER_INDEX &&
        i < MDTEST_HEADER_INDEX + MDTEST_HEADER_WORDS) {
      continue;
    }
    window[i] = mdtest_patternWord((uint16_t)i);
  }
}

void mdtest_putText(uint16_t *words, const char *text, uint32_t length) {
  for (uint32_t i = 0; i < length; i += 2) {
    uint16_t high = (uint8_t)text[i];
    uint16_t low = (i + 1 < length) ? (uint8_t)text[i + 1] : 0;
    words[i / 2] = (uint16_t)((high << 8) | low);
  }
}

void mdtest_getText(const uint16_t *words, char *text, uint32_t length) {
  for (uint32_t i = 0; i < length; i++) {
    uint16_t w = words[i / 2];
    text[i] = (char)((i & 1) ? (w & 0xFF) : (w >> 8));
  }
}

static uint16_t savedHeader[MDTEST_HEADER_WORDS];

// The fresh window's word at an index, whatever the fault did to it since.
static uint16_t original(uint32_t index) {
  if (index >= MDTEST_HEADER_INDEX &&
      index < MDTEST_HEADER_INDEX + MDTEST_HEADER_WORDS) {
    return savedHeader[index - MDTEST_HEADER_INDEX];
  }
  return mdtest_patternWord((uint16_t)index);
}

int mdtest_injectFault(uint16_t *window, unsigned int kind, unsigned int a,
                       unsigned int b) {
  if (a > 15U || b > 15U) {
    if (kind != MDTEST_FAULT_SCATTER) return -1;
  }
  for (unsigned int i = 0; i < MDTEST_HEADER_WORDS; i++) {
    savedHeader[i] = window[MDTEST_HEADER_INDEX + i];
  }
  const uint32_t bitA = 1UL << (a & 15U);
  const uint32_t bitB = 1UL << (b & 15U);
  for (uint32_t i = 0; i < MDTEST_WINDOW_WORDS; i++) {
    uint16_t word = original(i);
    switch (kind) {
      case MDTEST_FAULT_NONE:
        break;
      case MDTEST_FAULT_DATA_LOW:
        word &= (uint16_t)~bitA;
        break;
      case MDTEST_FAULT_DATA_HIGH:
        word |= (uint16_t)bitA;
        break;
      case MDTEST_FAULT_DATA_SWAP: {
        uint16_t bitAValue = (word & bitA) ? 1U : 0U;
        uint16_t bitBValue = (word & bitB) ? 1U : 0U;
        word &= (uint16_t)~(bitA | bitB);
        if (bitAValue) word |= (uint16_t)bitB;
        if (bitBValue) word |= (uint16_t)bitA;
        break;
      }
      case MDTEST_FAULT_ADDR_LOW:
        word = original(i & ~bitA);
        break;
      case MDTEST_FAULT_ADDR_TIED:
        word = original((i & ~bitB) | ((i & bitA) ? bitB : 0U));
        break;
      case MDTEST_FAULT_SCATTER:
        if (a == 0U) return -1;
        if (i % a == 0U) word ^= (uint16_t)(1U << (i % 16U));
        break;
      case MDTEST_FAULT_ROM3_IS_ROM4:
        word = original(i & (MDTEST_BANK_WORDS - 1U));
        break;
      case MDTEST_FAULT_PIN_DEAD:
        // One RP pin carries both A(a) and D(a): the address the RP sees and
        // the data it drives both lose it. A0 has no address line.
        word = original(a ? (i & ~(1UL << (a - 1U))) : i) & (uint16_t)~bitA;
        break;
      default:
        return -1;
    }
    window[i] = word;
  }
  return 0;
}
