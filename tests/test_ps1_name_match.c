#include "ps1_name_match.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static const char catalog[]=
    "Disney's Tarzan (Europe).png\n"
    "Disney's Tarzan (USA) (Rerelease).png\n"
    "Disney's Tarzan (USA).png\n"
    "Disney Tarzan (Spain).png\n"
    "Gran Turismo (USA).png\n"
    "Gran Turismo 2 (USA).png\n"
    "Dragon Ball GT - Final Bout (USA).png\n"
    "Resident Evil 2 (USA) (Disc 1).png\n"
    "Resident Evil 2 (USA) (Disc 2).png\n"
    "Metal Gear Solid (USA).png\n"
    "Disney's Hercules (USA).png\n"
    "Tarzan (USA) (Demo).png\n";

static void lookup(const char *file,const char *wanted)
{
    char found[256],url[512];
    int ok=ps1_name_catalog_match(file,catalog,strlen(catalog),
                                  found,sizeof(found));
    if(!wanted){assert(!ok);return;}
    if(!ok||strcmp(found,wanted)){
        fprintf(stderr,"PS1 lookup FAIL for [%s] -> [%s], expected [%s]\n",
                file,ok?found:"NONE",wanted);
        assert(ok&&strcmp(found,wanted)==0);
    }
    assert(ps1_name_catalog_url(found,url,sizeof(url)));
    assert(strstr(url,"https://thumbnails.libretro.com/Sony%20-%20PlayStation/Named_Boxarts/")==url);
    assert(strstr(url,".png")!=NULL);
}

int main(void)
{
    lookup("/dev_hdd0/PSXISO/Disney Tarzan Latino.cue",
           "Disney Tarzan (Spain).png");
    lookup("/dev_hdd0/PSXISO/Disney Tarzan Español.bin",
           "Disney Tarzan (Spain).png");
    lookup("/dev_hdd0/PSXISO/Disney's Tarzan (USA).cue",
           "Disney's Tarzan (USA).png");
    lookup("/dev_hdd0/PSXISO/Disney Tarzan (Europe).cue",
           "Disney's Tarzan (Europe).png");
    lookup("/dev_hdd0/PSXISO/Tarzan Latino.cue",
           "Disney Tarzan (Spain).png");
    lookup("/dev_hdd0/PSXISO/Gran Turismo 2 Latino.cue",
           "Gran Turismo 2 (USA).png");
    lookup("/dev_hdd0/PSXISO/Gran Turismo.cue",
           "Gran Turismo (USA).png");
    lookup("/dev_hdd0/PSXISO/Dragon Ball GT Final Bout Latino.cue",
           "Dragon Ball GT - Final Bout (USA).png");
    lookup("/dev_hdd0/PSXISO/Resident Evil 2 Latino Disco 2.cue",
           "Resident Evil 2 (USA) (Disc 2).png");
    lookup("/dev_hdd0/PSXISO/Resident Evil 2 Latino Disco 1.cue",
           "Resident Evil 2 (USA) (Disc 1).png");
    lookup("/dev_hdd0/PSXISO/Metal Gear Soli.cue",NULL);
    lookup("/dev_hdd0/PSXISO/Disney Tarzan Demostracion.cue",NULL);
    lookup("/dev_hdd0/PSXISO/Another Random Game.cue",NULL);
    printf("PASS PS1 catalog matcher: Disney Tarzan Latino -> Spanish cover; regional editions, disc pairs, no wrong fuzzy titles\n");
    return 0;
}
