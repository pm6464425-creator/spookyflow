/* SpookyFlow PS2 Classics HEN backend (GPL-3.0-or-later). */
#ifndef SF_PS2_HEN_H
#define SF_PS2_HEN_H
/* Start an asynchronous conversion/staging of a PS2 ISO on internal HDD.
 * Returns 0 if started, 1 if a matching, previously prepared image is ready,
 * or a negative error code. Never modifies the source ISO.
 */
int sf_ps2_hen_start(const char *path,const char *gameid);
/* Read-only PS2 Classics encrypted image, CONFIG and LIMG inspection.
 * Logs to the one app-wide diagnostics report; never edits PS2 games. */
void sf_ps2_hen_report_state(void);
/* 0=in progress; 1=fully prepared; negative=failed. Percent 0..100. */
int sf_ps2_hen_poll(int *percent);
/* Signal cancellation (on an OS-forced exit, a .sf-part can remain). */
void sf_ps2_hen_cancel(void);
#endif
