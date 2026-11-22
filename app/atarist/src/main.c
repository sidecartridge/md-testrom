/*
 * MDTEST.TOS: tests a SidecarTridge Multi-device from the Atari.
 *
 * It identifies the machine, recognises what is in the cartridge port from
 * plain reads of the window (common/mdtest.h), and shows the tests. It runs
 * without TOS once started (hw.c): its own screen, keyboard and clock, 80
 * columns on every machine; quitting restarts the computer, so it does not
 * belong in the AUTO folder.
 */
#include <stdio.h>
#include <string.h>

#include "chscreen.h"
#include "commands.h"
#include "console.h"
#include "devscreen.h"
#include "device.h"
#include "ltscreen.h"
#include "hw.h"
#include "machine.h"
#include "mdtest.h"
#include "report.h"
#include "summary.h"
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
#define KEY_ESC 27
#define KEY_RETURN 13
#define MENU_RESULT_COLUMN 50

/* Each test's last result, beside it in the menu: by its key. */
static const char *lastResult[26];

static void setResult(int key, const char *result) {
  if (key >= 'a' && key <= 'z') lastResult[key - 'a'] = result;
}

static const char *outcomeWord(Outcome o) {
  switch (o) {
    case OUT_PASS:
      return T_OUT_PASS;
    case OUT_WARN:
      return T_OUT_WARN;
    case OUT_FAIL:
      return T_OUT_FAIL;
    case OUT_SKIP:
      return T_OUT_SKIP;
    case OUT_STOPPED:
      return T_OUT_STOPPED;
    default:
      return 0;
  }
}

