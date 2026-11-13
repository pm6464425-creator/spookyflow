#ifndef SF_PS1_JOIN_H
#define SF_PS1_JOIN_H
/* PS1 Redump multi-BIN CUE -> single image in SpookyFlow cache, PS3 HEN/CFW.
 * Return 0 processing, 1 cached and ready, negative error.
 * No changes to user's source BINs or CUE. */
int sf_ps1_join_start(const char *cue);
int sf_ps1_join_poll(int *progress);
int sf_ps1_join_mount(void);
void sf_ps1_join_cancel(void);
#endif
