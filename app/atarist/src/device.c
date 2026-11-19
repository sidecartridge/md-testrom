#include "device.h"

#include <string.h>

#define WINDOW ((volatile unsigned short *)MDTEST_ROM4_ADDRESS)

/* Words read twice across both banks: open bus gives one or two values, or
 * values that change between reads; a device serves the same varied words
 * every time. */
#define SAMPLES 64
#define SAMPLE_STEP (MDTEST_WINDOW_WORDS / SAMPLES)
#define MIN_DISTINCT 8

static void text(char *out, const volatile unsigned short *words, int count) {
  for (int i = 0; i < count; i++) {
    unsigned short w = words[i];
    out[2 * i] = (char)(w >> 8);
    out[2 * i + 1] = (char)(w & 0xFF);
  }
  out[2 * count] = '\0';
}

static int printable(const char *s) {
  for (; *s; s++) {
    if (*s < 32 || *s > 126) return 0;
  }
  return 1;
}

/* Sampled words that are the pattern's own: a faulty bus still delivers
 * many of them (all those its fault does not touch). */
#define MIN_PATTERN 8

static int patternWords(void) {
  int matches = 0;
  for (unsigned int i = 0; i < SAMPLES; i++) {
    unsigned int index = i * SAMPLE_STEP + 7;
    if (WINDOW[index] == mdtest_patternWord((uint16_t)index)) matches++;
  }
  return matches;
}

static int servesData(void) {
  unsigned short first[SAMPLES];
  int distinct = 0;
  for (int i = 0; i < SAMPLES; i++) first[i] = WINDOW[i * SAMPLE_STEP + 1];
  for (int i = 0; i < SAMPLES; i++) {
    if (WINDOW[i * SAMPLE_STEP + 1] != first[i]) return 0;
    int seen = 0;
    for (int j = 0; j < i; j++) {
      if (first[j] == first[i]) seen = 1;
    }
    if (!seen) distinct++;
  }
  return distinct >= MIN_DISTINCT;
}

void device_recognise(Device *d) {
  const volatile unsigned short *h = &WINDOW[MDTEST_HEADER_INDEX];
  unsigned short sum = 0;
  char id[9];

  memset(d, 0, sizeof(*d));
  for (unsigned int i = 0; i < MDTEST_HEADER_WORDS; i++) sum += h[i];
  text(id, &h[MDTEST_HDR_ID], 4);
  if (sum == 0 && strcmp(id, MDTEST_ID) == 0) {
    d->kind = DEVICE_TEST;
    d->layout = h[MDTEST_HDR_LAYOUT];
    d->state = h[MDTEST_HDR_STATE];
    text(d->version, &h[MDTEST_HDR_VERSION], MDTEST_HDR_VERSION_WORDS);
    text(d->build, &h[MDTEST_HDR_BUILD], MDTEST_HDR_BUILD_WORDS);
    return;
  }
  if (patternWords() >= MIN_PATTERN) {
    d->kind = DEVICE_TEST_DAMAGED;
    return;
  }
  if (!servesData()) {
    d->kind = DEVICE_NONE;
    return;
  }
  /* v0.0.3 serves its version string in bytes 4 to 14 ("0.0.3"). */
  text(d->version, &WINDOW[2], 3);
  d->version[5] = '\0';
  if (d->version[0] >= '0' && d->version[0] <= '9' && d->version[1] == '.' &&
      printable(d->version)) {
    d->kind = DEVICE_OLD;
    return;
  }
  d->version[0] = '\0';
  d->kind = DEVICE_OTHER;
}