static void showMachine(void) {
  char cpu[8];
  sprintf(cpu, T_CPU, (int)machine.cpu);
  console_line(T_MACHINE, machine_name(&machine), cpu, machine.tos >> 8,
               machine.tos & 0xFF);
  console_line(T_MACHINE_DETAIL, (unsigned)(machine.tosDate & 0xFFFF),
               (unsigned)(machine.tosDate >> 24),
               (unsigned)((machine.tosDate >> 16) & 0xFF));
  console_line(T_MACHINE_SCREEN, machine_screen(&machine),
               machine.mch == -1 ? T_NO_COOKIE_JAR : "");
  console_line(T_MACHINE_RAM, machine.stRam / 1024);
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

/* The menu's entries, in order: each with its key (0 for a line that is not
 * a test). */
typedef struct {
  int key;
  const char *text;
} MenuEntry;

static int menuEntries(int lineTestReady, MenuEntry *e) {
  int n = 0;
  if (lineTestReady) {
    e[n++] = (MenuEntry){'a', T_MENU_RUN_ALL};
    e[n++] = (MenuEntry){'l', T_MENU_LINE};
    e[n++] = (MenuEntry){'k', T_MENU_LINE_LONG};
    if (machine_isMegaSte(&machine)) e[n++] = (MenuEntry){'m', T_MENU_LINE_MSTE};
  } else {
    e[n++] = (MenuEntry){0, T_MENU_LINE_OFF};
  }
  if (linePassed || overridden) e[n++] = (MenuEntry){'c', T_MENU_CHANNEL};
  if (unlocked) {
    e[n++] = (MenuEntry){'i', T_MENU_INFO};
    e[n++] = (MenuEntry){'p', T_MENU_SELECT};
    e[n++] = (MenuEntry){'g', T_MENU_LED};
    e[n++] = (MenuEntry){'d', T_MENU_CARD};
    e[n++] = (MenuEntry){'r', T_MENU_CARD_SWAP};
    e[n++] = (MenuEntry){'w', T_MENU_WIFI};
    e[n++] = (MenuEntry){'t', T_MENU_BT};
  } else if (linePassed) {
    e[n++] = (MenuEntry){0, T_MENU_LOCKED1};
    e[n++] = (MenuEntry){0, T_MENU_CHANNEL_NEXT};
  } else {
    e[n++] = (MenuEntry){0, T_MENU_LOCKED1};
    e[n++] = (MenuEntry){0, T_MENU_LOCKED2};
  }
#ifdef MDTEST_DEBUG
  if (!unlocked) e[n++] = (MenuEntry){'o', T_MENU_OVERRIDE};
#endif
  if (device.kind == DEVICE_TEST) e[n++] = (MenuEntry){'b', T_MENU_EXIT};
  e[n++] = (MenuEntry){'s', T_MENU_SAVE};
  e[n++] = (MenuEntry){'q', T_MENU_QUIT};
  return n;
}

/* The menu as a panel: the body cleared, each entry with its test's last
 * result. Screen only. */
static void drawMenu(int lineTestReady) {
  MenuEntry e[24];
  int n = menuEntries(lineTestReady, e);
  char title[48];
  snprintf(title, sizeof(title), T_TITLE, VERSION);
  console_screen(title, 0);
  for (int i = 0; i < n; i++) {
    int row = console_bodyRow();
    console_at(2, row, e[i].text);
    const char *result = e[i].key ? lastResult[e[i].key - 'a'] : 0;
    if (result) console_at(MENU_RESULT_COLUMN, row, result);
    console_bodyFrom(row + 1);
  }
  console_status(T_MENU_PROMPT, T_MENU_STATUS_RIGHT);
}

/* The menu's lines for the report and the host's log, as every earlier
 * version wrote them; drawn as the panel only when asked (at the start, and
 * on Space), so a test's results stay on the screen until then. */
static void showMenu(int lineTestReady, int draw) {
  MenuEntry e[24];
  int n = menuEntries(lineTestReady, e);
  console_record("");
  for (int i = 0; i < n; i++) console_record("%s", e[i].text);
  console_record(T_MENU_PROMPT);
  if (draw) {
    drawMenu(lineTestReady);
  } else {
    console_status(T_MENU_AFTER_TEST, T_MENU_STATUS_RIGHT);
  }
}

/* ROM3 as data again before any read of it: the mirror stops with it. */
static void giveBackRom3(void) {
  console_setMirror(0);
  if (cmd_selected() && cmd_unselect() != CMD_OK) {
    console_line(T_CH_UNSELECT_FAIL);
  }
}

/* The device tests need the command channel's test too: the line test
 * unlocks them only when that one has already passed. */
static void lineTestPassed(void) {
  int was = unlocked;
  linePassed = 1;
  unlocked = overridden || (linePassed && channelPassed);
  if (unlocked && !was) console_line(T_GATE_UNLOCKED);
}

static void lineTest(int key, const LtSizes *sizes) {
  giveBackRom3();
  LtVerdict v = ltscreen_run(sizes, ltscreen_estimate(sizes));
  setResult(key, v == LT_PASS     ? T_OUT_PASS
                 : v == LT_FAIL   ? T_OUT_FAIL
                                  : T_OUT_STOPPED);
  if (v == LT_PASS) lineTestPassed();
}

static void channelTest(void) {
  ChVerdict v = chscreen_test(machine.mch);
  channelPassed = v == CH_CLEAN || v == CH_RETRIES;
  setResult('c', v == CH_CLEAN     ? T_OUT_PASS
                 : v == CH_RETRIES ? T_OUT_WARN
                                   : T_OUT_FAIL);
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
  setResult('m', passes == 3 ? T_OUT_PASS : T_OUT_FAIL);
  if (passes == 3) lineTestPassed();
}

static int keyPressed(void) { return console_keyPressed(); }

static void waitTicks(unsigned long ticks) {
  unsigned long start = hw_ticks;
  while (hw_ticks - start < ticks) {
  }
}

/* The RP's requests, polled until it says stop or a key is pressed. */
/* An RP that started again serves ROM3 as data once it is up, seconds after
 * the reset: select it again, for as long as a boot takes. */
#define REOPEN_TICKS (200UL * 10)

static int reopen(void) {
  unsigned long start = hw_ticks;
  while (hw_ticks - start < REOPEN_TICKS) {
    if (cmd_select() == CMD_OK) break;
  }
  return chscreen_greet(machine.mch);
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

/* Bench mode, asked for in the window's header (a debug or EmuMD firmware):
 * the line test in a loop, each verdict sent to the RP, until a key; or the
 * RP's requests. */
static int benchMode(void) {
  int mode = device.bench;
  if (mode == MDTEST_BENCH_NONE || mode > MDTEST_BENCH_COMMANDS) return 0;
#ifndef MDTEST_DEBUG
  if (mode == MDTEST_BENCH_COMMANDS) mode = MDTEST_BENCH_LINE;
#endif
  if (mode == MDTEST_BENCH_COMMANDS) {
    console_line(T_BENCH_SKIP);
    benchRequests();
    return 1;
  }
  console_line(T_BENCH);
  for (unsigned long run = 1; !keyPressed(); run++) {
    console_line(T_BENCH_RUN, run);
    LtVerdict v = ltscreen_run(&lt_quick, ltscreen_estimate(&lt_quick));
    if (mode == MDTEST_BENCH_REQUESTS && v == LT_PASS) {
      benchRequests();
      return 1;
    }
  }
  return 1;
}

/* The guided run ([A]): the line test, the command channel, then the
 * device tests, each layer only once the one under it passed; the tests
 * that need the user only if they say so; then the summary. A key between
 * two tests (or during the line test) stops it with what it has. */
#define RUN_DEVICE_SECONDS 40UL /* the channel's test and the device tests: 38 s on a Mega ST */
#define HZ_200 hw_ticks

static Results results;

/* The summary as a panel of its own: its title starts a new screen. */
static void putSummary(const char *line, void *ctx) {
  int *started = ctx;
  if (!*started) {
    if (line[0] == '\0') return; /* the blank before the title */
    if (!strcmp(line, T_SUM_TITLE)) {
      console_page(T_SUM_TITLE);
      *started = 1;
      return;
    }
  }
  console_line("%s", line);
}

/* A key pressed since the last test: this area and the rest stopped. */
static int stoppedAt(Area area) {
  if (!keyPressed()) return 0;
  console_line(T_RUN_STOPPED);
  results.outcome[area] = OUT_STOPPED;
  return 1;
}

static void runDevice(Area area, void (*test)(void)) {
  static const char areaKeys[AREA_COUNT] = {'l', 'c', 'i', 'd', 'w',
                                            't', 'p', 'g', 'r'};
  test();
  devscreen_verdict(&results.outcome[area], &results.reason[area]);
  setResult(areaKeys[area], outcomeWord(results.outcome[area]));
}

/* A test the silent radio chip makes meaningless: said once, skipped. */
static int radioSilent(Area area, const char *name) {
  if (results.reason[AREA_WIFI] != R_CHIP_SILENT &&
      results.reason[AREA_BLUETOOTH] != R_CHIP_SILENT) {
    return 0;
  }
  console_blank();
  console_line(T_RUN_SKIP_RADIO, name);
  results.outcome[area] = OUT_SKIP;
  results.reason[area] = R_CHIP_SILENT;
  return 1;
}

static void runTests(const LtSizes *sizes) {
  for (int i = 0; i < AREA_COUNT; i++) results.outcome[i] = OUT_LOCKED;

  if (overridden) {
    console_blank();
    console_line(T_RUN_OVERRIDDEN);
    results.outcome[AREA_LINES] = OUT_SKIP;
  } else {
    giveBackRom3();
    LtVerdict v = ltscreen_run(sizes, ltscreen_estimate(sizes));
    if (v == LT_STOPPED) {
      results.outcome[AREA_LINES] = OUT_STOPPED;
      return;
    }
    if (v != LT_PASS) {
      results.outcome[AREA_LINES] = OUT_FAIL;
      results.reason[AREA_LINES] = R_LINES;
      console_line(T_RUN_GATE_LINES);
      return;
    }
    results.outcome[AREA_LINES] = OUT_PASS;
    setResult('l', T_OUT_PASS);
    lineTestPassed();
  }
  if (stoppedAt(AREA_COMMANDS)) return;

  channelTest();
  if (!channelPassed) {
    results.outcome[AREA_COMMANDS] = OUT_FAIL;
    results.reason[AREA_COMMANDS] =
        machine_isMegaSte(&machine) && machine.msteFast && machine.msteCache
            ? R_CMD_CACHE
            : R_CMD;
    console_line(T_RUN_GATE_CHANNEL);
    return;
  }
  results.outcome[AREA_COMMANDS] = OUT_PASS;
  for (int i = AREA_DEVICE; i < AREA_COUNT; i++) results.outcome[i] = OUT_NOT_RUN;
  /* The channel's test left it open: opening it again would say hello twice. */
  if (!cmd_selected() && !chscreen_open(machine.mch)) {
    results.outcome[AREA_DEVICE] = OUT_FAIL;
    results.reason[AREA_DEVICE] = R_LOST;
    return;
  }

  if (stoppedAt(AREA_DEVICE)) return;
  runDevice(AREA_DEVICE, devscreen_info);
  if (stoppedAt(AREA_CARD)) return;
  runDevice(AREA_CARD, devscreen_card);
  if (stoppedAt(AREA_WIFI)) return;
  runDevice(AREA_WIFI, devscreen_wifi);
  if (stoppedAt(AREA_BLUETOOTH)) return;
  if (!radioSilent(AREA_BLUETOOTH, T_AREA_BLUETOOTH)) {
    runDevice(AREA_BLUETOOTH, devscreen_bluetooth);
  }

  if (stoppedAt(AREA_SELECT)) return;
  console_blank();
  console_line(T_RUN_ASK_HANDS);
  int key;
  do {
    key = console_key();
  } while (key != 'y' && key != 'n');
  console_line(key == 'y' ? T_YES : T_NO);
  if (key != 'y') return;
  runDevice(AREA_SELECT, devscreen_select);
  if (stoppedAt(AREA_LED)) return;
  if (!radioSilent(AREA_LED, T_AREA_LED)) runDevice(AREA_LED, devscreen_led);
  if (stoppedAt(AREA_CARD_SWAP)) return;
  runDevice(AREA_CARD_SWAP, devscreen_cardSwap);
}

static void runAll(void) {
  unsigned long quick = ltscreen_estimate(&lt_quick) + RUN_DEVICE_SECONDS;
  unsigned long thorough = ltscreen_estimate(&lt_long) + RUN_DEVICE_SECONDS;
  console_page(T_RUN_TITLE);
  console_line(T_RUN_CHOOSE, (quick + 59) / 60, (thorough + 59) / 60);
  int key;
  do {
    key = console_key();
  } while (key != 'q' && key != 't' && key != KEY_ESC);
  if (key == KEY_ESC) return;
  console_line(T_RUN_STOP_HINT);
  console_status(T_RUN_STOP_HINT, 0);
  unsigned long start = HZ_200;
  summary_clear(&results);
  runTests(key == 't' ? &lt_long : &lt_quick);
  giveBackRom3();
  int started = 0;
  summary_write(&results, putSummary, &started);
  console_line(T_RUN_TOOK, (HZ_200 - start) / 200);
}

/* Quit: the computer restarts (nothing of TOS is left to return to). Asked
 * once more on the bottom line, so a stray Q does not lose the session. */
static int confirmQuit(void) {
  console_status(T_QUIT_CONFIRM, 0);
  return console_key() == 'q';
}

static void restart(int giveBack) {
  if (giveBack) giveBackRom3();
  console_record(T_QUIT_RESTART);
  console_status(T_QUIT_RESTART, 0);
  hw_restart();
}

static void run(void) {
  char machineText[48];
  char cpu[8];
  console_init();
  sprintf(cpu, T_CPU, (int)machine.cpu);
  snprintf(machineText, sizeof(machineText), T_TITLE_MACHINE,
           machine_name(&machine), cpu, machine.tos >> 8, machine.tos & 0xFF);
  console_setMachine(machineText);
  char title[48];
  snprintf(title, sizeof(title), T_TITLE, VERSION);
  console_title(title, machineText);
  console_line(T_TITLE, VERSION);
  showMachine();
  cmd_init(machine.mch, machine_isMegaSte(&machine));
  device_recognise(&device);
  int ready = showDevice();
  if (device.kind == DEVICE_TEST) {
    char text[80];
    snprintf(text, sizeof(text), T_DEVICE_BAR, device.version, device.build);
    console_device(text);
  }
  if (ready && benchMode()) return;
  /* The menu's panel at once when the test firmware answers as expected;
   * otherwise what was found stays on the screen, the menu a key away. */
  showMenu(ready, device.kind == DEVICE_TEST &&
                      device.layout == PROGRAM_LAYOUT_VERSION);

  for (;;) {
    int key = console_key();
    if (key == 'q') {
      if (confirmQuit()) restart(1);
      showMenu(ready, 0);
      continue;
    }
    if (key == ' ' || key == KEY_RETURN) {
      drawMenu(ready);
      continue;
    }
    if (key == 's') {
      report_save(overridden ? T_REPORT_GATE_OVERRIDDEN
                  : unlocked ? T_REPORT_GATE_PASSED
                  : linePassed ? T_REPORT_GATE_CHANNEL
                               : T_REPORT_GATE_LOCKED,
                  unlocked, machine.mch);
      console_line(T_MENU_PROMPT);
      continue;
    }
    if (ready && key == 'a') {
      runAll();
    } else if (ready && (key == 'l' || key == 'k')) {
      lineTest(key, key == 'l' ? &lt_quick : &lt_long);
    } else if (ready && key == 'm' && machine_isMegaSte(&machine)) {
      lineTestMegaSte();
    } else if (key == 'c' && (linePassed || overridden)) {
      channelTest();
    } else if (unlocked && (key == 'i' || key == 'p' || key == 'g' ||
                            key == 'd' || key == 'r' || key == 'w' || key == 't')) {
      if (chscreen_open(machine.mch)) {
        if (key == 'i') devscreen_info();
        if (key == 'p') devscreen_select();
        if (key == 'g') devscreen_led();
        if (key == 'd') devscreen_card();
        if (key == 'r') devscreen_cardSwap();
        if (key == 'w') devscreen_wifi();
        if (key == 't') devscreen_bluetooth();
        Outcome o;
        Reason r;
        devscreen_verdict(&o, &r);
        setResult(key, outcomeWord(o));
      }
    } else if (key == 'b' && device.kind == DEVICE_TEST) {
      if (devscreen_exit(machine.mch)) {
        /* The cartridge port is Booster's: nothing reads it again. */
        console_line(T_EXIT_QUIT);
        (void)console_key();
        restart(0);
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
    showMenu(ready, 0);
  }
}

/* From start.s, in supervisor mode on the program's own stack. */
int main(void) {
  machine_identify(&machine); /* what TOS left in RAM, read once */
  hw_takeOver(&machine);      /* from here on, no TOS */
  run();
  restart(1);
  return 0;
}
