/*
 * MDTEST.TOS: tests a SidecarTridge Multi-device from the Atari.
 *
 * It identifies the machine, recognises what is in the cartridge port from
 * plain reads of the window (common/mdtest.h), and shows the tests. Text
 * only, over GEMDOS and BIOS: any resolution, any machine, the AUTO folder.
 */
#include <mint/osbind.h>
#include <stdio.h>
#include <string.h>

#include "chscreen.h"
#include "commands.h"
#include "console.h"
#include "devscreen.h"
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
/* The gate: the device tests run only once the line test and then the
 * command channel's test have passed (or, on a debug build, the gate was
 * overridden), for this session. */
static int linePassed;
static int channelPassed;
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
    if (cmd_unselect() != CMD_OK) console_line(T_CH_UNSELECT_FAIL);
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
  if (linePassed || overridden) console_line(T_MENU_CHANNEL);
  if (unlocked) {
    console_line(T_MENU_INFO);
    console_line(T_MENU_SELECT);
    console_line(T_MENU_LED);
    console_line(T_MENU_CARD);
    console_line(T_MENU_CARD_SWAP);
    console_line(T_MENU_WIFI);
    console_line(T_MENU_BT);
  } else if (linePassed) {
    console_line(T_MENU_LOCKED1);
    console_line(T_MENU_CHANNEL_NEXT);
  } else {
    console_line(T_MENU_LOCKED1);
    console_line(T_MENU_LOCKED2);
  }
#ifdef MDTEST_DEBUG
  if (!unlocked) console_line(T_MENU_OVERRIDE);
#endif
  if (device.kind == DEVICE_TEST) console_line(T_MENU_EXIT);
  console_line(T_MENU_SAVE);
  console_line(T_MENU_QUIT);
  console_line(T_MENU_PROMPT);
}

/* ROM3 as data again before any read of it: the mirror stops with it. */
static void giveBackRom3(void) {
  console_setMirror(0);
  if (cmd_selected() && cmd_unselect() != CMD_OK) {
    console_line(T_CH_UNSELECT_FAIL);
  }
}

static void lineTestPassed(void) {
  linePassed = 1;
  unlocked = overridden || (linePassed && channelPassed);
}

static void lineTest(const LtSizes *sizes) {
  giveBackRom3();
  if (ltscreen_run(sizes, ltscreen_estimate(sizes)) == LT_PASS) {
    lineTestPassed();
  }
}

static void channelTest(void) {
  ChVerdict v = chscreen_test(machine.mch);
  channelPassed = v == CH_CLEAN || v == CH_RETRIES;
  unlocked = overridden || (linePassed && channelPassed);
  if (channelPassed && unlocked) console_line(T_GATE_UNLOCKED);
}

/* The quick pass at each Mega STE setting; the user's is put back. */
static void lineTestMegaSte(void) {
  static const unsigned char settings[3] = {0x00, 0x02, 0x03};
  unsigned char saved = MSTE_SPEED_CACHE;
  int passes = 0;
  giveBackRom3();
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
  if (passes == 3) lineTestPassed();
}

/* What MDTEST.BNC asks for: its first word. */
#define BENCH_LINE 1     /* anything else: the line test in a loop */
#define BENCH_REQUESTS 2 /* "requests": the line test, then the RP's requests */
#define BENCH_COMMANDS 3 /* "commands", debug builds: the requests at once */

static int benchFile(void) {
  char text[16] = {0};
  long handle = Fopen(BENCH_FILE, 0);
  if (handle < 0) return 0;
  long n = Fread((short)handle, sizeof(text) - 1, text);
  Fclose((short)handle);
  if (n > 0 && strncmp(text, "requests", 8) == 0) return BENCH_REQUESTS;
#ifdef MDTEST_DEBUG
  if (n > 0 && strncmp(text, "commands", 8) == 0) return BENCH_COMMANDS;
#endif
  return BENCH_LINE;
}

static int keyPressed(void) {
  if (!Bconstat(2)) return 0;
  (void)Bconin(2);
  return 1;
}

