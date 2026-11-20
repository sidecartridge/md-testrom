/**
 * File: cardtest.c
 * Author: Diego Parrilla Santamaría
 * Date: November 2026
 * Copyright: 2026 - GOODDATA LABS SL
 * Description: The microSD card's tests (cardtest.h). The card is the
 *              user's: one temporary file is written and removed, nothing
 *              else on it is written, and it is never formatted or repaired.
 */

#include "cardtest.h"

#include <stdio.h>
#include <string.h>

#include "cardhw.h"
#include "debug.h"
#include "ff.h"
#include "gconfig.h"
#include "jobs.h"
#include "pico/stdlib.h"

#define DRIVE "0:"
#define CHUNK_BYTES 2048U
#define CHUNKS_PER_STEP 4U  // 8 KB a step: a few ms on a working card
#define BYTES_PER_KB 1024U
#define BYTES_PER_MB (1024U * 1024U)
#define SECTOR_BYTES 512U
#define US_PER_MS 1000U
#define MS_PER_SECOND 1000U
#define PROGRESS_DONE 100U
// SDXC starts above 32 GB.
#define SDHC_MAX_SECTORS (32U * 1024U * 1024U * 2U)
// The swap test asks the card whether it is there this often.
#define SWAP_POLL_MS 500U
// Debug builds log a step slower than this (the ST waits 250 ms an answer).
#define SLOW_STEP_US 20000U
// A volume label: 11 characters on FAT, up to 11 two-byte ones on exFAT.
#define LABEL_BYTES 34U

#if (defined(_DEBUG) && (_DEBUG != 0)) || (defined(EMUMD) && EMUMD)
volatile uint32_t cardtestFault = CARDTEST_FAULT_NONE;
#define FAULT cardtestFault
#else
#define FAULT CARDTEST_FAULT_NONE
#endif

typedef enum {
  STEP_START,
  STEP_MOUNT,
  STEP_OPEN_WRITE,
  STEP_WRITE,
  STEP_OPEN_READ,
  STEP_READ,
  STEP_CLEAN,
  STEP_SWAP_FIRST,
  STEP_SWAP_OUT,
  STEP_SWAP_IN,
  STEP_SWAP_MOUNT,
} Step;

static uint16_t test;
static Step step;
static uint16_t endState;  // what the job ends in once cleaned up
static FATFS cardFs;
static FIL file;
static bool mounted;
static bool fileOpen;
static bool fileMade;
static uint32_t offset;  // bytes written or read back
static uint32_t writeUs;
static uint32_t readUs;
static uint32_t waitMs;      // the swap test's wait, each way
static uint32_t phaseStart;  // when its phase began
static uint32_t lastAsk;     // when it last asked the card
static uint8_t chunk[CHUNK_BYTES];

static uint32_t elapsedUs(uint32_t since) { return time_us_32() - since; }

// The file's contents: the window's pattern, word by word, with the upper
// half of the word index folded in so the second 128 KB differs from the
// first. Big-endian, as the ST reads the window.
// Byte and register layouts, the window's and the card's:
// NOLINTBEGIN(readability-magic-numbers)
static uint8_t fileByte(uint32_t pos) {
  uint32_t index = pos / 2;
  uint16_t word = (uint16_t)(mdtest_patternWord((uint16_t)index) ^
                             (uint16_t)(index >> 16));
  return (pos & 1U) ? (uint8_t)word : (uint8_t)(word >> 8);
}

static void fillChunk(uint32_t start) {
  for (uint32_t i = 0; i < CHUNK_BYTES; i++) chunk[i] = fileByte(start + i);
}

static const char *fatName(BYTE type) {
  switch (type) {
    case FS_FAT12:
      return "FAT12";
    case FS_FAT16:
      return "FAT16";
    case FS_FAT32:
      return "FAT32";
    case FS_EXFAT:
      return "exFAT";
    default:
      return "unknown";
  }
}

