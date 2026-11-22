#include "hw.h"

#include "font8x8.h"

/* start.s */
extern void hw_timerC(void);
extern void hw_acia(void);
extern void hw_dataCacheOff(void);

#define REG8(a) (*(volatile unsigned char *)(a))
#define REG16(a) (*(volatile unsigned short *)(a))
#define VECTOR(a) (*(void (*volatile *)(void))(a))

/* The MFP (68901). */
#define MFP_GPIP REG8(0xFFFA01)
#define MFP_IERA REG8(0xFFFA07)
#define MFP_IERB REG8(0xFFFA09)
#define MFP_IPRA REG8(0xFFFA0B)
#define MFP_IPRB REG8(0xFFFA0D)
#define MFP_ISRA REG8(0xFFFA0F)
#define MFP_ISRB REG8(0xFFFA11)
#define MFP_IMRA REG8(0xFFFA13)
#define MFP_IMRB REG8(0xFFFA15)
#define MFP_TCDCR REG8(0xFFFA1D)
#define MFP_TCDR REG8(0xFFFA23)
#define MFP_MONITOR_COLOUR 0x80 /* GPIP7: 1 on a colour monitor */
/* A Falcon's monitor is VIDEL's to say: bits 7-6, 0 for a monochrome one,
 * 2 for VGA, 3 for a TV. */
#define VIDEL_MONITOR REG8(0xFF8006)
#define VIDEL_MONITOR_SHIFT 6
#define VIDEL_MONITOR_MONO 0
#define VIDEL_MONITOR_VGA 2
#define VIDEL_MONITOR_TV 3
#define MCH_FALCON 3
#define MFP_B_TIMER_C 0x20
#define MFP_B_ACIA 0x40
#define VECTOR_TIMER_C VECTOR(0x114)
#define VECTOR_ACIA VECTOR(0x118)
/* Timer C: 2.4576 MHz / 64 / 192 = 200 Hz. */
#define TIMER_C_PRESCALE_64 0x50
#define TIMER_C_COUNT 192
/* The TT's second MFP: all of it off. */
#define TTMFP_IERA REG8(0xFFFA87)
#define TTMFP_IERB REG8(0xFFFA89)

/* The keyboard's and the MIDI port's ACIAs (6850). */
#define ACIA_KBD_CONTROL REG8(0xFFFC00)
#define ACIA_KBD_DATA REG8(0xFFFC02)
#define ACIA_MIDI_CONTROL REG8(0xFFFC04)
#define ACIA_MASTER_RESET 0x03
#define ACIA_KBD_IRQ 0x96     /* /64, 8N1, receive interrupt on */
#define ACIA_MIDI_QUIET 0x15  /* /16, 8N1, no interrupt */
#define ACIA_RX_FULL 0x01
#define ACIA_TX_EMPTY 0x02
#define ACIA_BAD_BYTE 0x50 /* framing or parity error */
#define IKBD_MOUSE_OFF 0x12

/* The shifter. */
#define SHIFTER_BASE_HI REG8(0xFF8201)
#define SHIFTER_BASE_MID REG8(0xFF8203)
#define SHIFTER_COUNT_HI REG8(0xFF8205)
#define SHIFTER_COUNT_MID REG8(0xFF8207)
#define SHIFTER_COUNT_LO REG8(0xFF8209)
#define SHIFTER_BASE_LO REG8(0xFF820D)   /* STE, TT, Falcon */
#define SHIFTER_LINE_OFFSET REG8(0xFF820F)
#define SHIFTER_HSCROLL REG8(0xFF8265)
#define SHIFTER_MODE REG8(0xFF8260)
#define SHIFTER_MEDIUM 0x01
#define SHIFTER_HIGH 0x02
#define PALETTE ((volatile unsigned short *)0xFF8240)

#define SCREEN_BYTES 32000UL
#define ROW_BYTES 1280UL  /* 8 lines of 160 bytes, or 16 of 80 */
#define MEDIUM_LINE 160
#define MONO_LINE 80
#define VBL_GUARD 20000UL
#define VBL_STABLE 4

volatile unsigned long hw_ticks;
const void *hw_keytbl;
/* The stack start.s puts the program on (STACK_BYTES there). */
unsigned long hw_stack[16384 / 4];

static unsigned char screenSpace[SCREEN_BYTES + 256];
static volatile unsigned char *screen;
static int mono;

/* --- The keyboard ------------------------------------------------------- */

