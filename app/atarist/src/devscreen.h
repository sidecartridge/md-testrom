/*
 * The device's own tests on the screen: what it is and how it has been
 * running, the SELECT button, the green LED, and leaving to Booster. The
 * channel is open (chscreen_open) for all but the exit, which opens it.
 */
#ifndef DEVSCREEN_H
#define DEVSCREEN_H

void devscreen_info(void);
void devscreen_select(void);
void devscreen_led(void);

/* Ask the device to leave for Booster: 1 when it is on its way (the
 * cartridge port is then Booster's, and the program must not read it). */
int devscreen_exit(long mch);

#endif