// The CID's printable characters (OEM, product name): others as '?'.
static void cidText(const uint8_t *from, unsigned count, char *out) {
  for (unsigned i = 0; i < count; i++) {
    out[i] = (from[i] >= ' ' && from[i] <= '~') ? (char)from[i] : '?';
  }
  out[count] = '\0';
}

static void cardLines(const CardHwInfo *info, uint32_t startMs) {
  const uint8_t *cid = info->cid;
  char oem[3];
  char product[6];
  cidText(&cid[1], 2, oem);
  cidText(&cid[3], 5, product);
  jobs_resultLine("card_type=%s", info->sectors > SDHC_MAX_SECTORS ? "SDXC"
                                                                   : "SDHC");
  jobs_resultLine("card_mb=%lu",
                  (unsigned long)(info->sectors / (BYTES_PER_MB / SECTOR_BYTES)));
  jobs_resultLine("maker=%02X", cid[0]);
  if (strchr(oem, '?') != NULL) {  // not text: the two bytes as they are
    jobs_resultLine("oem=%02X%02X", cid[1], cid[2]);
  } else {
    jobs_resultLine("oem=%s", oem);
  }
  jobs_resultLine("product=%s", product);
  jobs_resultLine("revision=%u.%u", cid[8] >> 4, cid[8] & 0x0FU);
  jobs_resultLine("serial=%02X%02X%02X%02X", cid[9], cid[10], cid[11],
                  cid[12]);
  // Manufactured: a year from 2000 and a month, 12 bits across bytes 13-14.
  unsigned made = ((cid[13] & 0x0FU) << 8) | cid[14];
  jobs_resultLine("made=%u-%02u", 2000U + (made >> 4), made & 0x0FU);
  jobs_resultLine("spi_khz=%lu", (unsigned long)info->spiKhz);
  jobs_resultLine("start_ms=%lu", (unsigned long)startMs);
}

// NOLINTEND(readability-magic-numbers)

// Start the card: false, with the reason in the result, when it does not.
static bool startCard(bool describe) {
  CardHwInfo info;
  uint32_t t0 = time_us_32();
  int answer = FAULT == CARDTEST_FAULT_SILENT ? CARDHW_NO_ANSWER
                                              : cardhw_start(&info);
  uint32_t startMs = elapsedUs(t0) / US_PER_MS;
  switch (answer) {
    case CARDHW_OK:
      jobs_resultLine("card=ok");
      if (describe) cardLines(&info, startMs);
      return true;
    case CARDHW_NO_ANSWER:
      jobs_resultLine("card=no answer");
      return false;
    case CARDHW_OLD_CARD:
      jobs_resultLine("card=standard capacity");
      return false;
    default:
      jobs_resultLine("card=failed");
      return false;
  }
}

// Mount and describe the filesystem: false, with the reason, when it fails.
static bool mountCard(bool describe, uint32_t *freeKb) {
  FRESULT fr = f_mount(&cardFs, DRIVE, 1);
  if (fr != FR_OK) {
    jobs_resultLine("filesystem=%s",
                    fr == FR_NO_FILESYSTEM ? "none" : "unreadable");
    jobs_resultLine("mount_error=%d", (int)fr);
    f_mount(NULL, DRIVE, 0);
    return false;
  }
  mounted = true;
  DWORD freeClusters = 0;
  FATFS *volume = NULL;
  fr = f_getfree(DRIVE, &freeClusters, &volume);
  if (fr != FR_OK || volume == NULL) {
    jobs_resultLine("filesystem=unreadable");
    jobs_resultLine("mount_error=%d", (int)fr);
    return false;
  }
  uint64_t clusterBytes = (uint64_t)volume->csize * SECTOR_BYTES;
  uint64_t freeBytes = (uint64_t)freeClusters * clusterBytes;
  if (FAULT == CARDTEST_FAULT_FULL) freeBytes = 0;
  *freeKb = (uint32_t)(freeBytes / BYTES_PER_KB);
  jobs_resultLine("filesystem=%s", fatName(volume->fs_type));
  if (!describe) return true;
  jobs_resultLine("size_mb=%lu",
                  (unsigned long)((uint64_t)(volume->n_fatent - 2U) *
                                  clusterBytes / BYTES_PER_MB));
  jobs_resultLine("free_mb=%lu", (unsigned long)(freeBytes / BYTES_PER_MB));
  char label[LABEL_BYTES] = "";
  if (f_getlabel(DRIVE, label, NULL) == FR_OK) {
    jobs_resultLine("label=%s", label);
  }
  // Booster's apps folder: information, not a failure.
  SettingsConfigEntry *apps =
      settings_find_entry(gconfig_getContext(), PARAM_APPS_FOLDER);
  if (apps != NULL && apps->value[0] != '\0') {
    FILINFO info;
    bool found = f_stat(apps->value, &info) == FR_OK &&
                 (info.fattrib & AM_DIR) != 0;
    jobs_resultLine("apps_folder=%s", found ? "found" : "missing");
  }
  return true;
}

