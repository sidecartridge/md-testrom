# Changelog

## v2.0.0 (2026-11-22) - release

A new test, from the ground up: the microfirmware and one program, `MDTEST.TOS`, that test the whole Multi-device and its connection to the Atari, and say what to do about what they find.

- **One program, no data file.** `MDTEST.TOS` replaces `TESTSCRT.TOS` and `TESTROM.BIN`: it computes the test data itself. Delete the old pair; the new program recognises a device still on v0.0.3 and says to update it.
- **Every line of the cartridge port.** The line test reads both banks in nine ways and names the fault it finds: a data or address line stuck or shorted, a select line, one byte lane, a bank not answering, the banks swapped, errors that come and go. A quick pass (about 22 s on an 8 MHz ST), a long one (a million random reads), and on a Mega STE the quick pass at 8 MHz, 16 MHz and 16 MHz with the cache.
- **The device itself.** Once the line test and the command channel test pass: device information, the microSD card (write and read speeds), the card out and back in, Wi-Fi out to the internet, Bluetooth, the SELECT button and the green LED.
- **Run all tests, guided.** [A] runs everything in order, quick (about 2 minutes) or thorough (about 6), asks before the tests that need your hands, and ends with a summary: each area's verdict, what it means and what to do.
- **The report on the microSD card.** [S] saves everything on the screen as `MDTEST-REPORT-nn.TXT` on the device's card, with the Wi-Fi and Bluetooth names left out if you say so.
- **The computer boots as if no cartridge were there.** The microfirmware puts no code on the Atari.
- **Every Atari with a cartridge port:** ST, STE, Mega ST, Mega STE, TT (colour monitors) and Falcon (RGB, TV, VGA and mono). Quitting restarts the computer, so the program does not belong in the `AUTO` folder.
- **Back to Booster** from the program with [B], or with the SELECT button.
- The release has the program as `MDTEST.TOS` and as a floppy image, `MDTEST.ST`.

---

## v0.0.3 (2025-07-01) - release
- First version
