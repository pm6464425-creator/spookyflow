#ifndef SF_PS1_MOUNT_H
#define SF_PS1_MOUNT_H
#include "sf_ps1_native_plan.h"
/* Native Cobra/HEN mount for a single 2352-byte sector PS1 image.
 * CUE with multiple tracks in one BIN is supported, other formats
 * return -41 to preserve the original webMAN fallback. */
int sf_ps1_mount(const char *path,int emulator);
/* Mount a completed cache-only merged image with its exact audio track table. */
int sf_ps1_mount_staged(const char *path,const SfPs1Track *tracks,unsigned n);
#endif