static void stopCard(void) {
  if (fileOpen) f_close(&file);
  fileOpen = false;
  if (mounted) f_mount(NULL, DRIVE, 0);
  mounted = false;
  cardhw_stop();
}

// The file's write or read failed: why, in the result.
static const char *writeError(FRESULT fr) {
  switch (fr) {
    case FR_WRITE_PROTECTED:
      return "write protected";
    case FR_DENIED:
      return "card full";
    case FR_NOT_READY:
    case FR_DISK_ERR:
      return "card stopped answering";
    default:
      return "write failed";
  }
}

static void fail(const char *what, FRESULT fr) {
  jobs_resultLine("error=%s", what);
  if (fr != FR_OK) jobs_resultLine("fs_error=%d", (int)fr);
  endState = MDTEST_JOB_FAILED;
  step = STEP_CLEAN;
}

static uint32_t kbPerSecond(uint32_t bytes, uint32_t us) {
  if (us == 0) us = 1;
  return (uint32_t)((uint64_t)bytes * (US_PER_MS * MS_PER_SECOND) /
                    BYTES_PER_KB / us);
}

void cardtest_begin(uint16_t which, const uint16_t params[4],
                     uint32_t nowMs) {
  test = which;
  mounted = false;
  fileOpen = false;
  fileMade = false;
  offset = 0;
  writeUs = 0;
  readUs = 0;
  endState = MDTEST_JOB_DONE;
  if (which == MDTEST_TEST_SD_SWAP) {
    uint32_t seconds = params[0] ? params[0] : MDTEST_SD_SWAP_SECONDS;
    if (seconds > MDTEST_SD_SWAP_MAX_SECONDS) {
      seconds = MDTEST_SD_SWAP_MAX_SECONDS;
    }
    waitMs = seconds * MS_PER_SECOND;
    phaseStart = nowMs;
    lastAsk = nowMs - SWAP_POLL_MS;
    step = STEP_SWAP_FIRST;
  } else {
    step = STEP_START;
  }
}

