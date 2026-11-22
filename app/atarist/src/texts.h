/*
 * Every string the user reads, in one place.
 *
 * Wording rules:
 * - A verdict line starts with PASS, FAIL, WARN or SKIP and says in plain
 *   words what was found; no register names, addresses or jargon on it.
 * - The technical detail goes on the line below, indented, for support.
 * - Advice says what to do next, as an instruction ("Update the firmware").
 * - The screen is 80 columns on every machine (the program sets medium or
 *   high resolution itself); a longer line wraps under its own indentation.
 */
#ifndef TEXTS_H
#define TEXTS_H

#define T_TITLE "SidecarTridge Multi-device Test %s"
#define T_TITLE_MACHINE "%s  %s  TOS %x.%02x"
#define T_DEVICE_BAR "Device   test firmware %s, build %s"
#define T_MACHINE "Machine: %s, %s, TOS %x.%02x"
#define T_MACHINE_DETAIL "  TOS date %04x-%02x-%02x"
#define T_MACHINE_SCREEN "  %s%s"
#define T_NO_COOKIE_JAR ", no cookie jar"
#define T_MACHINE_RAM "  RAM %lu KB"
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
#define T_MENU_QUIT "[Q] Quit: restarts the computer"
#define T_MENU_PROMPT "Press a key."
#define T_MENU_STATUS_RIGHT "Space: the menu"
#define T_MENU_AFTER_TEST "Press a test's key, or Space for the menu."
#define T_QUIT_CONFIRM \
  "Q again restarts the computer; any other key stays in the program."
#define T_QUIT_RESTART "Restarting the computer..."

#define T_LINE_TITLE "Line test: address, data and selects"
#define T_LINE_QUICK "Quick pass: about %lu s. Any key stops it."
#define T_LINE_LONG "Long pass: about %lu min. Any key stops it."
#define T_LINE_MSTE "Mega STE at %s MHz, cache %s:"
#define T_LINE_ROW "%-16s %s %s %3lu.%lu s"
#define T_LINE_ROW_ERRORS "%-16s %s FAIL %lu errors"
#define T_LINE_ROW_NOTIME "%-16s %s PASS"
#define T_LINE_ROW_STOPPED "%-16s %s stopped"
#define T_LINE_STATUS "Any key stops the line test."
#define T_LINE_HEAD_TEST "Test"
#define T_LINE_CELL_RUNNING "reading..."
#define T_LINE_CELL_PASS "PASS  %3lu.%lu s"
#define T_LINE_CELL_ERRORS "FAIL  %lu errors"
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
#define T_MENU_OVERRIDE "[O] Override the gate (debug build)"
#define T_BENCH "Bench mode: the line test in a loop."
#define T_BENCH_RUN "Run %lu"
#define T_BENCH_REQUESTS "Bench mode: the RP's requests."
#define T_BENCH_RUN_TEST "Bench: test %u"
#define T_BENCH_DONE "Bench: done"
#define T_BENCH_STOP "Bench mode: stopped by the RP."
#define T_BENCH_SKIP "WARN  Line test skipped (debug bench)."

#define T_SAVING "Saving the report on the microSD card..."
#define T_SAVED "Report saved on the microSD card as %s."
#define T_SAVE_FAILED "WARN  The report could not be saved."
#define T_SAVE_FAILED_DETAIL "  %s"
#define T_SAVE_ADVICE \
  "Check a microSD card is in the device and has room, or photograph the " \
  "screen for support."
#define T_SAVE_LOCKED "WARN  The report cannot be saved yet."
#define T_SAVE_LOCKED_DETAIL \
  "  it is written on the microSD card over the command channel, which " \
  "opens once the line test and the channel's test pass"
#define T_SAVE_LOCKED_ADVICE "Photograph the screen for support."
#define T_SAVE_NO_DEVICE "  the device stopped answering"
#define T_SAVE_ASK_NAMES \
  "Keep the Wi-Fi and Bluetooth names heard in the report? [Y/N]"
#define T_SAVE_NAMES_KEPT "  names kept"
#define T_SAVE_NAMES_LEFT_OUT "  names left out"
#define T_NETWORK_LEFT_OUT "(name left out)"

#define T_REPORT_FORMAT "MDTEST REPORT 1"
#define T_REPORT_FULL "(the report is full: %u later lines were not kept)"
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