#define KEY_BUFFER 32 /* a power of two */
#define KEY_RELEASE 0x80
static volatile unsigned char keys[KEY_BUFFER];
static volatile unsigned short keyIn, keyOut;
static unsigned char dropBytes; /* the rest of a mouse or clock packet */

/* The IKBD's packets that are not keys, and how many bytes follow each. */
static int packetLength(unsigned char header) {
  switch (header) {
    case 0xF6:
      return 7; /* status report */
    case 0xF7:
      return 5; /* absolute mouse */
    case 0xF8:
    case 0xF9:
    case 0xFA:
    case 0xFB:
      return 2; /* relative mouse */
    case 0xFC:
      return 6; /* time of day */
    case 0xFD:
      return 2; /* joystick report */
    case 0xFE:
    case 0xFF:
      return 1; /* joystick event */
    default:
      return -1; /* a key */
  }
}

/* The ACIA's interrupt: every byte it has, keys kept, the rest dropped. */
void keys_interrupt(void) {
  while (ACIA_KBD_CONTROL & ACIA_RX_FULL) {
    unsigned char status = ACIA_KBD_CONTROL;
    unsigned char data = ACIA_KBD_DATA;
    if (status & ACIA_BAD_BYTE) continue;
    if (dropBytes) {
      dropBytes--;
      continue;
    }
    int length = packetLength(data);
    if (length >= 0) {
      dropBytes = (unsigned char)length;
      continue;
    }
    if (data & KEY_RELEASE) continue;
    unsigned short next = (unsigned short)((keyIn + 1) & (KEY_BUFFER - 1));
    if (next != keyOut) {
      keys[keyIn] = data;
      keyIn = next;
    }
  }
}

int hw_key(void) {
  /* TOS's KEYTAB: the unshifted table first, 128 characters by scancode. */
  const unsigned char *unshifted = *(const unsigned char *const *)hw_keytbl;
  while (keyOut != keyIn) {
    unsigned char scancode = keys[keyOut];
    keyOut = (unsigned short)((keyOut + 1) & (KEY_BUFFER - 1));
    int c = unshifted[scancode & 0x7F];
    if (c >= 'A' && c <= 'Z') c += 'a' - 'A';
    if (c) return c;
  }
  return 0;
}

static void keyboardSend(unsigned char byte) {
  unsigned long guard = 100000UL;
  while (!(ACIA_KBD_CONTROL & ACIA_TX_EMPTY) && --guard) {
  }
  ACIA_KBD_DATA = byte;
}

/* --- The screen ---------------------------------------------------------- */

static unsigned long videoCounter(void) {
  return ((unsigned long)SHIFTER_COUNT_HI << 16) |
         ((unsigned long)SHIFTER_COUNT_MID << 8) | SHIFTER_COUNT_LO;
}

/* The next vertical blank, with interrupts off: the counter leaves the base
 * (a line displays), then holds at it (the switcher ROM's screen.c). */
static void waitVbl(unsigned long base) {
  unsigned long guard;
  int stable = 0;
  for (guard = 0; guard < VBL_GUARD && videoCounter() == base; guard++) {
  }
  for (guard = 0; guard < VBL_GUARD && stable < VBL_STABLE; guard++) {
    stable = videoCounter() == base ? stable + 1 : 0;
  }
}

/* ST medium on a Falcon's colour monitor: every VIDEL register, as TOS 4's
 * Vsetmode sets them (EmuTOS's tables: modes $99 on VGA, $89 and $A9 on RGB
 * or a TV at 60 and 50 Hz, the rate TOS chose kept). The ST shifter's mode
 * alone keeps the timing of the mode TOS was in. */
typedef struct {
  unsigned short h[6]; /* $FF8282-$FF828C: hold, border, display, sync */
  unsigned short v[6]; /* $FF82A2-$FF82AC */
} VidelTiming;

static const VidelTiming VIDEL_VGA = {
    {0x0017, 0x0012, 0x0001, 0x020E, 0x000D, 0x0011},
    {0x0419, 0x03AF, 0x008F, 0x008F, 0x03AF, 0x0415}};
static const VidelTiming VIDEL_RGB_60 = {
    {0x003E, 0x0032, 0x0009, 0x023F, 0x001C, 0x0034},
    {0x020D, 0x0201, 0x0016, 0x004D, 0x01DD, 0x0207}};
static const VidelTiming VIDEL_RGB_50 = {
    {0x003E, 0x0032, 0x0009, 0x023F, 0x001C, 0x0034},
    {0x0271, 0x0265, 0x002F, 0x006F, 0x01FF, 0x026B}};

