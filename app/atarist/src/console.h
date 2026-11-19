/*
 * The program's output: every line goes to the screen, into the report, and
 * to the emulator's console when the program runs in Hatari with NatFeats on
 * (nothing on a real machine), which is how the EmuMD harness reads the
 * screen.
 */
#ifndef CONSOLE_H
#define CONSOLE_H

/* Detect NatFeats; supervisor mode. Call once before any output. */
void console_init(void);

/* Clear the screen (the report keeps every line). */
void console_clear(void);

/* One line, printf-style, on the screen, in the report and to NatFeats. */
void console_line(const char *fmt, ...);

/* A blank line. */
void console_blank(void);

/* Wait for a key and return it, lower case. */
int console_key(void);

/* Break text into lines of fewer than width characters at spaces, each
 * continuation indented (under a verdict's text, or as the line itself is),
 * and hand each to put. */
void console_wrap(const char *text, int width,
                  void (*put)(const char *line, void *ctx), void *ctx);

/* Every line also to mirror (the command channel's log), or none (NULL). */
void console_setMirror(void (*mirror)(const char *line));

/* The report: every line written since the start. */
int console_reportLines(void);
const char *console_reportLine(int index);

#endif
