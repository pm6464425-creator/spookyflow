#include "sf_ps2_media.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static int n;
#define CHECK(x) do{if(!(x)){fprintf(stderr,"FAIL line %d: %s\n",__LINE__,#x);exit(1);}n++;}while(0)
int main(void)
{
    CHECK(!strcmp(sf_ps2_region("SLES_501.20"),"PAL"));
    CHECK(!strcmp(sf_ps2_region("SCES_123.45"),"PAL"));
    CHECK(!strcmp(sf_ps2_region("SLUS_201.74"),"NTSC-U"));
    CHECK(!strcmp(sf_ps2_region("SCUS_123.45"),"NTSC-U"));
    CHECK(!strcmp(sf_ps2_region("SLPM_650.01"),"NTSC-J"));
    CHECK(!strcmp(sf_ps2_region("SLPS_123.45"),"NTSC-J"));
    CHECK(!strcmp(sf_ps2_region("SCPS_123.45"),"NTSC-J"));
    CHECK(!strcmp(sf_ps2_region("SCKA_123.45"),"ASIA"));
    CHECK(!strcmp(sf_ps2_region("SLKA_123.45"),"ASIA"));
    CHECK(!strcmp(sf_ps2_region(NULL),"DESCONOCIDA"));
    CHECK(sf_ps2_is_known_cooked_cd("SLES_501.20"));
    CHECK(sf_ps2_is_known_cooked_cd("SLUS_201.74"));
    CHECK(!sf_ps2_is_known_cooked_cd("SLPM_650.01"));
    CHECK(!sf_ps2_is_known_cooked_cd(NULL));
    CHECK(sf_ps2_select_image_mode("SLES_501.20",1,0)==SF_PS2_IMAGE_CD_COOKED_2048);
    CHECK(sf_ps2_select_image_mode("SLUS_201.74",1,0)==SF_PS2_IMAGE_CD_COOKED_2048);
    CHECK(sf_ps2_select_image_mode("SLPM_650.01",1,0)==SF_PS2_IMAGE_DVD_2048);
    CHECK(sf_ps2_select_image_mode("SCES_123.45",1,0)==SF_PS2_IMAGE_DVD_2048);
    CHECK(sf_ps2_select_image_mode("SLPM_650.01",0,1)==SF_PS2_IMAGE_CD_RAW_2352);
    CHECK(sf_ps2_select_image_mode("SLES_501.20",1,1)==SF_PS2_IMAGE_CD_RAW_2352);
    CHECK(sf_ps2_select_image_mode("SLES_501.20",0,0)==0);
    printf("PS2 region and medium: %d/%d tests passed\n",n,21);
    return n==21?0:1;
}
