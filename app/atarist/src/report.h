/*
 * The report: every line the program wrote, saved as plain text next to the
 * program, REPORT00.TXT to REPORT99.TXT, a new file each time.
 */
#ifndef REPORT_H
#define REPORT_H

/* Save the report, its first line after the format the gate's state;
 * prints the outcome. */
void report_save(const char *gate);

#endif
