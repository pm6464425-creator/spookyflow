#include "sf_ps1_cue_general.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
static int count;
#define CK(x) do{if(!(x)){fprintf(stderr,"GENERAL PS1 CUE FAILED line %d: %s\n",__LINE__,#x);return 1;}count++;}while(0)
static SfPs1CuePlan p;
static SfPs1CueChunk c[SF_PS1_CUE_MAX_CHUNKS];
static SfPs1Track t[SF_PS1_CUE_MAX_TRACKS];
static uint64_t sizes[SF_PS1_CUE_MAX_FILES],total;
static unsigned n;
static int parse(const char *txt)
{
    return sf_ps1_cue_general_parse("/dev_hdd0/PSXISO/Game/Game.cue",
                                    txt,strlen(txt),&p);
}
static int layout(void)
{
    return sf_ps1_cue_general_layout(&p,sizes,t,c,&n,&total);
}
int main(void)
{
    const char *one="FILE \"Game.bin\" BINARY\n"
        " TRACK 01 MODE2/2352\n  INDEX 01 00:00:00\n"
        " TRACK 02 AUDIO\n  INDEX 00 00:50:00\n  INDEX 01 01:00:00\n"
        " TRACK 03 AUDIO\n  INDEX 01 02:00:00\n";
    CK(parse(one)==0);
    CK(p.file_count==1 && p.track_count==3);
    CK(!strcmp(p.file[0].path,"/dev_hdd0/PSXISO/Game/Game.bin"));
    sizes[0]=2352ULL*16000;
    CK(layout()==0);
    CK(t[0].lba==0 && t[1].lba==4500 && t[2].lba==9000);
    CK(n==1 && c[0].bytes==sizes[0] && !c[0].is_silence);
    CK(total==sizes[0]);
    {
       const char *hybrid=
         "REM comment\n"
         "FILE \"Disc 01.bin\" BINARY\n"
         "TRACK 01 MODE2/2352\nINDEX 01 00:00:00\n"
         "TRACK 02 AUDIO\nINDEX 00 00:57:00\nINDEX 01 01:00:00\n"
         "FILE \"Disc 02.bin\" BINARY\n"
         "TRACK 03 AUDIO\nINDEX 01 00:00:00\n"
         "FILE \"Disc 03.bin\" BINARY\n"
         "TRACK 04 AUDIO\nINDEX 01 00:00:00\n";
       CK(parse(hybrid)==0);
       CK(p.file_count==3 && p.track_count==4);
       sizes[0]=2352ULL*9000;sizes[1]=2352ULL*2000;
       sizes[2]=2352ULL*1000;
       CK(layout()==0);
       CK(t[0].lba==0 && t[1].lba==4500);
       CK(t[2].lba==9000 && t[3].lba==11000);
       CK(total==2352ULL*12000);
       CK(n==3 && c[0].file_index==0 && c[1].file_index==1);
    }
    {
       const char *gap=
         "FILE \"Disc 1.bin\" BINARY\n"
         "TRACK 01 MODE2/2352\nINDEX 01 00:00:00\n"
         "TRACK 02 AUDIO\nPREGAP 00:02:00\nINDEX 01 01:00:00\n"
         "FILE \"Disc 2.bin\" BINARY\n"
         "TRACK 03 AUDIO\nINDEX 01 00:00:00\nPOSTGAP 00:01:00\n";
       CK(parse(gap)==0);
       CK(p.file_count==2 && p.track_count==3);
       sizes[0]=2352ULL*9000;sizes[1]=2352ULL*2000;
       CK(layout()==0);
       CK(t[1].lba==4650 && t[2].lba==9150);
       CK(total==2352ULL*(11000+150+75));
       CK(n==5);
       CK(c[0].bytes==2352ULL*4500 && !c[0].is_silence);
       CK(c[1].bytes==2352ULL*150 && c[1].is_silence);
       CK(c[2].at==2352ULL*4500 && c[2].bytes==2352ULL*4500);
       CK(c[3].file_index==1 && c[3].bytes==2352ULL*2000);
       CK(c[4].is_silence && c[4].bytes==2352ULL*75);
    }
    {
       const char *nested="FILE \"Tracks\\Track01.bin\" BINARY\n"
         "TRACK 01 MODE1/2352\nINDEX 01 00:00:00\n";
       CK(parse(nested)==0);
       CK(!strcmp(p.file[0].path,"/dev_hdd0/PSXISO/Game/Tracks/Track01.bin"));
       sizes[0]=2352ULL*1000;CK(layout()==0);
       CK(t[0].lba==0 && n==1);
    }
    {
       const char *bad="FILE \"../system.bin\" BINARY\n"
         "TRACK 01 MODE2/2352\nINDEX 01 00:00:00\n";
       CK(parse(bad)!=0);
    }
    {
       const char *bad="FILE \"x.wav\" WAVE\n"
         "TRACK 01 AUDIO\nINDEX 01 00:00:00\n";
       CK(parse(bad)!=0);
    }
    {
       const char *bad="FILE \"x.bin\" BINARY\n"
         "TRACK 01 MODE2/2048\nINDEX 01 00:00:00\n";
       CK(parse(bad)!=0);
    }
    {
       const char *bad="FILE \"x.bin\" BINARY\n"
         "TRACK 01 MODE2/2352\nINDEX 01 00:00:00\n"
         "TRACK 02 AUDIO\nINDEX 01 00:02:00\nPREGAP 00:02:00\n"
         "INDEX 00 00:01:00\n";
       CK(parse(bad)!=0);
    }
    {
       const char *bad="FILE \"x.bin\" BINARY\n"
         "TRACK 01 MODE2/2352\nINDEX 01 00:00:00\n"
         "TRACK 02 AUDIO\nINDEX 01 00:10:00\nPOSTGAP 00:02:00\n"
         "TRACK 03 AUDIO\nINDEX 01 00:11:00\n";
       CK(parse(bad)!=0);
    }
    {
       const char *bad="FILE \"x.bin\" BINARY\n"
         "TRACK 01 MODE2/2352\nINDEX 01 00:00:00\n"
         "TRACK 02 AUDIO\nINDEX 01 00:10:00\n"
         "TRACK 03 AUDIO\nINDEX 01 00:09:00\n";
       CK(parse(bad)!=0);
    }
    {
       const char *bad="FILE \"x.bin\" BINARY\n"
         "TRACK 01 MODE2/2352\nINDEX 01 00:00:00\n"
         "FILE \"x.bin\" BINARY\n"
         "TRACK 02 AUDIO\nINDEX 01 00:00:00\n";
       CK(parse(bad)!=0);
    }
    {
       const char *text="FILE \"Data.bin\" BINARY\n"
         "TRACK 01 MODE2/2352\nINDEX 01 00:00:00\n";
       CK(parse(text)==0);
       sizes[0]=2048ULL*1000;CK(layout()!=0);
    }
    printf("GENERAL PS1: %d tests PASS (mixed FILE/TRACK, INDEX00, "
           "PREGAP, POSTGAP, nested paths, safe rejection)\n",count);
    return 0;
}
