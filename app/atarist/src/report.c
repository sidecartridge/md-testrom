#include "report.h"

#include <mint/osbind.h>
#include <stdio.h>
#include <string.h>

#include "console.h"
#include "texts.h"

#define MAX_REPORTS 100
#define REPORT_COLUMNS 81 /* lines of 80 characters at most */
#define REPORT_LINE_BYTES 200

typedef struct {
  short handle;
  long error;
} Writer;

static long writeText(short handle, const char *text) {
  long len = (long)strlen(text);
  long done = len ? Fwrite(handle, len, text) : 0;
  if (done < 0) return done;
  if (done != len) return -1;
  done = Fwrite(handle, 2L, "\r\n");
  return done == 2 ? 0 : (done < 0 ? done : -1);
}

static void putFile(const char *line, void *ctx) {
  Writer *w = ctx;
  if (w->error == 0) w->error = writeText(w->handle, line);
}

/* Wi-Fi network names go into a report only if the user agrees. */
static int askKeepNames(void) {
  console_line(T_SAVE_ASK_NAMES);
  for (;;) {
    int key = console_key();
    if (key == 'y' || key == 'n') {
      console_line(key == 'y' ? T_SAVE_NAMES_KEPT : T_SAVE_NAMES_LEFT_OUT);
      return key == 'y';
    }
  }
}

/* A report line, with a network's name replaced unless it is kept. */
static const char *reportLine(int index, int keepNames) {
  static char out[REPORT_LINE_BYTES];
  const char *line = console_reportLine(index);
  int start;
  int length = console_reportPrivate(index, &start);
  if (keepNames || length == 0) return line;
  snprintf(out, sizeof(out), "%.*s%s%s", start, line, T_NETWORK_LEFT_OUT,
           line + start + length);
  return out;
}

void report_save(const char *gate) {
  char name[16];
  int number;
  /* Fsfirst writes into the DTA; a buffer of the program's own. */
  static char dta[44];
  void *oldDta = Fgetdta();
  Fsetdta(dta);
  for (number = 0; number < MAX_REPORTS; number++) {
    sprintf(name, "REPORT%02d.TXT", number);
    if (Fsfirst(name, 0) < 0) break;
  }
  Fsetdta(oldDta);
  if (number == MAX_REPORTS) {
    console_line(T_SAVE_FAILED);
    console_line(T_SAVE_NONAME);
    return;
  }

  /* The lines to write, counted now: the outcome is printed after. */
  int lines = console_reportLines();
  int keepNames = 1;
  for (int i = 0; i < lines; i++) {
    int start;
    if (console_reportPrivate(i, &start)) {
      keepNames = askKeepNames();
      break;
    }
  }
  long handle = Fcreate(name, 0);
  long error = handle;
  if (handle >= 0) {
    Writer w = {(short)handle, 0};
    putFile(T_REPORT_FORMAT, &w);
    putFile(gate, &w);
    for (int i = 0; i < lines && w.error == 0; i++) {
      console_wrap(reportLine(i, keepNames), REPORT_COLUMNS, putFile, &w);
    }
    long closed = Fclose((short)handle);
    error = w.error ? w.error : closed;
  }
  if (error < 0) {
    console_line(T_SAVE_FAILED);
    console_line(T_SAVE_FAILED_DETAIL, name, error);
    console_line(T_SAVE_ADVICE);
    return;
  }
  console_line(T_SAVED, name);
}
