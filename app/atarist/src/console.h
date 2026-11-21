/*
 * The program's output: every line goes to the screen, into the report, and
 * to the emulator's console when the program runs in Hatari with NatFeats on
 * (nothing on a real machine), which is how the EmuMD harness reads the
 * screen.
 */
#ifndef CONSOLE_H
#define CONSOLE_H

/* After hw_takeOver(): the screen cleared, NatFeats detected. Once. */
void console_init(void);

/* Clear the body (the report keeps every line). */
void console_clear(void);

/* The fixed lines: the title bar, the device line under it, the bottom bar.
 * Screen only: the report and the host's log do not get them. */
void console_title(const char *left, const char *right);
void console_device(const char *text);
void console_status(const char *left, const char *right);

/* The machine, on the right of every title bar. */
void console_setMachine(const char *text);

/* A new screen: the body cleared, this title in the bar (right: the machine
 * when NULL). Screen only. */
void console_screen(const char *title, const char *right);

/* A new page for a test: console_screen(), and its title in the report and
 * the host's log after a blank line, as every earlier version wrote it. */
void console_page(const char *title);

/* A panel: the body (where lines scroll) starts at this row; text placed at
 * a column and row, screen only, verdict words coloured. */
void console_bodyFrom(int row);
int console_bodyRow(void);
void console_at(int col, int row, const char *text);

/* A line for the report and the host's log, not the screen (a panel draws
 * its own). */
void console_record(const char *fmt, ...);

/* One line, printf-style, on the screen, in the report and to NatFeats. */
void console_line(const char *fmt, ...);

/* A blank line. */
void console_blank(void);

/* Wait for a key and return it, lower case. */
int console_key(void);

/* Whether a key was pressed, the key taken. */
int console_keyPressed(void);

/* Break text into lines of fewer than width characters at spaces, each
 * continuation indented (under a verdict's text, or as the line itself is),
 * and hand each to put. */
void console_wrap(const char *text, int width,
                  void (*put)(const char *line, void *ctx), void *ctx);

/* Every line also to mirror (the command channel's log), or none (NULL). */
void console_setMirror(void (*mirror)(const char *line));

/* A line naming a Wi-Fi network or a Bluetooth device: shown as
 * console_line(), and the name's place in it kept, so a saved report can
 * leave the name out. */
void console_privateLine(const char *name, const char *fmt, ...);

/* The report: every line written since the start. */
int console_reportLines(void);
const char *console_reportLine(int index);

/* Where a report line names a network: its length, 0 for none. */
int console_reportPrivate(int index, int *start);

#endif
