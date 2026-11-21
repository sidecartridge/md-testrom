/*
 * What the program runs on: machine, CPU, TOS, memory and screen.
 */
#ifndef MACHINE_H
#define MACHINE_H

typedef struct {
  long mch;             /* _MCH cookie; -1 without a cookie jar */
  long cpu;             /* _CPU cookie (0, 10, 20, 30, 40, 60) */
  unsigned short tos;   /* TOS version, BCD ($0104) */
  unsigned long tosDate; /* $MMDDYYYY, BCD */
  unsigned long stRam;   /* bytes */
  unsigned long ttRam;   /* bytes */
  int msteFast;          /* Mega STE: 16 MHz */
  int msteCache;         /* Mega STE: cache on */
} Machine;

/* Supervisor mode, before hw_takeOver(): reads what TOS left in RAM. */
void machine_identify(Machine *m);

const char *machine_name(const Machine *m);
/* After hw_takeOver(): the monitor and the mode the program set. */
const char *machine_screen(const Machine *m);
int machine_isMegaSte(const Machine *m);

#endif
