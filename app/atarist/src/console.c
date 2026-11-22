#include "console.h"

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include "hw.h"

#define LINE_CHARS 160
/* The screen: a title line, a device line, the body, a bottom line. */
#define TITLE_ROW 0
#define DEVICE_ROW 1
#define BODY_TOP 3
#define BODY_BOTTOM 23
#define STATUS_ROW 24
#define SCREEN_WIDTH 81 /* console_wrap() keeps one column free */
/* The report: every line, end to end in one pool (most lines are short); a
 * guided run with the menu is about 200 lines. */
#define REPORT_LINES 1500
#define REPORT_BYTES 49152U

static char reportPool[REPORT_BYTES];
static unsigned short reportAt[REPORT_LINES];
static unsigned short reportUsed;
static unsigned char privateStart[REPORT_LINES];
static unsigned char privateLength[REPORT_LINES];
static int reportCount;
static unsigned reportDropped; /* lines that did not fit */
static long nfStderr;
static char machineText[HW_COLUMNS];
static void record(const char *text);
static int bodyTop = BODY_TOP;
static int cursor = BODY_TOP; /* the body's next row */

/*
 * NatFeats (Hatari's host calls). The opcodes $7300 and $7301 are illegal
 * instructions on a real 68000: detection catches the trap through the
 * illegal-instruction vector and answers 0. Supervisor mode.
 */
long nf_detect(const char *name) __asm__("mdt_nfdetect");
long nf_call(long id, const char *text) __asm__("mdt_nfcall");

__asm__(
    "    .text\n"
    "    .even\n"
    "    .globl mdt_nfdetect\n"
    "mdt_nfdetect:\n"
    "    movem.l %a2-%a3,-(%sp)\n"
    "    move.l  12(%sp),%a2\n"   /* the feature's name */
    "    move.l  0x10.w,%a3\n"    /* the illegal-instruction vector */
    "    move.l  %sp,%a1\n"
    "    move.l  #mdt_nffail,0x10.w\n"
    "    moveq   #0,%d0\n"
    "    move.l  %a2,-(%sp)\n"
    "    subq.l  #4,%sp\n"
    "    .dc.w   0x7300\n"         /* NATFEAT_ID: the feature's ID in d0 */
    "mdt_nffail:\n"
    "    move.l  %a1,%sp\n"
    "    move.l  %a3,0x10.w\n"
    "    movem.l (%sp)+,%a2-%a3\n"
    "    rts\n"
    "    .globl mdt_nfcall\n"
    "mdt_nfcall:\n"
    "    .dc.w   0x7301\n"         /* NATFEAT_CALL */
    "    rts\n");

/* --- The screen ----------------------------------------------------------- */

/* A word on its own: not part of a longer one. */
static int wordAt(const char *line, int at, const char *word) {
  int n = (int)strlen(word);
  if (strncmp(line + at, word, (size_t)n) != 0) return 0;
  if (at > 0 && line[at - 1] != ' ') return 0;
  char after = line[at + n];
  return after == '\0' || after == ' ' || after == '.' || after == ':' ||
         after == ',';
}

/* A row of text, the verdict words in their colours: PASS green, FAIL white
 * on red, WARN black on white. The rest of the row cleared. */
static void drawText(int col, int row, const char *text, int fg, int bg) {
  int n = (int)strlen(text);
  for (int i = 0; i < n && col + i < HW_COLUMNS;) {
    int len = 0, wfg = fg, wbg = bg;
    if (bg == HW_BLACK) {
      if (wordAt(text, i, "PASS")) {
        len = 4, wfg = HW_GREEN;
      } else if (wordAt(text, i, "FAIL")) {
        len = 4, wfg = HW_WHITE, wbg = HW_RED;
      } else if (wordAt(text, i, "WARN")) {
        len = 4, wfg = HW_BLACK, wbg = HW_WHITE;
      }
    }
    if (len == 0) len = 1, wfg = fg, wbg = bg;
    for (int k = 0; k < len && col + i + k < HW_COLUMNS; k++) {
      hw_putChar(col + i + k, row, text[i + k], wfg, wbg);
    }
    i += len;
  }
}

static void drawRow(int row, const char *text, int fg, int bg) {
  drawText(0, row, text, fg, bg);
  for (int c = (int)strlen(text); c < HW_COLUMNS; c++) {
    hw_putChar(c, row, ' ', fg, bg);
  }
}

/* A bar: reversed, a text on the left and one on the right. */
static void drawBar(int row, const char *left, const char *right) {
  char line[HW_COLUMNS + 1];
  memset(line, ' ', HW_COLUMNS);
  line[HW_COLUMNS] = '\0';
  int n = (int)strlen(left);
  if (n > HW_COLUMNS - 2) n = HW_COLUMNS - 2;
  memcpy(line + 1, left, (size_t)n);
  if (right && *right) {
    int r = (int)strlen(right);
    if (r > HW_COLUMNS - 2 - n - 2) r = HW_COLUMNS - 2 - n - 2;
    if (r > 0) memcpy(line + HW_COLUMNS - 1 - r, right, (size_t)r);
  }
  drawRow(row, line, HW_BLACK, HW_WHITE);
}

static void putScreen(const char *line, void *ctx) {
  (void)ctx;
  if (cursor > BODY_BOTTOM) {
    hw_scrollUp(bodyTop, BODY_BOTTOM);
    cursor = BODY_BOTTOM;
  }
  drawRow(cursor++, line, HW_WHITE, HW_BLACK);
}

