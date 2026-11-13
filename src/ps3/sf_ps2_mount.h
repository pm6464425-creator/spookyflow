/* Independent PS2 Cobra ISO mount (CFW only). No webMAN HTTP or plugin needed. */
#ifndef SF_PS2_MOUNT_H
#define SF_PS2_MOUNT_H
/* Preflight is read-only, must succeed BEFORE PS2 CONFIG is modified. */
int sf_ps2_preflight(const char *path);
/* 0 success, -41 PS3HEN (requires encrypted PS2 Classics), other codes errors. */
int sf_ps2_mount(const char *path, int emulator);
#endif
