#ifndef SF_PS1_RESOLVE_H
#define SF_PS1_RESOLVE_H
#include <stddef.h>
/* Resolve a PS1 .CUE's expected BIN/IMG filename from its own directory.
 * Exact file wins; otherwise only unique same-title case/region variants.
 * -1 not found, -2 ambiguous, -3 filesystem error.
 * Read-only: no renames, copies, source changes, or external web service. */
int sf_ps1_resolve_existing(const char *expected,char *out,size_t cap);
#endif