/* The device's own tests. */
#define T_MENU_INFO "[I] Device information"
#define T_MENU_SELECT "[P] SELECT button"
#define T_MENU_LED "[G] Green LED"
#define T_MENU_CARD "[D] microSD card"
#define T_MENU_CARD_SWAP "[R] microSD card out and back in"
#define T_MENU_WIFI "[W] Wi-Fi"
#define T_MENU_BT "[T] Bluetooth"
#define T_MENU_EXIT "[B] Back to Booster"
#define T_YES "yes"
#define T_NO "no"

#define T_INFO_TITLE "Device information"
#define T_INFO_FIRMWARE "  Firmware  %s, build %s"
#define T_INFO_BOARD "  Board     Pico W, ID %s"
#define T_INFO_FLASH "  Flash     %s KB"
#define T_INFO_CLOCK_MV "  Clock     %s MHz, core %s mV"
#define T_INFO_CLOCK "  Clock     %s MHz"
#define T_INFO_TEMP "  Chip      %s C"
#define T_INFO_UPTIME "  Running   %s s"
#define T_INFO_HEAP "  Heap      %s bytes free, %s lowest"
#define T_INFO_STACK "  Stack     %s of %s bytes used"
#define T_INFO_RESTART "  Started   %s"
#define T_INFO_RESTART_PHASE "    while: %s"
#define T_INFO_RESTART_PC "    at: %s"
#define T_INFO_RESTART_POWER "at power-on"
#define T_INFO_RESTART_RESET "after a reset asked for"
#define T_INFO_RESTART_PANIC "after a firmware panic"
#define T_INFO_RESTART_FAULT "after a firmware crash"
#define T_INFO_RESTART_HANG "after a hang (watchdog)"
#define T_INFO_RESTART_OTHER "after a reboot"
#define T_INFO_HOSTNAME "  Hostname  %s"
#define T_INFO_WIFI "  Wi-Fi     %s"
#define T_INFO_WIFI_SET "a network is set up"
#define T_INFO_WIFI_NONE "no network set up"
#define T_INFO_SD "  microSD   %s KB/s"
#define T_INFO_PASS "PASS  The device reports no problem."
#define T_INFO_WARN_CRASH "WARN  The device restarted after a crash."
#define T_INFO_WARN_CRASH_DETAIL "  last restart: %s"
#define T_INFO_WARN_CRASH_ADVICE \
  "If this happens again, save the report and send it to support."
#define T_INFO_WARN_LOOP "WARN  The device keeps restarting."
#define T_INFO_WARN_LOOP_DETAIL "  %s crash restarts in a minute"
#define T_INFO_WARN_TEMP "WARN  The device's chip is out of range."
#define T_INFO_WARN_TEMP_DETAIL "  %s C, expected %d to %d C"
#define T_INFO_WARN_SETTINGS "WARN  Booster's settings could not be read."
#define T_INFO_WARN_SETTINGS_ADVICE \
  "Open Booster and save its settings again."

#define T_SEL_TITLE "SELECT button"
#define T_SEL_ASK "Press SELECT on the device once and release it (%u s)."
#define T_SEL_PRESSED "  SELECT pressed"
#define T_SEL_RELEASED "  SELECT released"
#define T_SEL_WARN_RESET "WARN  Release SELECT now."
#define T_SEL_WARN_RESET_DETAIL \
  "  held 10 s it resets the device to factory settings"
#define T_SEL_PASS "PASS  SELECT works."
#define T_SEL_PASS_DETAIL "  pressed for %s ms"
#define T_SEL_AFTER "Outside this test, a press goes back to Booster."
#define T_SEL_STUCK "FAIL  SELECT reads pressed all the time."
#define T_SEL_STUCK_DETAIL "  pressed from the start, %s ms"
#define T_SEL_STUCK_ADVICE \
  "Check the button is not stuck down; if it is not, the button is faulty."
#define T_SEL_NONE "FAIL  No SELECT press was seen."
#define T_SEL_NONE_DETAIL "  waited %u s: %s"
#define T_SEL_NONE_ADVICE \
  "Press SELECT firmly and test again; if it still fails, the button or " \
  "its contacts are faulty."

