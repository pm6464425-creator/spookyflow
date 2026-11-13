/* GPL-3.0-or-later. Conservative PS2 disc identification. */
#include "sf_ps2_media.h"
#include <string.h>
const char *sf_ps2_region(const char *id)
{
    if(!id || strlen(id)<4)return "DESCONOCIDA";
    if(!memcmp(id,"SLUS",4) || !memcmp(id,"SCUS",4) ||
       !memcmp(id,"NPUD",4) || !memcmp(id,"NPUC",4))return "NTSC-U";
    if(!memcmp(id,"SLES",4) || !memcmp(id,"SCES",4) ||
       !memcmp(id,"SCED",4) || !memcmp(id,"SLED",4) ||
       !memcmp(id,"NPED",4))return "PAL";
    if(!memcmp(id,"SLPM",4) || !memcmp(id,"SLPS",4) ||
       !memcmp(id,"SCPS",4) || !memcmp(id,"SCAJ",4) ||
       !memcmp(id,"SLAJ",4) || !memcmp(id,"NPJD",4) ||
       !memcmp(id,"NPJC",4) || !memcmp(id,"PAPX",4))return "NTSC-J";
    if(!memcmp(id,"SCKA",4) || !memcmp(id,"SLKA",4) ||
       !memcmp(id,"SCCS",4))return "ASIA";
    return "DESCONOCIDA";
}
int sf_ps2_is_known_cooked_cd(const char *id)
{
    /* Verified PS2 CD-ROM editions: Rumble Racing North America and Europe.
     * These titles are often converted from raw BIN/CUE to cooked 2048 ISO.
     * Never guess disc type for unrelated ISO by size: 2048 may be DVD. */
    return id && (!strcmp(id,"SLES_501.20") || !strcmp(id,"SLUS_201.74"));
}
int sf_ps2_select_image_mode(const char *id,int has_2048_pvd,int has_2352_pvd)
{
    if(has_2352_pvd) return SF_PS2_IMAGE_CD_RAW_2352;
    if(has_2048_pvd) return sf_ps2_is_known_cooked_cd(id)?
        SF_PS2_IMAGE_CD_COOKED_2048:SF_PS2_IMAGE_DVD_2048;
    return 0;
}
