/*
 * MDTEST.TOS's entry and its interrupt handlers.
 *
 * TOS loads the program; this is the only code that calls it: Super(0) for
 * supervisor mode on the program's own stack, and Keytbl() for the keyboard's
 * layout. From main() on the program runs without TOS (hw.c), and it never
 * returns: quitting restarts the computer.
 */

    .text
    .globl  __start
    .globl  _hw_timerC
    .globl  _hw_acia
    .globl  _hw_restart
    .globl  _hw_dataCacheOff
    .extern _main
    .extern _hw_stack
    .extern _hw_keytbl
    .extern _hw_ticks
    .extern _keys_interrupt

/* The size of _hw_stack (hw.c). */
    .equ    STACK_BYTES, 16384

__start:
    lea     _hw_stack+STACK_BYTES, %sp

    /* Super(0): supervisor mode, on this stack. */
    clr.l   -(%sp)
    move.w  #0x20, -(%sp)
    trap    #1
    addq.l  #6, %sp

    /* Keytbl(-1, -1, -1): the tables TOS reads the keyboard with. */
    moveq   #-1, %d0
    move.l  %d0, -(%sp)
    move.l  %d0, -(%sp)
    move.l  %d0, -(%sp)
    move.w  #16, -(%sp)
    trap    #14
    lea     14(%sp), %sp
    move.l  %d0, _hw_keytbl

    jsr     _main
1:  bra.s   1b

/* MFP Timer C, 200 times a second: the program's clock. The MFP is in
   software end-of-interrupt mode: clear Timer C's in-service bit. */
_hw_timerC:
    addq.l  #1, _hw_ticks
    move.b  #0xdf, 0xfffa11
    rte

/* MFP GPIP4: the keyboard's ACIA has a byte. */
_hw_acia:
    movem.l %d0-%d1/%a0-%a1, -(%sp)
    jsr     _keys_interrupt
    movem.l (%sp)+, %d0-%d1/%a0-%a1
    move.b  #0xbf, 0xfffa11
    rte

/* A 68030's data cache off and cleared, so every cartridge read reaches the
   bus. movec is written as words: the assembler builds for the 68000, and
   this runs only on a 68030. */
_hw_dataCacheOff:
    .dc.w   0x4e7a, 0x0002          /* movec %cacr,%d0 */
    andi.l  #0xfffffeff, %d0        /* the data cache off */
    ori.l   #0x00000800, %d0        /* and cleared */
    .dc.w   0x4e7b, 0x0002          /* movec %d0,%cacr */
    rts

/* A cold start, as the switcher ROM does it: interrupts off, the three
   "memory valid" magics cleared so TOS sizes the RAM again, then the ROM's
   reset entry. */
_hw_restart:
    move.w  #0x2700, %sr
    clr.l   0x00000420
    clr.l   0x0000043a
    clr.l   0x0000051a
    move.l  0x00000004, %a0
    jmp     (%a0)
