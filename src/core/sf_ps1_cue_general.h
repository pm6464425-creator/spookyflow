#ifndef SF_PS1_CUE_GENERAL_H
#define SF_PS1_CUE_GENERAL_H
#include <stdint.h>
#include <stddef.h>
#include "sf_ps1_native_plan.h"
#define SF_PS1_CUE_MAX_FILES 99
#define SF_PS1_CUE_MAX_TRACKS 99
#define SF_PS1_CUE_MAX_CHUNKS 420
typedef struct {
    char path[SF_PS1_IMAGE_MAX];
} SfPs1CueFile;
typedef struct {
    uint32_t index00,index01,pregap,postgap;
    unsigned file_index;
    unsigned is_audio:1;
    unsigned has_index00:1;
    unsigned has_index01:1;
} SfPs1CueTrack;
typedef struct {
    SfPs1CueFile file[SF_PS1_CUE_MAX_FILES];
    SfPs1CueTrack track[SF_PS1_CUE_MAX_TRACKS];
    unsigned file_count,track_count;
} SfPs1CuePlan;
typedef struct {
    unsigned file_index;
    uint64_t at,bytes;
    unsigned is_silence;
} SfPs1CueChunk;
/* Any grouping of one or more 2352-sector BINARY files and 1-99
 * AUDIO/MODE1/MODE2 tracks. Multi-file CUE can contain multiple tracks
 * per file. Reject paths escaping the game's directory. */
int sf_ps1_cue_general_parse(const char *cue_path,const char *text,
                             size_t len,SfPs1CuePlan *out);
/* Preflight and plan copy layout. Chunks contain ordered source ranges and
 * synthetic silent gaps; source bytes and audio are otherwise unchanged.
 * Returns 0 only if complete unambiguous CD layout fits. */
int sf_ps1_cue_general_layout(const SfPs1CuePlan *plan,
                              const uint64_t *sizes,SfPs1Track *out_tracks,
                              SfPs1CueChunk *chunks,unsigned *nchunks,
                              uint64_t *expected_bytes);
#endif