static void waitTicks(unsigned long ticks) {
  unsigned long start = *(volatile unsigned long *)0x4BA;
  while (*(volatile unsigned long *)0x4BA - start < ticks) {
  }
}

/* The RP's requests, polled until it says stop or a key is pressed. */
/* An RP that started again serves ROM3 as data once it is up, seconds after
 * the reset: select it again, for as long as a boot takes. */
#define REOPEN_TICKS (200UL * 10)

static int reopen(void) {
  unsigned long start = *(volatile unsigned long *)0x4BA;
  while (*(volatile unsigned long *)0x4BA - start < REOPEN_TICKS) {
    if (cmd_select() == CMD_OK) break;
  }
  return chscreen_open(machine.mch);
}

static void benchRequests(void) {
  console_line(T_BENCH_REQUESTS);
  if (!chscreen_open(machine.mch)) return;
  while (!keyPressed()) {
    CmdAnswer a;
    waitTicks(50);
    if (cmd_request(&a) != CMD_OK) {
      if (!cmd_selected() && !reopen()) return;
      continue;
    }
    if (a.data[0] == MDTEST_REQUEST_RUN) {
      console_line(T_BENCH_RUN_TEST, a.data[1]);
      chscreen_job(a.data[1], &a.data[2]);
      console_line(T_BENCH_DONE);
    } else if (a.data[0] == MDTEST_REQUEST_LINE_TEST) {
      giveBackRom3();
      ltscreen_run(&lt_quick, ltscreen_estimate(&lt_quick));
      if (!chscreen_open(machine.mch)) return;
      console_line(T_BENCH_DONE);
    } else if (a.data[0] == MDTEST_REQUEST_CHANNEL_TEST) {
      chscreen_test(machine.mch);
      if (!cmd_selected() && !reopen()) return;
      console_line(T_BENCH_DONE);
    } else if (a.data[0] == MDTEST_REQUEST_STOP) {
      console_line(T_BENCH_STOP);
      break;
    }
  }
  giveBackRom3();
}

/* Bench mode, selected by MDTEST.BNC next to the program: the line test in a
 * loop, each verdict sent to the RP, until a key; or the RP's requests. */
static int benchMode(void) {
  int mode = benchFile();
  if (!mode) return 0;
  if (mode == BENCH_COMMANDS) {
    console_line(T_BENCH_SKIP);
    benchRequests();
    return 1;
  }
  console_line(T_BENCH);
  for (unsigned long run = 1; !keyPressed(); run++) {
    console_line(T_BENCH_RUN, run);
    LtVerdict v = ltscreen_run(&lt_quick, ltscreen_estimate(&lt_quick));
    if (mode == BENCH_REQUESTS && v == LT_PASS) {
      benchRequests();
      return 1;
    }
  }
  return 1;
}

static void run(void) {
  console_init();
  console_clear();
  console_line(T_TITLE, VERSION);
  machine_identify(&machine);
  showMachine();
  cmd_init(machine.mch, machine_isMegaSte(&machine));
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
                  : linePassed ? T_REPORT_GATE_CHANNEL
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
    } else if (key == 'c' && (linePassed || overridden)) {
      channelTest();
    } else if (unlocked && (key == 'i' || key == 'p' || key == 'g' ||
                            key == 'd' || key == 'r' || key == 'w' || key == 't')) {
      console_blank();
      if (chscreen_open(machine.mch)) {
        if (key == 'i') devscreen_info();
        if (key == 'p') devscreen_select();
        if (key == 'g') devscreen_led();
        if (key == 'd') devscreen_card();
        if (key == 'r') devscreen_cardSwap();
        if (key == 'w') devscreen_wifi();
        if (key == 't') devscreen_bluetooth();
      }
    } else if (key == 'b' && device.kind == DEVICE_TEST) {
      if (devscreen_exit(machine.mch)) {
        /* The cartridge port is Booster's: quit without reading it. */
        console_line(T_EXIT_QUIT);
        (void)console_key();
        Cconws("\033e");
        return;
      }
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
  giveBackRom3();
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
