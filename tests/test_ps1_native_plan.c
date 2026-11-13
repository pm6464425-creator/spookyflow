#include "sf_ps1_native_plan.h"
#include <stdio.h>
#include <string.h>
#define CK(x) do{if(!(x)){fprintf(stderr,"PS1 native CUE test FAIL line %d: %s\n",__LINE__,#x);return 1;}}while(0)
int main(void)
{
    SfPs1NativePlan p;
    const char *cue="/dev_hdd0/PSXISO/Test Game.cue";
    const char *valid="REM GENRE Test\nFILE \"Test Game.bin\" BINARY\n"
                     "  TRACK 01 MODE2/2352\n"
                     "    INDEX 01 00:00:00\n"
                     "  TRACK 02 AUDIO\n"
                     "    INDEX 00 00:10:00\n"
                     "    INDEX 01 00:12:05\n"
                     "  TRACK 03 AUDIO\n"
                     "    INDEX 01 01:11:50\n";
    const char *missing="FILE \"x.bin\" BINARY\nTRACK 01 MODE2/2352\nTRACK 02 AUDIO\nINDEX 01 00:03:00\n";
    const char *multi="FILE \"a.bin\" BINARY\nTRACK 01 MODE2/2352\nINDEX 01 00:00:00\nFILE \"b.bin\" BINARY\nTRACK 02 AUDIO\nINDEX 01 00:00:00\n";
    const char *traversal="FILE \"../system.bin\" BINARY\nTRACK 01 MODE2/2352\nINDEX 01 00:00:00\n";
    const char *raw2048="FILE \"x.iso\" BINARY\nTRACK 01 MODE1/2048\nINDEX 01 00:00:00\n";
    const char *gap="FILE \"x.bin\" BINARY\nTRACK 01 MODE2/2352\nINDEX 01 00:00:00\nPREGAP 00:02:00\n";
    const char *repeated="FILE \"x.bin\" BINARY\nTRACK 01 MODE2/2352\nINDEX 01 00:00:00\nINDEX 01 00:00:00\n";
    const char *outoforder="FILE \"x.bin\" BINARY\nTRACK 01 MODE2/2352\nINDEX 01 00:00:00\nTRACK 02 AUDIO\nINDEX 01 00:00:00\n";
    CK(sf_ps1_plan_cue(cue,valid,strlen(valid),&p)==0);
    CK(p.num_tracks==3);
    CK(!strcmp(p.image,"/dev_hdd0/PSXISO/Test Game.bin"));
    CK(!p.tracks[0].is_audio && p.tracks[1].is_audio && p.tracks[2].is_audio);
    CK(p.tracks[0].lba==0 && p.tracks[1].lba==12*75+5 && p.tracks[2].lba==71*75+50);
    CK(sf_ps1_plan_cue(cue,missing,strlen(missing),&p)!=0);
    CK(sf_ps1_plan_cue(cue,multi,strlen(multi),&p)!=0);
    CK(sf_ps1_plan_cue(cue,traversal,strlen(traversal),&p)!=0);
    CK(sf_ps1_plan_cue(cue,raw2048,strlen(raw2048),&p)!=0);
    CK(sf_ps1_plan_cue(cue,gap,strlen(gap),&p)!=0);
    CK(sf_ps1_plan_cue(cue,repeated,strlen(repeated),&p)!=0);
    CK(sf_ps1_plan_cue(cue,outoforder,strlen(outoforder),&p)!=0);
    CK(sf_ps1_plan_cue(cue,valid,0,&p)!=0);
    CK(sf_ps1_plan_cue(cue,valid,32769,&p)!=0);
    puts("PS1 native CUE: safe one-BIN/multi-track, fallback for multi-file/invalid CUE: OK");
    return 0;
}
