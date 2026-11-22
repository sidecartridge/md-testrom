#include "machine.h"

#include "hw.h"

#define COOKIE_JAR (*(long **)0x5A0)
#define SYSBASE (*(unsigned char **)0x4F2)
#define PHYSTOP (*(unsigned long *)0x42E)
#define RAMTOP (*(unsigned long *)0x5A4)
#define RAMVALID (*(unsigned long *)0x5A8)
#define RAMVALID_MAGIC 0x1357BD13UL
#define TTRAM_START 0x01000000UL
#define MSTE_SPEED_CACHE (*(volatile unsigned char *)0xFF8E21)

#define COOKIE_MCH 0x5F4D4348L /* _MCH */
#define COOKIE_CPU 0x5F435055L /* _CPU */
#define MCH_MEGA_STE 0x00010010L

static int cookie(long id, long *value) {
  long *jar = COOKIE_JAR;
  if (!jar) return 0;
  for (; jar[0]; jar += 2) {
    if (jar[0] == id) {
      *value = jar[1];
      return 1;
    }
  }
  return 0;
}

void machine_identify(Machine *m) {
  long value;
  /* The ROM's header: the RAM copy sysbase points at may be patched. */
  unsigned char *os = *(unsigned char **)(SYSBASE + 8);

  m->mch = cookie(COOKIE_MCH, &value) ? value : -1;
  m->cpu = cookie(COOKIE_CPU, &value) ? value : 0;
  m->tos = *(unsigned short *)(os + 2);
  m->tosDate = *(unsigned long *)(os + 0x18);
  m->stRam = PHYSTOP;
  m->ttRam = (RAMVALID == RAMVALID_MAGIC && RAMTOP > TTRAM_START)
                 ? RAMTOP - TTRAM_START
                 : 0;
  m->msteFast = 0;
  m->msteCache = 0;
  if (machine_isMegaSte(m)) {
    unsigned char reg = MSTE_SPEED_CACHE;
    /* Bit 0 the cache, bit 1 the 16 MHz clock. */
    m->msteCache = reg & 1;
    m->msteFast = (reg >> 1) & 1;
  }
}

int machine_isMegaSte(const Machine *m) { return m->mch == MCH_MEGA_STE; }

const char *machine_name(const Machine *m) {
  if (m->mch == -1) return "ST";
  switch (m->mch >> 16) {
    case 0:
      return "ST";
    case 1:
      if (m->mch == MCH_MEGA_STE) return "Mega STE";
      if ((m->mch & 0xFFFF) == 1) return "ST Book";
      return "STE";
    case 2:
      return "TT";
    case 3:
      return "Falcon";
    default:
      return "unknown machine";
  }
}

const char *machine_screen(const Machine *m) {
  (void)m;
  /* The program's own: high resolution on a mono monitor, medium on
   * colour. */
  return hw_mono() ? "monochrome monitor, 640 x 400"
                   : "colour monitor, 640 x 200, 4 colours";
}
