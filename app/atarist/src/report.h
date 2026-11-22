/*
 * The report: every line the program wrote, as plain text (80 columns, CR LF),
 * saved on the device's microSD card by its firmware, MDTEST-REPORT-00.TXT to
 * -99.TXT, a new file each time. It travels over the command channel, so it
 * can be saved only once the line test and the channel's test have passed.
 */
#ifndef REPORT_H
#define REPORT_H

/* Save the report, its first line after the format the gate's state;
 * prints the outcome. */
void report_save(const char *gate, int channelOpen, long mch);

#endif