static uint16_t cardStep(uint16_t *progress, uint16_t *live) {
  static const uint32_t total = MDTEST_SD_FILE_KB * BYTES_PER_KB;
  uint32_t freeKb = 0;
  FRESULT fr = FR_OK;
  switch (step) {
    case STEP_START:
      jobs_resultLine("test=card");
      live[0] = MDTEST_SD_PHASE_START;
      if (!startCard(true)) {
        endState = MDTEST_JOB_FAILED;
        step = STEP_CLEAN;
        break;
      }
      step = STEP_MOUNT;
      break;
    case STEP_MOUNT:
      live[0] = MDTEST_SD_PHASE_MOUNT;
      if (!mountCard(true, &freeKb)) {
        endState = MDTEST_JOB_FAILED;
        step = STEP_CLEAN;
        break;
      }
      if (freeKb < MDTEST_SD_FILE_KB) {
        fail("card full", FR_OK);
        break;
      }
      step = STEP_OPEN_WRITE;
      break;
    case STEP_OPEN_WRITE: {
      live[0] = MDTEST_SD_PHASE_WRITE;
      jobs_resultLine("file=%s", MDTEST_SD_FILE);
      uint32_t t0 = time_us_32();
      fr = f_open(&file, MDTEST_SD_FILE, FA_CREATE_ALWAYS | FA_WRITE);
      writeUs += elapsedUs(t0);
      if (fr != FR_OK) {
        fail(fr == FR_WRITE_PROTECTED ? "write protected" : "cannot create",
             fr);
        break;
      }
      fileOpen = true;
      fileMade = true;
      step = STEP_WRITE;
      break;
    }
    case STEP_WRITE: {
      uint32_t t0 = time_us_32();
      for (unsigned n = 0; n < CHUNKS_PER_STEP && offset < total; n++) {
        fillChunk(offset);
        UINT written = 0;
        if (FAULT == CARDTEST_FAULT_WRITE && offset >= total / 2) {
          fr = FR_DISK_ERR;
        } else {
          fr = f_write(&file, chunk, CHUNK_BYTES, &written);
        }
        if (fr == FR_OK && written < CHUNK_BYTES) fr = FR_DENIED;  // full
        if (fr != FR_OK) break;
        offset += CHUNK_BYTES;
      }
      if (fr == FR_OK && offset >= total) fr = f_close(&file);
      writeUs += elapsedUs(t0);
      if (fr != FR_OK) {
        jobs_resultLine("written_kb=%lu", (unsigned long)(offset / BYTES_PER_KB));
        fail(writeError(fr), fr);
        break;
      }
      live[1] = (uint16_t)(offset / BYTES_PER_KB);
      *progress = (uint16_t)(offset * (PROGRESS_DONE / 2) / total);
      if (offset >= total) {
        fileOpen = false;
        jobs_resultLine("write_kbs=%lu",
                        (unsigned long)kbPerSecond(total, writeUs));
        offset = 0;
        step = STEP_OPEN_READ;
      }
      break;
    }
    case STEP_OPEN_READ: {
      live[0] = MDTEST_SD_PHASE_READ;
      uint32_t t0 = time_us_32();
      fr = f_open(&file, MDTEST_SD_FILE, FA_READ);
      readUs += elapsedUs(t0);
      if (fr != FR_OK) {
        fail("cannot reopen", fr);
        break;
      }
      fileOpen = true;
      step = STEP_READ;
      break;
    }
    case STEP_READ: {
      for (unsigned n = 0; n < CHUNKS_PER_STEP && offset < total; n++) {
        UINT got = 0;
        uint32_t t0 = time_us_32();
        fr = f_read(&file, chunk, CHUNK_BYTES, &got);
        readUs += elapsedUs(t0);
        if (fr == FR_OK && got < CHUNK_BYTES) fr = FR_INT_ERR;  // short
        if (fr != FR_OK) break;
        if (FAULT == CARDTEST_FAULT_READ && offset == total / 2) chunk[1] ^= 1U;
        for (uint32_t i = 0; i < CHUNK_BYTES; i++) {
          uint8_t expected = fileByte(offset + i);
          if (chunk[i] != expected) {
            jobs_resultLine("mismatch_at=%lu", (unsigned long)(offset + i));
            jobs_resultLine("mismatch_read=%02X", chunk[i]);
            jobs_resultLine("mismatch_expected=%02X", expected);
            fail("data differs", FR_OK);
            return MDTEST_JOB_RUNNING;
          }
        }
        offset += CHUNK_BYTES;
      }
      if (fr != FR_OK) {
        fail(fr == FR_INT_ERR ? "file short" : "read failed", fr);
        break;
      }
      live[1] = (uint16_t)(offset / BYTES_PER_KB);
      *progress = (uint16_t)(PROGRESS_DONE / 2 +
                             offset * (PROGRESS_DONE / 2) / total);
      if (offset >= total) {
        jobs_resultLine("read_kbs=%lu",
                        (unsigned long)kbPerSecond(total, readUs));
        jobs_resultLine("verified_kb=%lu", (unsigned long)MDTEST_SD_FILE_KB);
        step = STEP_CLEAN;
      }
      break;
    }
    case STEP_CLEAN:
    default:
      live[0] = MDTEST_SD_PHASE_CLEAN;
      if (fileOpen) f_close(&file);
      fileOpen = false;
      if (fileMade) {
        fr = f_unlink(MDTEST_SD_FILE);
        if (fr != FR_OK && fr != FR_NO_FILE) {
          jobs_resultLine("file_left=%s", MDTEST_SD_FILE);
          endState = MDTEST_JOB_FAILED;
        }
        fileMade = false;
      }
      stopCard();
      return endState;
  }
  return MDTEST_JOB_RUNNING;
}

