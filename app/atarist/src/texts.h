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

#define T_MENU_LINE "[L] Line test: address, data, select"
#define T_MENU_LINE_OFF "[ ] Line test: needs the test firmware"
#define T_MENU_LOCKED1 "    Wi-Fi, microSD, Bluetooth, SELECT"
#define T_MENU_LOCKED2 "    and LED: after the line test"
#define T_MENU_SAVE "[S] Save report"
#define T_MENU_QUIT "[Q] Quit"
#define T_MENU_PROMPT "Press a key."

#define T_LINE_SKIP "SKIP  The line test is not in this build."

#define T_SAVED "Report saved as %s."
#define T_SAVE_FAILED "WARN  The report could not be saved."
#define T_SAVE_FAILED_DETAIL "  %s: GEMDOS error %ld"
#define T_SAVE_ADVICE "Check the disk is not write-protected or full."
#define T_SAVE_NONAME "  no free name REPORT00.TXT to REPORT99.TXT"

#define T_REPORT_FORMAT "MDTEST REPORT 1"

#endif
