/* PS2 mount preflight: pure C, independent of PS3 syscalls. GPL-3.0-or-later. */
#ifndef SF_PS2_MOUNT_PLAN_H
#define SF_PS2_MOUNT_PLAN_H
#include <stddef.h>
#define SF_PS2_MAX_PARTS 64
#define SF_PS2_MAX_PATH 1024
typedef int (*SfPs2FileExists)(void *ctx, const char *path);
/* No remapping of arbitrary paths: only internal PS2ISO .iso/.iso.0 files. */
int sf_ps2_valid_iso_path(const char *path);
/* Returns contiguous count, or negative on unsupported path/missing image.
   Destination must hold SF_PS2_MAX_PARTS arrays of SF_PS2_MAX_PATH bytes. */
int sf_ps2_collect_parts(const char *path, SfPs2FileExists exists, void *ctx,
                         char paths[SF_PS2_MAX_PARTS][SF_PS2_MAX_PATH]);
#endif
