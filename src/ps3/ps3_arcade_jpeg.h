#ifndef SF_PS3_ARCADE_JPEG_H
#define SF_PS3_ARCADE_JPEG_H
#include <stdint.h>
/* Arcade-only: decode PNG/JPEG thumbnail and encode compact baseline JPEG
 * 320x448 max canvas, preserving the original image aspect ratio.
 * Returns a malloc'd JPEG <=180 KB, caller must free().
 */
int ps3_arcade_jpeg_compact(void *ctx,const uint8_t *original,uint32_t n,
                            uint8_t **out,uint32_t *out_len);
#endif
