/* V15.52: exactly one PS1 report, append across XMB sessions, no stale SNES. */
#include "log.h"
#include <stdio.h>
#include <assert.h>
#include <stdlib.h>
#include <string.h>
int main(int argc,char **argv)
{
    char buf[20000];
    FILE *f;
    size_t n;
    assert(argc==2);
    remove(argv[1]);
    assert(log_open(argv[1])==0);
    log_printf("ps1_video_diag: XREGISTRY_READONLY=YES VALUE=2 MODE=FULL_SCREEN_16_9");
    log_printf("PS1_DIAG STAGE=MOUNT_PSX_7027 RESULT=OK TRACKS=3");
    log_printf("PS1_MOUNT RESULT=OK");
    log_printf("ps1: image selected");
    log_printf("snes: DIAG_SPAWN=SHOULD_NOT_LEAK");
    log_printf("video_diag: SNES_ONLY=SHOULD_NOT_LEAK");
    log_printf("audio: DIAG_UI_MODE=SHOULD_NOT_LEAK");
    log_printf("cover: SHOULD_NOT_LEAK");
    log_close();
    assert(log_open(argv[1])==0);
    log_printf("ps1_video_diag: SECOND_SESSION=YES");
    log_close();
    f=fopen(argv[1],"rb");assert(f);
    n=fread(buf,1,sizeof(buf)-1,f);assert(!ferror(f));
    assert(fclose(f)==0);buf[n]=0;
    assert(strstr(buf,"SPOOKYFLOW V15.52 - DIAGNOSTICO AVANZADO PANTALLA PS1"));
    assert(strstr(buf,"XREGISTRY_READONLY=YES"));
    assert(strstr(buf,"MOUNT_PSX_7027"));
    assert(strstr(buf,"PS1_MOUNT RESULT=OK"));
    assert(strstr(buf,"SECOND_SESSION=YES"));
    assert(strstr(buf,"NUEVA SESION - VIDEO PS1 V15.52"));
    assert(!strstr(buf,"SHOULD_NOT_LEAK"));
    assert(!strstr(buf,"SPOOKYFLOW V15.48"));
    remove(argv[1]);
    puts("PASS V15.52 single persistent PS1 TXT; correct filter and no old SNES/audio/cover reports");
    return 0;
}
