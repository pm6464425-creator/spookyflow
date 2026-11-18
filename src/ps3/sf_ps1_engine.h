#ifndef SF_PS1_ENGINE_H
#define SF_PS1_ENGINE_H
/* Verified self-contained APP SELF (GPL PSXonPS3).
 * Return 0 only after full SHA and signed SELF APP type-4 checks. */
int sf_ps1_engine_ready(void);
const char *sf_ps1_engine_path(void);
#endif
