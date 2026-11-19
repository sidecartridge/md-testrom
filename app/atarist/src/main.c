/*
 * MDTEST.TOS: tests a SidecarTridge Multi-device from the Atari.
 *
 * It identifies the machine, recognises what is in the cartridge port from
 * plain reads of the window (common/mdtest.h), and shows the tests. Text
 * only, over GEMDOS and BIOS: any resolution, any machine, the AUTO folder.
 */
#include <mint/osbind.h>
#include <stdio.h>

#include "console.h"
#include "device.h"
#include "ltscreen.h"
#include "machine.h"
#include "mdtest.h"
#include "report.h"
#include "texts.h"

/* The window layout this program reads; a test build overrides it to show
 * a version mismatch. */
#ifndef PROGRAM_LAYOUT_VERSION
#define PROGRAM_LAYOUT_VERSION MDTEST_LAYOUT_VERSION
#endif

static Machine machine;
static Device device;
/* The device tests run only once the line test has passed (or, on a debug
 * build, the gate was overridden), for this session. */
static int unlocked;
static int overridden;

#define MSTE_SPEED_CACHE (*(volatile unsigned char *)0xFF8E21)
#define BENCH_FILE "MDTEST.BNC"

static void showMachine(void) {
  char cpu[8];
  sprintf(cpu, T_CPU, (int)machine.cpu);
  console_line(T_MACHINE, machine_name(&machine), cpu, machine.tos >> 8,
               machine.tos & 0xFF);
  console_line(T_MACHINE_DETAIL, (unsigned)(machine.tosDate & 0xFFFF),
               (unsigned)(machine.tosDate >> 24),
               (unsigned)((machine.tosDate >> 16) & 0xFF),
               machine.gemdos & 0xFF, machine.gemdos >> 8);
  console_line(T_MACHINE_SCREEN, machine_screen(&machine),
               machine.mch == -1 ? T_NO_COOKIE_JAR : "");
  console_line(T_MACHINE_RAM, machine.stRam / 1024, machine.freeRam / 1024);
  if (machine.ttRam) console_line(T_MACHINE_TTRAM, machine.ttRam / 1024);
  if (machine_isMegaSte(&machine)) {
    console_line(T_MACHINE_MSTE, machine.msteFast ? "16" : "8",
                 machine.msteCache ? "on" : "off");
  }
}

/* Says what is in the port; true when the line test can run. */
static int showDevice(void) {
  switch (device.kind) {
    case DEVICE_NONE:
      console_line(T_DEVICE_NONE);
      console_line(T_FOUND_NONE);
      console_line(T_FOUND_NONE_DETAIL);
      console_line(T_FOUND_NONE_ADVICE);
      console_line(T_FOUND_NONE_LINE);
      return 1;
    case DEVICE_OTHER:
      console_line(T_DEVICE_OTHER);
      console_line(T_FOUND_OTHER);
      console_line(T_FOUND_OTHER_DETAIL);
      console_line(T_FOUND_OTHER_ADVICE);
      return 0;
    case DEVICE_OLD:
      console_line(T_DEVICE_OLD, device.version);
      console_line(T_FOUND_OLD);
      console_line(T_FOUND_OLD_ADVICE, VERSION);
      return 0;
    case DEVICE_TEST_DAMAGED:
      console_line(T_DEVICE_DAMAGED);
      console_line(T_FOUND_DAMAGED);
      console_line(T_FOUND_DAMAGED_DETAIL);
      console_line(T_FOUND_RUN_LINE);
      return 1;
    case DEVICE_TEST:
      break;
  }
  console_line(T_DEVICE_TEST, device.version);
  console_line(T_DEVICE_TEST_DETAIL, device.layout, device.build);
  if (device.layout != PROGRAM_LAYOUT_VERSION) {
    int firmwareOlder = device.layout < PROGRAM_LAYOUT_VERSION;
    console_line(firmwareOlder ? T_FOUND_LAYOUT_OLD : T_FOUND_LAYOUT_NEW);
    console_line(T_FOUND_LAYOUT_DETAIL, device.layout,
                 (unsigned)PROGRAM_LAYOUT_VERSION);
    console_line(firmwareOlder ? T_FOUND_UPDATE_FIRMWARE
                               : T_FOUND_UPDATE_PROGRAM);
    return 0;
  }
  if (device.state != MDTEST_STATE_DATA) {
    console_line(T_FOUND_COMMANDS);
    console_line(T_FOUND_COMMANDS_DETAIL, device.state);
  }
  console_line(T_FOUND_OK);
  return 1;
}