#define T_LED_TITLE "Green LED"
#define T_LED_STARTING "Starting the radio chip, which drives the LED."
#define T_LED_CHIP_STARTED "  radio chip started in %s ms"
#define T_LED_CHIP_HEAP "  it took %s bytes of memory"
#define T_LED_ASK_ON "Is the green LED on? [Y/N]"
#define T_LED_ASK_OFF "Is the green LED off now? [Y/N]"
#define T_LED_ASK_BLINK "Is the green LED blinking? [Y/N]"
#define T_LED_ANSWER_YES "  yes"
#define T_LED_ANSWER_NO "  no"
#define T_LED_PASS "PASS  The green LED works."
#define T_LED_FAIL "FAIL  The LED does not do what it is told."
#define T_LED_FAIL_DETAIL "  on: %s, off: %s, blinking: %s"
#define T_LED_FAIL_ADVICE \
  "The radio chip answers, so the LED or its wiring on the Pico W is " \
  "faulty."
#define T_LED_NO_CHIP "FAIL  The radio chip does not answer."
#define T_LED_NO_CHIP_DETAIL "  error %s"
#define T_LED_NO_CHIP_ADVICE \
  "The Pico W's radio chip is faulty: Wi-Fi and Bluetooth will fail too."
#define T_LED_CHIP_ERROR "FAIL  The radio chip did not take the LED."
#define T_LED_CHIP_ERROR_DETAIL "  %s, error %s"
#define T_LED_CHIP_ERROR_ADVICE \
  "The Pico W's radio chip is faulty: Wi-Fi and Bluetooth may fail too."

#define T_CARD_TITLE "microSD card"
#define T_CARD_ASK \
  "Starting the card, then writing, reading back and deleting one file."
#define T_CARD_TYPE "  Card      %s, %s MB, bus %s kHz"
#define T_CARD_MAKER "  Maker     %s %s %s, rev %s, made %s"
#define T_CARD_SERIAL "  Serial    %s, started in %s ms"
#define T_CARD_FORMAT "  Format    %s, %s MB, %s MB free"
#define T_CARD_LABEL "  Label     %s"
#define T_CARD_APPS_FOUND "  Booster's apps folder is on the card."
#define T_CARD_APPS_MISSING "  Booster's apps folder is not on the card."
#define T_CARD_WRITING "  Writing the test file %s..."
#define T_CARD_READING "  Reading it back..."
#define T_CARD_SPEED "  Write     %s KB/s, read %s KB/s"
#define T_CARD_PASS "PASS  The microSD card works."
#define T_CARD_NONE "FAIL  No microSD card answers."
#define T_CARD_NONE_DETAIL "  none in the slot, or it does not answer"
#define T_CARD_NONE_ADVICE \
  "Push a card fully into the slot and test again; if one is in, try " \
  "another card: if none answers, the slot is faulty."
#define T_CARD_OLD "FAIL  The card is too old for the device."
#define T_CARD_OLD_DETAIL "  a standard-capacity card (2 GB or less)"
#define T_CARD_OLD_ADVICE "Use an SDHC or SDXC card."
#define T_CARD_START "FAIL  The card answered, then failed to start."
#define T_CARD_START_ADVICE \
  "Try another card; if every card fails, the slot or its wiring is faulty."
#define T_CARD_NO_FS "FAIL  The card has no filesystem the device reads."
#define T_CARD_FS_DETAIL "  filesystem error %s"
#define T_CARD_NO_FS_ADVICE \
  "Format it as FAT32 or exFAT on a computer (this erases it), then test " \
  "again."
#define T_CARD_BAD_FS "FAIL  The card's filesystem cannot be read."
#define T_CARD_BAD_FS_ADVICE \
  "Check the card on a computer: it may be damaged. Then try another card."
#define T_CARD_FULL "FAIL  The card is full."
#define T_CARD_FULL_DETAIL "  %s MB free; the test needs %u KB"
#define T_CARD_FULL_ADVICE "Free some space on the card, then test again."
#define T_CARD_LOCKED "FAIL  The card refuses to be written."
#define T_CARD_LOCKED_DETAIL "  write protected"
#define T_CARD_LOCKED_ADVICE \
  "The card is locked or worn out: try another card."
#define T_CARD_WRITE "FAIL  Writing the test file failed."
#define T_CARD_WRITE_DETAIL "  %s, after %s KB, error %s"
#define T_CARD_WRITE_ADVICE \
  "Try another card; if every card fails, the slot or its wiring is faulty."