void console_init(void) {
  nfStderr = nf_detect("NF_STDERR");
  hw_clearRows(0, HW_ROWS - 1, HW_BLACK);
  bodyTop = cursor = BODY_TOP;
}

void console_clear(void) {
  bodyTop = cursor = BODY_TOP;
  hw_clearRows(bodyTop, BODY_BOTTOM, HW_BLACK);
}

void console_title(const char *left, const char *right) {
  drawBar(TITLE_ROW, left, right ? right : machineText);
}

void console_setMachine(const char *text) {
  snprintf(machineText, sizeof(machineText), "%s", text);
}

void console_screen(const char *title, const char *right) {
  console_clear();
  console_title(title, right);
}

void console_page(const char *title) {
  console_screen(title, 0);
  record("");
  record(title);
}

void console_device(const char *text) {
  char line[HW_COLUMNS + 1];
  snprintf(line, sizeof(line), " %s", text);
  drawRow(DEVICE_ROW, line, HW_WHITE, HW_BLACK);
}

void console_status(const char *left, const char *right) {
  drawBar(STATUS_ROW, left, right);
}

void console_bodyFrom(int row) {
  if (row < BODY_TOP) row = BODY_TOP;
  if (row > BODY_BOTTOM) row = BODY_BOTTOM;
  bodyTop = cursor = row;
}

int console_bodyRow(void) { return cursor; }

void console_at(int col, int row, const char *text) {
  drawText(col, row, text, HW_WHITE, HW_BLACK);
}

/* Where a wrapped line continues: under a verdict's text, or under the
 * line's own indentation. */
static int hangingIndent(const char *text) {
  if (!strncmp(text, "PASS  ", 6) || !strncmp(text, "FAIL  ", 6) ||
      !strncmp(text, "WARN  ", 6) || !strncmp(text, "SKIP  ", 6)) {
    return 6;
  }
  int n = 0;
  while (text[n] == ' ') n++;
  return n;
}

void console_wrap(const char *text, int width,
                  void (*put)(const char *line, void *ctx), void *ctx) {
  int indent = hangingIndent(text);
  int first = 1;
  char out[LINE_CHARS + 2];
  if (width > LINE_CHARS) width = LINE_CHARS;
  while (*text || first) {
    int pad = first ? 0 : indent;
    int room = width - 1 - pad;
    if (room < 10) {
      pad = 0;
      room = width - 1;
    }
    int len = (int)strlen(text);
    int cut = len;
    if (len > room) {
      cut = room;
      while (cut > 0 && text[cut] != ' ') cut--;
      if (cut == 0) cut = room;
    }
    memset(out, ' ', (size_t)pad);
    memcpy(out + pad, text, (size_t)cut);
    out[pad + cut] = '\0';
    put(out, ctx);
    text += cut;
    while (*text == ' ') text++;
    first = 0;
  }
}

static void (*mirrorLine)(const char *line);

void console_setMirror(void (*mirror)(const char *line)) {
  mirrorLine = mirror;
}

/* A line for the report, the host's log and the RP's mirror. */
static void record(const char *text) {
  size_t n = strlen(text);
  if (n > LINE_CHARS - 1) n = LINE_CHARS - 1;
  if (reportCount < REPORT_LINES && reportUsed + n + 1 <= REPORT_BYTES) {
    reportAt[reportCount] = reportUsed;
    memcpy(reportPool + reportUsed, text, n);
    reportPool[reportUsed + n] = '\0';
    reportUsed = (unsigned short)(reportUsed + n + 1);
    reportCount++;
  } else {
    reportDropped++;
  }
  if (nfStderr) {
    nf_call(nfStderr, "MDTEST: ");
    nf_call(nfStderr, text);
    nf_call(nfStderr, "\n");
  }
  if (mirrorLine) mirrorLine(text);
}

static void emit(const char *text) {
  console_wrap(text, SCREEN_WIDTH, putScreen, 0);
  record(text);
}

void console_record(const char *fmt, ...) {
  char text[LINE_CHARS];
  va_list args;
  va_start(args, fmt);
  vsnprintf(text, sizeof(text), fmt, args);
  va_end(args);
  record(text);
}

void console_line(const char *fmt, ...) {
  char text[LINE_CHARS];
  va_list args;
  va_start(args, fmt);
  vsnprintf(text, sizeof(text), fmt, args);
  va_end(args);
  emit(text);
}

void console_privateLine(const char *name, const char *fmt, ...) {
  char text[LINE_CHARS];
  va_list args;
  va_start(args, fmt);
  vsnprintf(text, sizeof(text), fmt, args);
  va_end(args);
  int line = reportCount;
  emit(text);
  const char *at = name[0] ? strstr(text, name) : 0;
  if (at && line < reportCount) { /* the line was kept */
    privateStart[line] = (unsigned char)(at - text);
    privateLength[line] = (unsigned char)strlen(name);
  }
}

void console_blank(void) { emit(""); }

/* Keys from the program's own keyboard handler (hw.c): every key typed is
 * kept, whatever the program was doing. */
int console_key(void) {
  int ch;
  while ((ch = hw_key()) == 0) {
  }
  return ch;
}

int console_keyPressed(void) { return hw_key() != 0; }

int console_reportLines(void) { return reportCount; }

const char *console_reportLine(int index) {
  return reportPool + reportAt[index];
}

unsigned console_reportDropped(void) { return reportDropped; }

int console_reportPrivate(int index, int *start) {
  *start = privateStart[index];
  return privateLength[index];
}
