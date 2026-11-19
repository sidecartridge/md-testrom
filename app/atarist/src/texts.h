/*
 * Every string the user reads, in one place.
 *
 * Wording rules:
 * - A verdict line starts with PASS, FAIL, WARN or SKIP and says in plain
 *   words what was found; no register names, addresses or jargon on it.
 * - The technical detail goes on the line below, indented, for support.
 * - Advice says what to do next, as an instruction ("Update the firmware").
 * - Lines fit in 40 columns where they can: low resolution shows 40.
 */
#ifndef TEXTS_H
#define TEXTS_H

#define T_TITLE "SidecarTridge Multi-device Test %s"
#define T_MACHINE "Machine: %s, %s, TOS %x.%02x"
#define T_MACHINE_DETAIL "  TOS date %04x-%02x-%02x, GEMDOS %x.%02x"
#define T_MACHINE_SCREEN "  %s%s"
#define T_NO_COOKIE_JAR ", no cookie jar"
#define T_MACHINE_RAM "  RAM %lu KB, free %lu KB"
#define T_MACHINE_TTRAM "  TT-RAM %lu KB"
#define T_MACHINE_MSTE "  Mega STE at %s MHz, cache %s"
#define T_CPU "680%02d"

#define T_DEVICE_TEST "Device:  test firmware %s"
#define T_DEVICE_TEST_DETAIL "  layout %u, build %s"
#define T_DEVICE_NONE "Device:  none found"
#define T_DEVICE_OTHER "Device:  another microfirmware"
#define T_DEVICE_OLD "Device:  Multidevice Test %s"

#define T_FOUND_NONE "FAIL  No Multi-device answers in the cartridge port."
#define T_FOUND_NONE_DETAIL "  open bus where the window header should be"
#define T_FOUND_NONE_ADVICE \
  "Switch the computer off, check the device is fully in the port, then " \
  "start the Multidevice Test microfirmware from Booster."
#define T_FOUND_OTHER "FAIL  The device runs another microfirmware."
#define T_FOUND_OTHER_DETAIL "  no test header in the window"
#define T_FOUND_OTHER_ADVICE \
  "Start the Multidevice Test microfirmware from Booster, then run this " \
  "program again."
#define T_FOUND_OLD "FAIL  The device runs an old Multidevice Test."
#define T_FOUND_OLD_ADVICE \
  "Update the Multidevice Test microfirmware from Booster to version %s."
#define T_FOUND_LAYOUT_OLD "FAIL  The microfirmware is older than this program."
#define T_FOUND_LAYOUT_NEW "FAIL  This program is older than the microfirmware."
#define T_FOUND_LAYOUT_DETAIL "  window layout %u, this program reads layout %u"
#define T_FOUND_UPDATE_FIRMWARE \
  "Update the Multidevice Test microfirmware from Booster."
#define T_FOUND_UPDATE_PROGRAM \
  "Use the MDTEST.TOS of the same release as the microfirmware."
#define T_FOUND_COMMANDS "WARN  The device was left in command mode."
#define T_FOUND_COMMANDS_DETAIL "  window state %u: a previous run did not finish"
#define T_FOUND_OK "PASS  The test microfirmware answers."
#define T_DEVICE_DAMAGED "Device:  test firmware, header unreadable"
#define T_FOUND_DAMAGED "FAIL  The window reads with errors."
#define T_FOUND_DAMAGED_DETAIL "  the test pattern, but not its header"
#define T_FOUND_RUN_LINE "Run the line test to find the faulty line."
#define T_FOUND_NONE_LINE "The line test can tell what does not answer."

#define T_MENU_LINE "[L] Line test: address, data, select"
#define T_MENU_LINE_OFF "[ ] Line test: needs the test firmware"
#define T_MENU_LOCKED1 "    Wi-Fi, microSD, Bluetooth, SELECT"
#define T_MENU_LOCKED2 "    and LED: after the line test"
#define T_MENU_SAVE "[S] Save report"
#define T_MENU_QUIT "[Q] Quit"
#define T_MENU_PROMPT "Press a key."