#define T_CARD_DIFFERS "FAIL  The card gave back other data than written."
#define T_CARD_DIFFERS_DETAIL "  at byte %s: read %s, written %s"
#define T_CARD_DIFFERS_ADVICE \
  "The card or the slot's wiring corrupts data: try another card; if " \
  "every card fails, the slot is faulty."
#define T_CARD_LEFT "WARN  The test file could not be deleted."
#define T_CARD_LEFT_DETAIL "  delete %s from the card on a computer"
#define T_CARD_SLOW "WARN  The card is slow."
#define T_CARD_SLOW_DETAIL "  under %u KB/s"
#define T_CARD_SLOW_ADVICE \
  "Apps will load slowly from it; a faster card is better."

#define T_SWAP_TITLE "microSD card out and back in"
#define T_SWAP_LOOKING "Looking for the card..."
#define T_SWAP_OUT "Take the microSD card out of the device (%u s)."
#define T_SWAP_EMPTY "  No card in the slot."
#define T_SWAP_IS_OUT "  The card is out."
#define T_SWAP_IN "Put the card back in (%u s)."
#define T_SWAP_IN_FIRST "Put a microSD card in (%u s)."
#define T_SWAP_BACK "  A card is in; starting it."
#define T_SWAP_PASS "PASS  The slot sees a card leave and come back."
#define T_SWAP_NOT_OUT "FAIL  The card was not seen leaving."
#define T_SWAP_WAITED "  waited %u s"
#define T_SWAP_NOT_OUT_ADVICE \
  "Take the card fully out and test again; if it was out, the slot may " \
  "be faulty."
#define T_SWAP_NOT_IN "FAIL  No card was seen coming in."
#define T_SWAP_NOT_IN_ADVICE \
  "Push the card fully in and test again; if it is in, run the microSD " \
  "card test to see why it does not answer."
#define T_SWAP_UNUSABLE "FAIL  The card came back but cannot be used."
#define T_SWAP_UNUSABLE_ADVICE "Run the microSD card test to see why."

/* The guided run ([A]) and its summary. */
#define T_MENU_RUN_ALL "[A] Run all tests, guided"
#define T_RUN_TITLE "All tests, in order"
#define T_RUN_CHOOSE \
  "[Q] Quick, about %lu min, or [T] thorough, about %lu min? [Esc] back"
#define T_RUN_STOP_HINT "A key between tests stops the run."
#define T_RUN_OVERRIDDEN "SKIP  Line test: gate overridden (debug build)."
#define T_RUN_GATE_LINES "The run stops here: the line test failed."
#define T_RUN_GATE_CHANNEL "The run stops here: the device takes no commands."
#define T_RUN_SKIP_RADIO \
  "SKIP  %s: the radio chip does not answer (above)."
#define T_RUN_ASK_HANDS \
  "Now the tests that need you: SELECT, the LED, the card out and in? [Y/N]"
#define T_RUN_STOPPED "The run was stopped."
#define T_RUN_TOOK "The run took %lu s."

#define T_SUM_TITLE "Summary"
#define T_SUM_AREA "  %-13s %s"
#define T_SUM_FINDINGS "What it means"
#define T_SUM_FINDING "* %s"
#define T_SUM_ADVICE "  %s"
#define T_AREA_LINES "Lines"
#define T_AREA_COMMANDS "Commands"
#define T_AREA_DEVICE "Device"
#define T_AREA_CARD "microSD card"
#define T_AREA_WIFI "Wi-Fi"
#define T_AREA_BLUETOOTH "Bluetooth"
#define T_AREA_SELECT "SELECT"
#define T_AREA_LED "Green LED"
#define T_AREA_CARD_SWAP "Card out/in"
#define T_OUT_PASS "PASS"
#define T_OUT_WARN "WARN"
#define T_OUT_FAIL "FAIL"
#define T_OUT_SKIP "SKIP"
#define T_OUT_LOCKED "LOCKED"
#define T_OUT_NOT_RUN "not run"
#define T_OUT_STOPPED "stopped"

#define T_C_ALL_GOOD "Everything tested works."
#define T_A_ALL_GOOD "No action needed."
#define T_C_HANDS_NOT_RUN "SELECT, the LED and the card slot were not tested."
#define T_A_HANDS_NOT_RUN "Run all again and answer Y to test them too."
#define T_C_STOPPED "The run was stopped before the end."
#define T_A_STOPPED "Run it again to the end for a full answer."
#define T_C_LINES "The device and the computer do not connect cleanly."
#define T_A_LINES \
  "The line test's finding says where. Switch off, seat the device firmly " \
  "and test again; if another computer fails the same way, the device " \
  "needs repair."
