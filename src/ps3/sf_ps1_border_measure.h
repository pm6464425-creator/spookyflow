#ifndef SF_PS1_BORDER_MEASURE_H
#define SF_PS1_BORDER_MEASURE_H
#include <stddef.h>
typedef struct SfPs1BorderResult {
    unsigned width,height,top,bottom,left,right;
    unsigned active_width,active_height;
    unsigned central_nonblack_permille;
    unsigned ambiguous, valid;
} SfPs1BorderResult;
/* 4-byte-per-pixel packed ARGB or RGBA; pitch in bytes.
 * Detect near-black uniform bands ONLY. No source-of-border claim.
 * 0=image measured, -1=bad stride/dimensions, -2=all-black/ambiguous.
 */
int sf_ps1_analyze_rgba(const unsigned char *data,unsigned w,unsigned h,
                        unsigned pitch,SfPs1BorderResult *out);
#endif
