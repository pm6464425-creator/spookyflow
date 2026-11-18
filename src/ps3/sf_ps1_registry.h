/* PS3 XRegistry read-only game upscaler probe. No user data is exported. */
#ifndef SF_PS1_REGISTRY_H
#define SF_PS1_REGISTRY_H
/* 0: found; -1: I/O; -2: malformed; -3: not found; -4: invalid value.
 * Any output set to -1 represents unknown. */
int sf_ps1_registry_read_modes(const char *path, int *upscaler, int *smoothing);
const char *sf_ps1_upscaler_name(int mode);
#endif