#define T_C_CMD_CACHE \
  "Commands fail on this Mega STE at 16 MHz with the cache on."
#define T_A_CMD_CACHE "Switch the cache off or run at 8 MHz, then test again."
#define T_C_CMD "The device reads well but takes no commands."
#define T_A_CMD \
  "Put this program and the firmware on the same release; if it stays, " \
  "send the report to SidecarTridge."
#define T_C_LOST "The device stopped answering during a test."
#define T_A_LOST "Check the device is seated and powered, then test again."
#define T_C_DEV_CRASH "The firmware has restarted by itself."
#define T_A_DEV_CRASH \
  "Note what was running when it did, and send the report to SidecarTridge."
#define T_C_DEV_TEMP "The device's chip runs outside 0-70 C."
#define T_A_DEV_TEMP "Check nothing heats the device, then test again."
#define T_C_DEV_SETTINGS "Booster's settings cannot be read."
#define T_A_DEV_SETTINGS "Open Booster and save its settings again."
#define T_C_CARD_NONE "No microSD card answers."
#define T_A_CARD_NONE \
  "Push a card in until it clicks; if a good card is never seen, the slot " \
  "is faulty."
#define T_C_CARD_START "A microSD card answers but does not start."
#define T_A_CARD_START \
  "Try another card; if none starts, the slot or its lines are faulty."
#define T_C_CARD_OLD "The microSD card is too old for the device."
#define T_A_CARD_OLD "Use an SDHC or SDXC card."
#define T_C_CARD_FORMAT "The microSD card has no filesystem the device reads."
#define T_A_CARD_FORMAT \
  "Format it as FAT32 or exFAT on a computer; that erases it."
#define T_C_CARD_FULL "The microSD card is full."
#define T_A_CARD_FULL "Free some space on it."
#define T_C_CARD_LOCKED "The microSD card refuses to be written."
#define T_A_CARD_LOCKED "Check the card's lock switch, or its adapter."
#define T_C_CARD_WRITE "The microSD card does not keep data."
#define T_A_CARD_WRITE "Copy your files off it and replace the card."
#define T_C_CARD_SLOW "The microSD card is slow."
#define T_A_CARD_SLOW "A faster card makes Booster's downloads quicker."
#define T_C_CHIP "The radio chip does not answer: no Wi-Fi, Bluetooth or LED."
#define T_A_CHIP \
  "Switch the computer off and on; if it stays silent, the Pico W's radio " \
  "is faulty."
#define T_C_WIFI_NONE \
  "Wi-Fi is not set up to join a network: only the radio was tested."
#define T_A_WIFI_NONE "Set up Wi-Fi in Booster to test the connection too."
#define T_C_WIFI_NOT_FOUND "Booster's Wi-Fi network is not in range."
#define T_A_WIFI_NOT_FOUND \
  "Bring the access point closer; the device needs a 2.4 GHz network that " \
  "shows its name."
#define T_C_WIFI_JOIN "The device cannot join the Wi-Fi network."
#define T_A_WIFI_JOIN \
  "Check the password in Booster, and that the router hands out addresses."
#define T_C_WIFI_NET "Wi-Fi works, but the internet is not reached."
#define T_A_WIFI_NET "Check the router's internet connection and its DNS."
#define T_C_WIFI_WEAK "The Wi-Fi signal is weak."
#define T_A_WIFI_WEAK "Bring the device and the access point closer."
#define T_C_BT_HALF \
  "Wi-Fi works but Bluetooth does not start: the chip's Bluetooth side is " \
  "faulty."
#define T_C_BT_START "Bluetooth does not start."
#define T_A_BT_START \
  "Switch the computer off and on; if it stays, the Pico W's radio is " \
  "faulty."
#define T_C_BT_SCAN "Bluetooth starts but cannot scan."
#define T_A_BT_SCAN \
  "Test again; if it keeps failing, the chip's Bluetooth side is faulty."
#define T_C_BT_NOTHING "No Bluetooth device was heard."
#define T_A_BT_NOTHING "Make a phone nearby visible, to check the scan."
#define T_C_SEL_NONE "The SELECT button was not seen."
#define T_A_SEL_NONE \
  "Press it firmly; if it is never seen, the button or its line is faulty."
