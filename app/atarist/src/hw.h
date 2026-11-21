/*
 * The machine without TOS: the screen, the keyboard and the clock, driven
 * directly (the switcher ROM's way), and the restart that ends the program.
 *
 * hw_takeOver() runs once, after machine_identify(): from then on nothing
 * calls TOS. Interrupts stay at level 5, so only the MFP reaches the CPU, and
 * of the MFP only Timer C (the 200 Hz clock) and the keyboard's ACIA, both
 * with handlers of the program's own (start.s).
 */
#ifndef HW_H
#define HW_H

#include "machine.h"

#define HW_COLUMNS 80
#define HW_ROWS 25

/* Colours: the four of medium resolution. Mono shows 0 dark, the rest
 * light. */
#define HW_BLACK 0
#define HW_WHITE 1
#define HW_GREEN 2
#define HW_RED 3

/* The 200 Hz clock (Timer C), counting from the take-over. */
extern volatile unsigned long hw_ticks;

/* The keyboard's layout, from TOS before the take-over (start.s). */
extern const void *hw_keytbl;

void hw_takeOver(const Machine *m);

/* 1 on a monochrome monitor (high resolution), 0 on colour (medium). */
int hw_mono(void);

/* One character at a column and row, in a colour on a background. */
void hw_putChar(int col, int row, char c, int fg, int bg);

/* Rows from top to bottom (inclusive) one row up; the bottom row cleared. */
void hw_scrollUp(int top, int bottom);

/* Rows from top to bottom (inclusive) cleared to a background. */
void hw_clearRows(int top, int bottom, int bg);

/* A key pressed: its character (lower case for letters), 0 if none. */
int hw_key(void);

/* Restart the computer: TOS starts again from its ROM. Never returns. */
void hw_restart(void) __attribute__((noreturn));

#endif
