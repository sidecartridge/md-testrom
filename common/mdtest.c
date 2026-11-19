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
