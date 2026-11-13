#ifndef SF_PS1_MULTIBIN_H
#define SF_PS1_MULTIBIN_H
#include <stddef.h>
#include <stdint.h>
#include "sf_ps1_native_plan.h"
#define SF_PS1_MULTIBIN_MAX 99
typedef struct {
    char path[SF_PS1_IMAGE_MAX];
    uint32_t index01;
    uint32_t pregap;
    int is_audio;
    int has_index00;
} SfPs1MultiPart;
typedef struct {
    SfPs1MultiPart part[SF_PS1_MULTIBIN_MAX];
    unsigned count;
} SfPs1MultiPlan;
/* Parses one BINARY FILE per track, preserving INDEX 00/01 and PREGAP.
 * This strict separate-BIN variant is intentionally independent of PS1
 * single-BIN CUE support. Never silently drop Red Book audio tracks.
 * Returns zero for valid >=2 file CUE or negative for unsupported layouts. */
int sf_ps1_multibin_parse(const char *cue,const char *text,size_t size,
                          SfPs1MultiPlan *out);
/* Builds Cobra's track positions from the copied source byte sizes.
 * The final buffer is one raw 2352-sector image; no CD-DA data is discarded. */
int sf_ps1_multibin_layout(const SfPs1MultiPlan *plan,
                           const uint64_t *source_sizes,
                           SfPs1Track *tracks,uint64_t *total_bytes);
#endif