#define T_LINE_TITLE "Line test: address, data and selects"
#define T_LINE_QUICK "Quick pass: about %lu s. Any key stops it."
#define T_LINE_LONG "Long pass: about %lu min. Any key stops it."
#define T_LINE_MSTE "Mega STE at %s MHz, cache %s:"
#define T_LINE_ROW "%-16s %s %s %3lu.%lu s"
#define T_LINE_ROW_ERRORS "%-16s %s FAIL %lu errors"
#define T_LINE_ROW_NOTIME "%-16s %s PASS"
#define T_LINE_ROW_STOPPED "%-16s %s stopped"
#define T_BANK_ROM4 "ROM4"
#define T_BANK_ROM3 "ROM3"
#define T_BOTH_BANKS "both banks"

#define T_TEST_SEQ_WORDS "Words in order"
#define T_TEST_SEQ_BYTES "Bytes in order"
#define T_TEST_RANDOM_WORDS "Random words"
#define T_TEST_RANDOM_BYTES "Random bytes"
#define T_TEST_ADDRESS_WALK "Address lines"
#define T_TEST_SAME_ADDRESS "Same address"
#define T_TEST_FAR_ADDRESSES "Far addresses"
#define T_TEST_AFTER_RAM "After RAM write"
#define T_TEST_IRQ_OFF "Interrupts off"

#define T_FIND_BANK "%s:"
#define T_FIND_NO_ANSWER "FAIL  %s does not answer."
#define T_FIND_NO_ANSWER_DETAIL "  every read is open bus ($FFFF or $0000)"
#define T_FIND_NO_ANSWER_ADVICE \
  "Check the !%s select line (GPIO %d) and the connector."
#define T_FIND_NONE_AT_ALL "FAIL  The device does not answer at all."
#define T_FIND_NONE_AT_ALL_ADVICE \
  "Check the device is fully in the port and the test microfirmware runs."
#define T_FIND_DATA_LOW "FAIL  Data line D%d is stuck low."
#define T_FIND_DATA_HIGH "FAIL  Data line D%d is stuck high."
#define T_FIND_DATA_DETAIL "  GPIO %d, %lu errors in %lu reads"
#define T_FIND_DATA_ADVICE \
  "Check D%d's level shifter and its pin on the cartridge connector."
#define T_FIND_DATA_PAIR "FAIL  Data lines D%d and D%d are shorted."
#define T_FIND_PAIR_ADVICE "Look for a solder bridge between the two lines."
#define T_FIND_ADDR_LOW "FAIL  Address line A%d is stuck low."
#define T_FIND_ADDR_HIGH "FAIL  Address line A%d is stuck high."
#define T_FIND_ADDR_DETAIL \
  "  GPIO %d: reads return another address's data"
#define T_FIND_ADDR_ADVICE \
  "Check A%d's level shifter and its pin on the cartridge connector."
#define T_FIND_ADDR_TIED "FAIL  Address lines A%d and A%d are shorted."
#define T_FIND_BANK_SWAP "FAIL  ROM3 and ROM4 are mixed up."
#define T_FIND_BANK_SWAP_ADVICE \
  "Check the !ROM3 and !ROM4 select lines (GPIO 26 and 22)."
#define T_FIND_PIN_DEAD "FAIL  The Pico's pin GPIO %d is dead."
#define T_FIND_PIN_DETAIL "  it carries D%d and A%d, and both fail"
#define T_FIND_PIN_ADVICE \
  "Check GPIO %d's solder joint on the Pico and its level shifters."
#define T_FIND_LANE "FAIL  Only the %s bytes read wrong."
#define T_FIND_LANE_EVEN "even"
#define T_FIND_LANE_ODD "odd"
#define T_FIND_LANE_ADVICE "Check the level shifter of data lines %s."
#define T_FIND_INTERMITTENT "FAIL  Errors come and go: %lu in %lu reads."
#define T_FIND_INTERMITTENT_ADVICE \
  "Try another computer; if they stay, check the connector and shifters."
#define T_FIND_UNCLASSIFIED "FAIL  The errors fit no single fault."
#define T_FIND_UNCLASSIFIED_ADVICE "Send the report to support."
#define T_FIND_UNSTABLE "  the same address gave two values %lu times"
#define T_FIND_FIRST "  %06lX: expected %04X, read %04X"
#define T_FIND_FIRST_BYTE "  %06lX: expected %02X, read %02X (byte)"

#define T_GATE_PASS "PASS  Every line answers."
#define T_GATE_UNLOCKED "The device tests are unlocked."
#define T_GATE_FAIL "The device tests stay locked until this test passes."
#define T_GATE_STOPPED "SKIP  The line test was stopped."
#define T_GATE_OVERRIDE "WARN  Gate overridden (debug build)."

