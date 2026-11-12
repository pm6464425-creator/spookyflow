/* The PS3 hardware JPGDEC may reject progressive JPEGs supplied by modern
 * poster CDNs. This software fallback decodes the same bytes from NTFS RAM,
 * without a proxy, internet round-trip, HDD write or image-format change.
 * stb_image v2.31 is public domain / MIT dual-licensed.
 */
#define STBI_ONLY_JPEG
#define STBI_NO_STDIO
#define STBI_NO_HDR
#define STBI_NO_LINEAR
#define STBI_FAILURE_USERMSG
#define STBI_ASSERT(x) ((void)0)
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#include "ps3_jpeg_soft.h"
#include <limits.h>
#include <stdlib.h>

int ps3_jpeg_soft_decode(const unsigned char *bytes,uint32_t count,
                         int max_dim,int *w,int *h,unsigned char **pixels)
{
    int iw=0,ih=0,components=0;
    unsigned char *rgba;
    if(!bytes||count<32||count>(3u*1024u*1024u) ||
       count>INT_MAX||!w||!h||!pixels||
       max_dim<=0||max_dim>4096)return -1;
    *pixels=NULL;
    if(!stbi_info_from_memory(bytes,(int)count,&iw,&ih,&components) ||
       iw<=0||ih<=0||iw>max_dim||ih>max_dim ||
       (unsigned long long)iw*(unsigned long long)ih>16777216ULL)return -1;
    rgba=stbi_load_from_memory(bytes,(int)count,&iw,&ih,&components,4);
    if(!rgba)return -1;
    *w=iw;*h=ih;*pixels=rgba;
    return 0;
}
