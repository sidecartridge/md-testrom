/*
 * The guided run's summary: one line per area, then what the results most
 * likely mean and what to do next. Plain C over a callback, so the host
 * tests run every combination it knows.
 */
#ifndef SUMMARY_H
#define SUMMARY_H

/* The areas, in the order the guided run tests them. */
typedef enum {
  AREA_LINES,
  AREA_COMMANDS,
  AREA_DEVICE,
  AREA_CARD,
  AREA_WIFI,
  AREA_BLUETOOTH,
  AREA_SELECT,
  AREA_LED,
  AREA_CARD_SWAP,
  AREA_COUNT
} Area;

typedef enum {
  OUT_NOT_RUN, /* not reached, or not asked for */
  OUT_PASS,
  OUT_WARN,
  OUT_FAIL,
  OUT_SKIP,    /* an earlier finding makes it meaningless */
  OUT_LOCKED,  /* behind the gate: the line test or the commands failed */
  OUT_STOPPED  /* stopped by the user */
} Outcome;

/* Why an area did not pass: what the conclusions are drawn from. */
typedef enum {
  R_NONE,
  R_LOST,            /* the device stopped answering during the test */
  R_LINES,           /* the line test found a fault (its finding above) */
  R_CMD_CACHE,       /* commands fail on a Mega STE at 16 MHz, cache on */
  R_CMD,             /* reads work, commands do not */
  R_DEV_CRASH,       /* the firmware restarted by itself */
  R_DEV_TEMP,        /* the chip's temperature out of range */
  R_DEV_SETTINGS,    /* Booster's settings unreadable */
  R_CARD_NONE,       /* no card answers */
  R_CARD_START,      /* a card answers, then fails to start */
  R_CARD_OLD,        /* a standard-capacity card */
  R_CARD_FORMAT,     /* no filesystem the device reads, or unreadable */
  R_CARD_FULL,
  R_CARD_LOCKED,
  R_CARD_WRITE,      /* writing or reading back failed, or data differs */
  R_CARD_SLOW,
  R_CHIP_SILENT,     /* the radio chip does not answer */
  R_WIFI_NONE,       /* no network set up in Booster */
  R_WIFI_NOT_FOUND,  /* Booster's network not heard */
  R_WIFI_JOIN,       /* refused, wrong password, no address */
  R_WIFI_NET,        /* joined, but the router, names or the web fail */
  R_WIFI_WEAK,
  R_BT_START,        /* the controller does not start */
  R_BT_SCAN,
  R_BT_NOTHING,      /* nothing heard: a warning */
  R_SEL_NONE,        /* no press seen */
  R_SEL_STUCK,       /* pressed from the start */
  R_LED_UNSEEN,      /* the chip took the commands, the LED did not follow them */
  R_SWAP             /* the card was not seen leaving or coming back */
} Reason;

typedef struct {
  Outcome outcome[AREA_COUNT];
  Reason reason[AREA_COUNT];
} Results;

/* Every area not run, no reason. */
void summary_clear(Results *r);

/* The summary's lines, one call each. */
void summary_write(const Results *r, void (*put)(const char *line, void *ctx),
                   void *ctx);

#endif