#define T_MENU_LINE_LONG "[K] Line test, long pass"
#define T_MENU_LINE_MSTE "[M] Line test at 8, 16, 16+cache MHz"
#define T_MENU_UNLOCKED "    Device tests: unlocked"
#define T_MENU_OVERRIDE "[O] Override the gate (debug build)"
#define T_BENCH "Bench mode: the line test in a loop."
#define T_BENCH_RUN "Run %lu"
#define T_BENCH_REQUESTS "Bench mode: the RP's requests."
#define T_BENCH_RUN_TEST "Bench: test %u"
#define T_BENCH_DONE "Bench: done"
#define T_BENCH_STOP "Bench mode: stopped by the RP."
#define T_BENCH_SKIP "WARN  Line test skipped (debug bench)."

#define T_SAVED "Report saved as %s."
#define T_SAVE_FAILED "WARN  The report could not be saved."
#define T_SAVE_FAILED_DETAIL "  %s: GEMDOS error %ld"
#define T_SAVE_ADVICE "Check the disk is not write-protected or full."
#define T_SAVE_NONAME "  no free name REPORT00.TXT to REPORT99.TXT"

#define T_REPORT_FORMAT "MDTEST REPORT 1"
/* The report's second line, whatever was run: the gate's state. */
#define T_REPORT_GATE_PASSED "Line test and command channel: passed, device tests unlocked."
#define T_REPORT_GATE_CHANNEL "Line test: passed; command channel: not passed, device tests locked."
#define T_REPORT_GATE_LOCKED "Line test: not passed, device tests locked."
#define T_REPORT_GATE_OVERRIDDEN "Line test: gate overridden (debug build)."


/* The command channel */
#define T_CH_TITLE "Command channel test"
#define T_CH_NO_SELECT "FAIL  The device takes no commands."
#define T_CH_NO_SELECT_DETAIL "  no answer to the select frame"
#define T_CH_NO_SELECT_ADVICE "Check !ROM3 into the RP (GPIO 26)."
#define T_CH_NO_HELLO "FAIL  The device stopped answering."
#define T_CH_NO_HELLO_DETAIL "  ROM3 selected, no answer to hello"
#define T_CH_NO_HELLO_ADVICE "Check GPIO 26; update the firmware."
#define T_CH_HELLO "PASS  The device takes commands."
#define T_CH_HELLO_DETAIL "  protocol %u, boot %08lX"
#define T_CH_VERSION "FAIL  Program and firmware differ."
#define T_CH_VERSION_DETAIL "  protocol: program %u, firmware %u"
#define T_CH_ROW "%-16s%6lu  %s"
#define T_CH_ROW_ERRORS "%-16s%6lu  %lu errors"
#define T_CH_HELLOS "Hello burst"
#define T_CH_ECHOES "Echo, mixed"
#define T_CH_DURING_JOB "During a job"
#define T_CH_COUNTERS "RP counters"
#define T_CH_CLEAN "PASS  The command channel is clean."
#define T_CH_RETRIES "WARN  Commands needed retries."
#define T_CH_RETRIES_DETAIL "  %lu retries in %lu commands"
#define T_CH_RETRIES_FIRST "  first: command $%04X, %u words"
#define T_CH_RETRIES_ADVICE "Clean the cartridge port's contacts."
#define T_CH_UNRELIABLE "FAIL  The command channel is unreliable."
#define T_CH_UNRELIABLE_DETAIL "  %lu unanswered, %lu wrong answers"
#define T_CH_RESTARTED "FAIL  The device restarted during the test."
#define T_CH_RESTARTED_DETAIL "  boot %08lX, then %08lX"
#define T_CH_COUNTERS_DETAIL "  RP answered %lu, program saw %lu"
#define T_CH_COUNTERS_ERRORS "  RP: %lu checksum errors, %lu overruns"
#define T_CH_LOCKED "The device tests stay locked."
#define T_CH_UNSELECT_FAIL "WARN  ROM3 was not given back."
#define T_MENU_CHANNEL "[C] Command channel test"
#define T_MENU_CHANNEL_NEXT "    then: [C] command channel test"
#define T_JOB_RESULT "  %s"
#define T_JOB_FAILED "FAIL  Test %u did not finish."
#define T_JOB_FAILED_DETAIL "  job %u, state %u"
#define T_JOB_RESTARTED "FAIL  The device restarted during test %u."
#define T_JOB_RESTARTED_DETAIL "  ROM3 is test data again: the job is lost"

#endif
