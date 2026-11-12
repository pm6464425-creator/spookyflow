#ifndef PS3_JPEG_SOFT_H
#define PS3_JPEG_SOFT_H
#include <stdint.h>
/* Decode baseline/progressive JPEG into malloc-owned RGBA.
 * Caller must free(*pixels); max dimensions are checked before decoding. */
int ps3_jpeg_soft_decode(const unsigned char *bytes,uint32_t count,
                         int max_dim,int *w,int *h,unsigned char **pixels);
#endif