static void showMenu(int lineTestReady) {
  console_blank();
  if (lineTestReady) {
    console_line(T_MENU_LINE);
    console_line(T_MENU_LINE_LONG);
    if (machine_isMegaSte(&machine)) console_line(T_MENU_LINE_MSTE);
  } else {
    console_line(T_MENU_LINE_OFF);
  }
  if (unlocked) {
    console_line(T_MENU_UNLOCKED);
  } else {
    console_line(T_MENU_LOCKED1);
    console_line(T_MENU_LOCKED2);
  }
#ifdef MDTEST_DEBUG
  if (!unlocked) console_line(T_MENU_OVERRIDE);
#endif
  console_line(T_MENU_SAVE);
  console_line(T_MENU_QUIT);
  console_line(T_MENU_PROMPT);
}

static void lineTest(const LtSizes *sizes) {
  if (ltscreen_run(sizes, ltscreen_estimate(sizes)) == LT_PASS) unlocked = 1;
}

/* The quick pass at each Mega STE setting; the user's is put back. */
static void lineTestMegaSte(void) {
  static const unsigned char settings[3] = {0x00, 0x02, 0x03};
  unsigned char saved = MSTE_SPEED_CACHE;
  int passes = 0;
  for (int i = 0; i < 3; i++) {
    MSTE_SPEED_CACHE = (unsigned char)((saved & 0xFC) | settings[i]);
    console_blank();
    console_line(T_LINE_MSTE, settings[i] & 2 ? "16" : "8",
                 settings[i] & 1 ? "on" : "off");
    if (ltscreen_run(&lt_quick, ltscreen_estimate(&lt_quick)) == LT_PASS) {
      passes++;
    }
  }
  MSTE_SPEED_CACHE = saved;
  if (passes == 3) unlocked = 1;
}

/* Bench mode: the line test in a loop, each verdict sent to the RP, until a
 * key. Selected by MDTEST.BNC next to the program. */
static int benchMode(void) {
  static char dta[44];
  void *oldDta = Fgetdta();
  Fsetdta(dta);
  int found = Fsfirst(BENCH_FILE, 0) >= 0;
  Fsetdta(oldDta);
  if (!found) return 0;
  console_line(T_BENCH);
  for (unsigned long run = 1; !Bconstat(2); run++) {
    console_line(T_BENCH_RUN, run);
    ltscreen_run(&lt_quick, ltscreen_estimate(&lt_quick));
  }
  (void)Bconin(2);
  return 1;
}

static void run(void) {
  console_init();
  console_clear();
  console_line(T_TITLE, VERSION);
  machine_identify(&machine);
  showMachine();
  device_recognise(&device);
  int ready = showDevice();
  if (ready && benchMode()) {
    Cconws("\033e");
    return;
  }
  showMenu(ready);

  for (;;) {
    int key = console_key();
    if (key == 'q') break;
    if (key == 's') {
      report_save(overridden ? T_REPORT_GATE_OVERRIDDEN
                  : unlocked ? T_REPORT_GATE_PASSED
                             : T_REPORT_GATE_LOCKED);
      console_line(T_MENU_PROMPT);
      continue;
    }
    if (ready && key == 'l') {
      lineTest(&lt_quick);
    } else if (ready && key == 'k') {
      lineTest(&lt_long);
    } else if (ready && key == 'm' && machine_isMegaSte(&machine)) {
      lineTestMegaSte();
#ifdef MDTEST_DEBUG
    } else if (key == 'o' && !unlocked) {
      console_line(T_GATE_OVERRIDE);
      unlocked = 1;
      overridden = 1;
#endif
    } else {
      continue;
    }
    showMenu(ready);
  }
  Cconws("\033e");
}

int main(void) {
  /* Supervisor mode on the program's own stack: the window, the cookie jar
   * and the system variables are read directly. */
  long ssp = Super(0L);
  run();
  Super(ssp);
  return 0;
}
