#ifndef SF_PS1_NATIVE_PLAN_H
#define SF_PS1_NATIVE_PLAN_H
#include <stdint.h>
#include <stddef.h>
#define SF_PS1_MAX_TRACKS 99
#define SF_PS1_IMAGE_MAX 1024
typedef struct { uint32_t lba; int is_audio; } SfPs1Track;
typedef struct {
    char image[SF_PS1_IMAGE_MAX];
    SfPs1Track tracks[SF_PS1_MAX_TRACKS];
    unsigned num_tracks;
} SfPs1NativePlan;
/* Strict single-file 2352-sector .CUE parser. Return 0 only for safe, complete
 * layouts. Other layouts must use the existing webMAN backend, never silently
 * lose audio or mount an incorrect source. Paths are absolute; no traversal. */
int sf_ps1_plan_cue(const char *cue_path,const char *text,size_t length,
                    SfPs1NativePlan *out);
#endif