#define T_C_SEL_STUCK "The SELECT button reads pressed all the time."
#define T_A_SEL_STUCK \
  "Free the button: held 10 s at power-on, it resets the device's settings."
#define T_C_LED "The radio chip works but the green LED did not do what it was told."
#define T_A_LED \
  "The LED or its path on the Pico W is faulty; nothing else needs it."
#define T_C_SWAP "The card slot does not see a card leave or come back."
#define T_A_SWAP "Clean the slot and try again; if it stays, it is faulty."
#define T_C_UNKNOWN "No single cause fits these results."
#define T_A_UNKNOWN "Save the report and send it to SidecarTridge."

#define T_BT_TITLE "Bluetooth"
#define T_BT_STARTING "Starting the Bluetooth controller..."
#define T_BT_CLASSIC "Listening for Bluetooth devices (%u s)..."
#define T_BT_LE "Listening for low-energy devices (%u s)..."
#define T_BT_CONTROLLER "  Bluetooth %s, address %s, started in %s ms"
#define T_BT_FIRMWARE "  Firmware  %s, maker %s"
#define T_BT_HEARD "  Devices   %s%s heard: %s classic, %s low energy"
#define T_BT_DEVICE "  %4s dBm %-7s %s %s"
#define T_BT_PASS "PASS  Bluetooth works."
#define T_BT_NOTHING "WARN  No Bluetooth device was heard."
#define T_BT_NOTHING_ADVICE \
  "Make a phone nearby visible in its Bluetooth settings, then test again."
#define T_BT_CHIP_FAIL "FAIL  The radio chip does not answer."
#define T_BT_CHIP_FAIL_DETAIL "  error %s when started"
#define T_BT_CHIP_FAIL_ADVICE \
  "Switch the computer off and on; if it stays silent, the Pico W's radio " \
  "is faulty."
#define T_BT_START_FAIL "FAIL  The Bluetooth controller does not start."
#define T_BT_START_FAIL_DETAIL "  %s"
#define T_BT_START_FAIL_ADVICE \
  "Run the Wi-Fi test: if it works, the chip's Bluetooth side is faulty."
#define T_BT_SCAN_FAIL "FAIL  The Bluetooth controller cannot scan."
#define T_BT_SCAN_FAIL_DETAIL "  %s, error %s"
#define T_BT_SCAN_FAIL_ADVICE \
  "Test again; if it keeps failing, the chip's Bluetooth side is faulty."

#define T_WIFI_TITLE "Wi-Fi"
#define T_WIFI_CHIP "Starting the radio chip..."
#define T_WIFI_SCAN "Listening for networks..."
#define T_WIFI_JOIN "Joining the network set up in Booster..."
#define T_WIFI_ADDRESS "  Joined; asking for an address..."
#define T_WIFI_ROUTER "Asking the router..."
#define T_WIFI_DNS "Looking up %s..."
#define T_WIFI_WEB "Asking %s for a page..."
#define T_WIFI_RADIO "  Radio     MAC %s, started in %s ms"
#define T_WIFI_FIRMWARE "  Firmware  %s, country %s"
#define T_WIFI_COUNTRY "  Country   %s"
#define T_WIFI_HEARD "  Networks  %s heard, %s hidden"
#define T_WIFI_NET "  %4s dBm ch %-2s %-4s %s"
#define T_WIFI_SEEN "  Booster's network %s, %s dBm (%s)"
#define T_WIFI_NOT_SEEN "  Booster's network %s, not heard"
#define T_WIFI_JOINED "  Joined    in %s ms, signal %s dBm (%s)"
#define T_WIFI_ADDR "  Address   %s, mask %s"
#define T_WIFI_GATEWAY "  Router    %s, DNS %s, %s"
#define T_WIFI_ROUTER_OK "  Router    answered in %s us"
#define T_WIFI_DNS_OK "  Name      %s is %s, %s ms"
#define T_WIFI_WEB_OK "  Web       answered %s in %s ms"
#define T_WIFI_PASS "PASS  Wi-Fi works, out to the internet."
#define T_WIFI_NOT_SET_UP "WARN  Wi-Fi is not set up to join a network."
#define T_WIFI_NOT_SET_UP_DETAIL \
  "  the radio works and hears %s networks; joining and the internet " \
  "were not tested"
#define T_WIFI_NOT_SET_UP_ADVICE \
  "Set up Wi-Fi in Booster to test the connection too."
