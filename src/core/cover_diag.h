/* Isolated BDISO end-to-end diagnostics (no cover algorithms modified).
 * The report is truncated once per application launch. Logs are flushed
 * immediately, surviving a hang or power-off. */
#ifndef SF_COVER_DIAG_H
#define SF_COVER_DIAG_H
int cover_diag_start(const char *path);
void cover_diag_stop(void);
void cover_diag(const char *stage,const char *fmt,...)
#if defined(__GNUC__)
 __attribute__((format(printf,2,3)))
#endif
 ;
#endif