static bool cardThere(void) {
  return FAULT != CARDTEST_FAULT_SILENT && cardhw_present();
}

static uint16_t swapStep(uint32_t nowMs, uint16_t *live) {
  uint32_t inPhase = nowMs - phaseStart;
  live[0] = (uint16_t)(step == STEP_SWAP_FIRST  ? MDTEST_SD_SWAP_FIRST
                       : step == STEP_SWAP_OUT ? MDTEST_SD_SWAP_OUT
                       : step == STEP_SWAP_IN  ? MDTEST_SD_SWAP_IN
                                               : MDTEST_SD_SWAP_MOUNT);
  live[1] = (uint16_t)(inPhase >= waitMs
                           ? 0
                           : (waitMs - inPhase + MS_PER_SECOND - 1) /
                                 MS_PER_SECOND);
  if (step == STEP_SWAP_MOUNT) {
    uint32_t freeKb = 0;
    bool up = startCard(false) && mountCard(false, &freeKb);
    stopCard();
    if (!up) {
      jobs_resultLine("error=card back but not usable");
      return MDTEST_JOB_FAILED;
    }
    jobs_resultLine("back=yes");
    return MDTEST_JOB_DONE;
  }
  if (nowMs - lastAsk < SWAP_POLL_MS) return MDTEST_JOB_RUNNING;
  lastAsk = nowMs;
  bool there = cardThere();
  switch (step) {
    case STEP_SWAP_FIRST:
      jobs_resultLine("test=card swap");
      jobs_resultLine("card_at_start=%s", there ? "yes" : "no");
      step = there ? STEP_SWAP_OUT : STEP_SWAP_IN;
      phaseStart = nowMs;
      break;
    case STEP_SWAP_OUT:
      if (!there) {
        jobs_resultLine("out_after_ms=%lu", (unsigned long)inPhase);
        step = STEP_SWAP_IN;
        phaseStart = nowMs;
      } else if (inPhase >= waitMs) {
        jobs_resultLine("error=card not taken out");
        cardhw_stop();
        return MDTEST_JOB_FAILED;
      }
      break;
    case STEP_SWAP_IN:
    default:
      if (there) {
        jobs_resultLine("in_after_ms=%lu", (unsigned long)inPhase);
        step = STEP_SWAP_MOUNT;
      } else if (inPhase >= waitMs) {
        jobs_resultLine("error=card not put back");
        cardhw_stop();
        return MDTEST_JOB_FAILED;
      }
      break;
  }
  return MDTEST_JOB_RUNNING;
}

uint16_t cardtest_step(uint32_t nowMs, uint16_t *progress,
                       uint16_t live[MDTEST_JOB_LIVE_WORDS]) {
#if defined(_DEBUG) && (_DEBUG != 0)
  Step was = step;
  uint32_t began = time_us_32();
#endif
  uint16_t state = test == MDTEST_TEST_SD_SWAP ? swapStep(nowMs, live)
                                               : cardStep(progress, live);
#if defined(_DEBUG) && (_DEBUG != 0)
  uint32_t took = elapsedUs(began);
  if (took > SLOW_STEP_US) {
    DPRINTF("Card: step %d took %lu us\n", (int)was, (unsigned long)took);
  }
#endif
  return state;
}

void cardtest_abort(void) {
  if (fileOpen) f_close(&file);
  fileOpen = false;
  if (fileMade && f_unlink(MDTEST_SD_FILE) != FR_OK) {
    jobs_resultLine("file_left=%s", MDTEST_SD_FILE);
  }
  fileMade = false;
  stopCard();
}