#define T_WIFI_WEAK "WARN  The signal is weak."
#define T_WIFI_WEAK_DETAIL "  %s dBm"
#define T_WIFI_WEAK_ADVICE \
  "Move the device and the access point closer, or away from metal."
#define T_WIFI_STATIC "WARN  Booster's fixed address was refused."
#define T_WIFI_STATIC_DETAIL "  %s; DHCP used instead"
#define T_WIFI_STATIC_ADVICE \
  "Check the address, mask and router in Booster's Wi-Fi settings."
#define T_WIFI_NOTHING "WARN  No network was heard at all."
#define T_WIFI_NOTHING_ADVICE \
  "Check the antenna end of the device is not covered or near metal."
#define T_WIFI_CHIP_FAIL "FAIL  The radio chip does not answer."
#define T_WIFI_CHIP_FAIL_DETAIL "  no answer when started"
#define T_WIFI_CHIP_FAIL_ADVICE \
  "Switch the computer off and on; if it stays silent, the Pico W's radio " \
  "is faulty."
#define T_WIFI_SCAN_FAIL "FAIL  The radio cannot scan for networks."
#define T_WIFI_SCAN_FAIL_DETAIL "  error %s"
#define T_WIFI_SCAN_FAIL_ADVICE \
  "Test again; if it keeps failing, the radio chip is faulty."
#define T_WIFI_NONE "FAIL  No Wi-Fi network is set up."
#define T_WIFI_NONE_DETAIL "  Booster has no network name"
#define T_WIFI_NONE_ADVICE "Set up Wi-Fi in Booster, then test again."
#define T_WIFI_NOT_FOUND "FAIL  Booster's network is not in range."
#define T_WIFI_NOT_FOUND_DETAIL "  %s not found"
#define T_WIFI_NOT_FOUND_ADVICE \
  "Check the access point is on, near, on 2.4 GHz (the device has no " \
  "5 GHz) and shows its name."
#define T_WIFI_PASSWORD "FAIL  The network refused the password."
#define T_WIFI_PASSWORD_DETAIL "  %s: wrong password"
#define T_WIFI_PASSWORD_ADVICE "Enter the Wi-Fi password again in Booster."
#define T_WIFI_JOIN_FAIL "FAIL  The device could not join the network."
#define T_WIFI_JOIN_FAIL_DETAIL "  %s: %s"
#define T_WIFI_JOIN_FAIL_ADVICE \
  "Check the access point takes new devices with WPA2, then test again."
#define T_WIFI_NO_ADDRESS "FAIL  The network gave the device no address."
#define T_WIFI_NO_ADDRESS_DETAIL "  joined; DHCP did not answer"
#define T_WIFI_NO_ADDRESS_ADVICE \
  "Check the router hands out addresses, or set one in Booster."
#define T_WIFI_ROUTER_FAIL "FAIL  The router does not answer."
#define T_WIFI_ROUTER_FAIL_DETAIL "  router %s"
#define T_WIFI_ROUTER_FAIL_ADVICE \
  "Check the router address in Booster, or restart the router."
#define T_WIFI_DNS_FAIL "FAIL  Names cannot be looked up."
#define T_WIFI_DNS_FAIL_DETAIL "  %s through DNS %s: %s"
#define T_WIFI_DNS_FAIL_ADVICE \
  "Check the router reaches the internet, and the DNS server in Booster."
#define T_WIFI_WEB_FAIL "FAIL  The internet does not answer."
#define T_WIFI_WEB_FAIL_DETAIL "  %s at %s: %s"
#define T_WIFI_WEB_FAIL_ADVICE \
  "Check the router reaches the internet; a firewall may block the device."
#define T_WIFI_MEMORY "FAIL  The device ran out of memory."
#define T_WIFI_MEMORY_DETAIL "  %s"
#define T_WIFI_MEMORY_ADVICE "Save the report and send it to SidecarTridge."

#define T_EXIT_TITLE "Back to Booster"
#define T_EXIT_DONE "The device is restarting into Booster."
#define T_EXIT_ADVICE \
  "Reset the computer, or switch it off and on, to use Booster."
#define T_EXIT_FAIL "FAIL  The device did not answer."
#define T_EXIT_FAIL_ADVICE "Press SELECT on the device to go back to Booster."
#define T_EXIT_QUIT "Press a key to restart the computer into Booster."

#endif