#define VIDEL_SYNC REG8(0xFF820A)
#define VIDEL_SYNC_50HZ 0x02
#define VIDEL_OFFSET REG16(0xFF820E)
#define VIDEL_LINE_WIDTH REG16(0xFF8210)
#define VIDEL_H(i) REG16(0xFF8282 + 2 * (i))
#define VIDEL_V(i) REG16(0xFF82A2 + 2 * (i))
#define VIDEL_CO REG16(0xFF82C0)
#define VIDEL_MODE REG16(0xFF82C2)
#define VIDEL_SPSHIFT REG16(0xFF8266)
#define VIDEL_MEDIUM_WIDTH 80 /* words a line */

static void falconMedium(int monitor) {
  const VidelTiming *t = &VIDEL_VGA;
  unsigned short mode = 0x09, co = 0x0186;
  if (monitor != VIDEL_MONITOR_VGA) {
    t = (VIDEL_SYNC & VIDEL_SYNC_50HZ) ? &VIDEL_RGB_50 : &VIDEL_RGB_60;
    mode = 0x04;
    co = monitor == VIDEL_MONITOR_TV ? 0x0083 : 0x0081;
  }
  for (int i = 0; i < 6; i++) {
    VIDEL_H(i) = t->h[i];
    VIDEL_V(i) = t->v[i];
  }
  SHIFTER_MODE = 0;
  VIDEL_OFFSET = 0;
  VIDEL_LINE_WIDTH = VIDEL_MEDIUM_WIDTH;
  VIDEL_MODE = mode;
  VIDEL_CO = co;
  VIDEL_SPSHIFT = 0;
  SHIFTER_MODE = SHIFTER_MEDIUM;
  /* Writing the ST shifter's mode has just set these two its own way. */
  VIDEL_LINE_WIDTH = VIDEL_MEDIUM_WIDTH;
  VIDEL_MODE = mode;
}

static void screenInit(const Machine *m) {
  unsigned long base = ((unsigned long)screenSpace + 255) & ~255UL;
  screen = (volatile unsigned char *)base;
  for (unsigned long i = 0; i < SCREEN_BYTES / 4; i++) {
    ((volatile unsigned long *)screen)[i] = 0;
  }
  SHIFTER_BASE_HI = (unsigned char)(base >> 16);
  SHIFTER_BASE_MID = (unsigned char)(base >> 8);
  /* Every machine after the ST has the low byte and fine scrolling. */
  int hasSteShifter = m->mch != -1 && (m->mch >> 16) >= 1;
  if (hasSteShifter) {
    SHIFTER_BASE_LO = 0;
    SHIFTER_LINE_OFFSET = 0;
    SHIFTER_HSCROLL = 0;
  }
  /* The resolution changes in the vertical blank, after two of them
   * (EmuTOS's way; set mid-frame, a mono screen wrapped on a Mega ST). */
  waitVbl(base);
  waitVbl(base);
  if (m->mch != -1 && (m->mch >> 16) == MCH_FALCON) {
    int monitor = (VIDEL_MONITOR >> VIDEL_MONITOR_SHIFT) & 3;
    mono = monitor == VIDEL_MONITOR_MONO;
    if (mono) {
      SHIFTER_MODE = SHIFTER_HIGH;
    } else {
      falconMedium(monitor);
    }
  } else {
    mono = !(MFP_GPIP & MFP_MONITOR_COLOUR);
    SHIFTER_MODE = mono ? SHIFTER_HIGH : SHIFTER_MEDIUM;
  }
  if (mono) {
    /* Light pixels on dark: an ST's shifter reads bit 0 of colour 0 alone, a
     * Falcon's VIDEL colours 0 and 1. */
    PALETTE[0] = 0x000;
    PALETTE[1] = 0x777;
  } else {
    PALETTE[HW_BLACK] = 0x000;
    PALETTE[HW_WHITE] = 0x777;
    PALETTE[HW_GREEN] = 0x070;
    PALETTE[HW_RED] = 0x700;
  }
}

int hw_mono(void) { return mono; }

