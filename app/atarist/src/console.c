#include "console.h"

#include <mint/osbind.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#define LINE_CHARS 160
#define REPORT_LINES 200

static char report[REPORT_LINES][LINE_CHARS];
static int reportCount;
static long nfStderr;
static int columns;

/*
 * NatFeats (Hatari's host calls). The opcodes $7300 and $7301 are illegal
 * instructions on a real 68000: detection catches the trap through the
 * illegal-instruction vector and answers 0. Supervisor mode.
 */
long nf_detect(const char *name) __asm__("mdt_nfdetect");
long nf_call(long id, const char *text) __asm__("mdt_nfcall");
/* Line-A's variables (the text screen's size among them), on every TOS. */
char *linea_base(void) __asm__("mdt_lineabase");
#define LINEA_V_CEL_MX (-44) /* last text column */

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
    "    .globl mdt_lineabase\n"
    "mdt_lineabase:\n"
    "    move.l  %a2,-(%sp)\n"
    "    .dc.w   0xa000\n"            /* Line-A init: variables in a0 */
    "    move.l  %a0,%d0\n"
    "    move.l  (%sp)+,%a2\n"
    "    rts\n"
    "    .globl mdt_nfcall\n"
    "mdt_nfcall:\n"
    "    .dc.w   0x7301\n"         /* NATFEAT_CALL */
    "    rts\n");

void console_init(void) {
  nfStderr = nf_detect("NF_STDERR");
  columns = *(short *)(linea_base() + LINEA_V_CEL_MX) + 1;
  if (columns < 40 || columns > 255) columns = Getrez() == 0 ? 40 : 80;
  /* Cursor off; lines are wrapped here, never by the screen. */
  Cconws("\033w\033f");
}

void console_clear(void) { Cconws("\033E"); }

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

static void putScreen(const char *line, void *ctx) {
  (void)ctx;
  Cconws(line);
  Cconws("\r\n");
}

static void emit(const char *text) {
  console_wrap(text, columns, putScreen, 0);
  if (reportCount < REPORT_LINES) {
    strncpy(report[reportCount], text, LINE_CHARS - 1);
    report[reportCount][LINE_CHARS - 1] = '\0';
    reportCount++;
  }
  if (nfStderr) {
    nf_call(nfStderr, "MDTEST: ");
    nf_call(nfStderr, text);
    nf_call(nfStderr, "\n");
  }
}

void console_line(const char *fmt, ...) {
  char text[LINE_CHARS];
  va_list args;
  va_start(args, fmt);
  vsnprintf(text, sizeof(text), fmt, args);
  va_end(args);
  emit(text);
}

void console_blank(void) { emit(""); }

int console_key(void) {
  long key = Bconin(2);
  int ch = (int)(key & 0xFF);
  if (ch >= 'A' && ch <= 'Z') ch += 'a' - 'A';
  return ch;
}

int console_reportLines(void) { return reportCount; }

const char *console_reportLine(int index) { return report[index]; }
