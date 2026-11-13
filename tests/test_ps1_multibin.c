#include "sf_ps1_multibin.h"
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#define CK(cond) do{if(!(cond)){fprintf(stderr,"PS1 multibin FAIL line %d: %s\n",__LINE__,#cond);exit(1);}}while(0)
int main(void)
{
    char cue[16384]={0};size_t off=0;
    SfPs1MultiPlan plan;
    SfPs1Track tracks[SF_PS1_MULTIBIN_MAX];
    uint64_t sizes[SF_PS1_MULTIBIN_MAX],sum;
    const char *path="/dev_hdd0/PSXISO/Tekken (USA).cue";
    for(int i=0;i<28;i++){
        off+=(size_t)snprintf(cue+off,sizeof(cue)-off,
          "FILE \"Tekken (USA) (Track %02d).bin\" BINARY\n"
          "  TRACK %02d %s\n"
          "    INDEX 01 %s\n",
          i+1,i+1,i?"AUDIO":"MODE2/2352",
          i==1?"00:02:00":"00:00:00");
        sizes[i]=2352u*(uint64_t)(i==0?10000:1800);
    }
    CK(sf_ps1_multibin_parse(path,cue,strlen(cue),&plan)==0);
    CK(plan.count==28);
    CK(!strcmp(plan.part[0].path,"/dev_hdd0/PSXISO/Tekken (USA) (Track 01).bin"));
    CK(plan.part[27].is_audio==1);
    CK(sf_ps1_multibin_layout(&plan,sizes,tracks,&sum)==0);
    CK(tracks[0].lba==0);
    CK(tracks[1].lba==10000+150);
    CK(tracks[2].lba==10000+1800);
    CK(tracks[27].lba==10000+26*1800);
    CK(sum==2352u*(uint64_t)(10000+27*1800));
    CK(sf_ps1_multibin_parse(path,cue,0,&plan)!=0);
    {
        const char *gap="FILE \"T1.bin\" BINARY\n"
          "TRACK 01 MODE2/2352\nINDEX 01 00:00:00\n"
          "FILE \"T2.bin\" BINARY\nTRACK 02 AUDIO\n"
          "PREGAP 00:02:00\nINDEX 01 00:00:00\n";
        uint64_t a[2]={2352u*1000u,2352u*200u};
        CK(sf_ps1_multibin_parse(path,gap,strlen(gap),&plan)==0);
        CK(sf_ps1_multibin_layout(&plan,a,tracks,&sum)==0);
        CK(tracks[1].lba==1150);
        CK(sum==2352u*1350u);
    }
    {
        const char *bad="FILE \"../hidden.bin\" BINARY\n"
        "TRACK 01 MODE2/2352\nINDEX 01 00:00:00\n"
        "FILE \"two.bin\" BINARY\nTRACK 02 AUDIO\nINDEX 01 00:00:00\n";
        CK(sf_ps1_multibin_parse(path,bad,strlen(bad),&plan)!=0);
    }
    {
        const char *bad="FILE \"T1.bin\" BINARY\n"
        "TRACK 01 MODE1/2048\nINDEX 01 00:00:00\n"
        "FILE \"T2.bin\" BINARY\nTRACK 02 AUDIO\nINDEX 01 00:00:00\n";
        CK(sf_ps1_multibin_parse(path,bad,strlen(bad),&plan)!=0);
    }
    {
        const char *bad="FILE \"T1.bin\" BINARY\n"
        "TRACK 01 MODE2/2352\nINDEX 01 00:00:00\n"
        "TRACK 02 AUDIO\nINDEX 01 00:01:00\n"
        "FILE \"T2.bin\" BINARY\nTRACK 03 AUDIO\nINDEX 01 00:00:00\n";
        CK(sf_ps1_multibin_parse(path,bad,strlen(bad),&plan)!=0);
    }
    puts("PS1 Redump 28 BIN + 28 tracks / audio LBA / explicit pregap / safe rejections: OK");
    return 0;
}