void hw_putChar(int col, int row, char c, int fg, int bg) {
  if (col < 0 || col >= HW_COLUMNS || row < 0 || row >= HW_ROWS) return;
  const unsigned char *glyph = font8x8_basic[(unsigned char)c & 0x7F];
  volatile unsigned char *at = screen + (unsigned long)row * ROW_BYTES;
  if (mono) {
    unsigned char ink = fg ? 0xFF : 0x00, paper = bg ? 0xFF : 0x00;
    at += col;
    for (int y = 0; y < 8; y++) {
      unsigned char bits = (unsigned char)((glyph[y] & ink) | (~glyph[y] & paper));
      at[0] = bits;
      at[MONO_LINE] = bits;
      at += 2 * MONO_LINE;
    }
    return;
  }
  /* Medium: two planes, a word each per 16 pixels; a character is one byte
   * of each plane's word. */
  at += (unsigned long)(col >> 1) * 4 + (col & 1);
  unsigned char ink0 = (fg & 1) ? 0xFF : 0, ink1 = (fg & 2) ? 0xFF : 0;
  unsigned char paper0 = (bg & 1) ? 0xFF : 0, paper1 = (bg & 2) ? 0xFF : 0;
  for (int y = 0; y < 8; y++) {
    unsigned char g = glyph[y];
    at[0] = (unsigned char)((g & ink0) | (~g & paper0));
    at[2] = (unsigned char)((g & ink1) | (~g & paper1));
    at += MEDIUM_LINE;
  }
}

static void fillLongs(volatile unsigned long *to, unsigned long longs,
                      unsigned long value) {
  while (longs--) *to++ = value;
}

void hw_clearRows(int top, int bottom, int bg) {
  if (top > bottom) return;
  unsigned long value = 0;
  if (mono) {
    value = bg ? 0xFFFFFFFFUL : 0;
  } else {
    unsigned long p0 = (bg & 1) ? 0xFFFFUL : 0, p1 = (bg & 2) ? 0xFFFFUL : 0;
    value = (p0 << 16) | p1;
  }
  fillLongs((volatile unsigned long *)(screen + (unsigned long)top * ROW_BYTES),
            (unsigned long)(bottom - top + 1) * ROW_BYTES / 4, value);
}

void hw_scrollUp(int top, int bottom) {
  if (top >= bottom) {
    hw_clearRows(top, bottom, HW_BLACK);
    return;
  }
  volatile unsigned long *to =
      (volatile unsigned long *)(screen + (unsigned long)top * ROW_BYTES);
  volatile unsigned long *from = to + ROW_BYTES / 4;
  unsigned long longs = (unsigned long)(bottom - top) * ROW_BYTES / 4;
  while (longs--) *to++ = *from++;
  hw_clearRows(bottom, bottom, HW_BLACK);
}

/* --- The take-over -------------------------------------------------------- */

void hw_takeOver(const Machine *m) {
  __asm__ volatile("move.w #0x2700,%%sr" ::: "cc");

  /* Every MFP interrupt off, the TT's second MFP too. */
  MFP_IERA = 0;
  MFP_IERB = 0;
  MFP_IPRA = 0;
  MFP_IPRB = 0;
  MFP_ISRA = 0;
  MFP_ISRB = 0;
  if (m->mch != -1 && (m->mch >> 16) == 2) {
    TTMFP_IERA = 0;
    TTMFP_IERB = 0;
  }

  /* The 68030's data cache off: a cartridge read must reach the bus. */
  if (m->cpu >= 30) hw_dataCacheOff();

  screenInit(m);

  /* The clock: Timer C at 200 Hz, the program's handler. Timer D (the serial
   * port's baud rate) keeps its setting. */
  VECTOR_TIMER_C = hw_timerC;
  MFP_TCDCR = (unsigned char)((MFP_TCDCR & 0x0F) | TIMER_C_PRESCALE_64);
  MFP_TCDR = TIMER_C_COUNT;

  /* The keyboard: its ACIA interrupting into the program's handler, the
   * MIDI ACIA (same interrupt line) quiet, the mouse off. */
  ACIA_MIDI_CONTROL = ACIA_MASTER_RESET;
  ACIA_MIDI_CONTROL = ACIA_MIDI_QUIET;
  ACIA_KBD_CONTROL = ACIA_MASTER_RESET;
  ACIA_KBD_CONTROL = ACIA_KBD_IRQ;
  keyboardSend(IKBD_MOUSE_OFF);
  while (ACIA_KBD_CONTROL & ACIA_RX_FULL) (void)ACIA_KBD_DATA;
  dropBytes = 0;
  keyIn = keyOut = 0;
  VECTOR_ACIA = hw_acia;

  MFP_IMRB = MFP_B_TIMER_C | MFP_B_ACIA;
  MFP_IERB = MFP_B_TIMER_C | MFP_B_ACIA;
  /* Level 5: the MFP's interrupts (6) reach the CPU, TOS's VBL (4) and HBL
   * (2) do not. */
  __asm__ volatile("move.w #0x2500,%%sr" ::: "cc");
}
