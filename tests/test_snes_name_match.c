/* SNES official boxart matcher: no fuzzy guess, no ROM downloads. */
#include "snes_name_match.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static int checks=0;
#define CHECK(x) do {if(!(x)){fprintf(stderr,"SNES matcher FAIL line %d: %s\n",__LINE__,#x);exit(1);}checks++;}while(0)
static const char *const index_names[]={
    "Chrono Trigger (Japan).png",
    "Chrono Trigger (USA).png",
    "Donkey Kong Country (USA).png",
    "Donkey Kong Country 2 - Diddy's Kong Quest (USA) (En,Fr).png",
    "Donkey Kong Country 3 - Dixie Kong's Double Trouble! (USA) (En,Fr).png",
    "Doom (USA).png",
    "Doom Troopers (USA).png",
    "Final Fantasy III (USA).png",
    "Legend of Zelda, The - A Link to the Past (USA).png",
    "Super Mario Kart (USA).png",
    "Super Mario RPG - Legend of the Seven Stars (USA).png",
    "Super Mario World (Europe).png",
    "Super Mario World (Japan).png",
    "Super Mario World (USA) (Rev 1).png",
    "Super Mario World (USA).png",
    "Super Mario World 2 - Yoshi's Island (USA).png",
};
static char index_buf[8192];
static void lookup(const char *rom,const char *expect)
{
    char output[256];
    int found=snes_name_catalog_match(rom,index_buf,strlen(index_buf),output,sizeof(output));
    if(expect){if(!found||strcmp(output,expect)!=0)fprintf(stderr,"Mismatch ROM=%s\n wanted=%s\n actual=%s found=%d\n",rom,expect,output,found);CHECK(found);CHECK(strcmp(output,expect)==0);}
    else CHECK(!found);
}
int main(void)
{
    size_t i;char url[512];
    for(i=0;i<sizeof(index_names)/sizeof(index_names[0]);i++){
        strcat(index_buf,index_names[i]);strcat(index_buf,"\n");
    }
    lookup("/dev_hdd0/SNES/001 - Super Mario World.sfc","Super Mario World (USA).png");
    lookup("/dev_hdd0/SNES/200_Super Mario World (USA) [!].smc","Super Mario World (USA).png");
    lookup("/dev_usb000/SNES/Super Mario World (Europe).zip","Super Mario World (Europe).png");
    lookup("/dev_usb000/SNES/Super Mario World (USA) (Rev 1).smc","Super Mario World (USA) (Rev 1).png");
    lookup("/dev_hdd0/SNES/Donkey Kong Country 2.sfc","Donkey Kong Country 2 - Diddy's Kong Quest (USA) (En,Fr).png");
    lookup("/dev_hdd0/SNES/Donkey Kong Country 3.zip","Donkey Kong Country 3 - Dixie Kong's Double Trouble! (USA) (En,Fr).png");
    lookup("/dev_hdd0/SNES/Super Mario RPG.sfc","Super Mario RPG - Legend of the Seven Stars (USA).png");
    lookup("/dev_hdd0/SNES/The Legend of Zelda - A Link to the Past (USA).sfc","Legend of Zelda, The - A Link to the Past (USA).png");
    lookup("/dev_hdd0/SNES/Yoshi's Island.zip","Super Mario World 2 - Yoshi's Island (USA).png");
    lookup("/dev_hdd0/SNES/012 Doom (USA).smc","Doom (USA).png");
    lookup("/dev_hdd0/SNES/Chrono Trigger.sfc","Chrono Trigger (USA).png");
    lookup("/dev_hdd0/SNES/Chrono Trigger (Japan).zip","Chrono Trigger (Japan).png");
    lookup("/dev_hdd0/SNES/Super Mario Kart (USA).smc","Super Mario Kart (USA).png");
    lookup("/dev_hdd0/SNES/Super Mario World [MOD].smc",NULL);
    lookup("/dev_hdd0/SNES/Super Mario World - Kaizo Hack.smc",NULL);
    lookup("/dev_hdd0/SNES/Doom Troopers.smc","Doom Troopers (USA).png");
    lookup("/dev_hdd0/SNES/Not in the catalog.sfc",NULL);
    CHECK(!snes_name_catalog_url("../../unverified.png",url,sizeof(url)));
    CHECK(!snes_name_catalog_url("unknown.sfc",url,sizeof(url)));
    CHECK(snes_name_catalog_url("Donkey Kong Country 2 - Diddy's Kong Quest (USA) (En,Fr).png",url,sizeof(url)));
    CHECK(strstr(url,"Nintendo%20-%20Super%20Nintendo%20Entertainment%20System")!=NULL);
    CHECK(strstr(url,"%27")!=NULL);
    CHECK(strstr(url,"%20")!=NULL);
    CHECK(!snes_name_catalog_url("Super Mario World (USA).png",url,30));
    CHECK(!snes_name_catalog_match("/dev_hdd0/SNES/Doom.sfc",NULL,0,url,sizeof(url)));
    puts("SNES official-cover catalog matcher: all checks passed");
    printf("Assertions: %d\n",checks);
    return 0;
}
