/* Standalone host test for the V15.48 single, persistent forensic video log. */
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "log.h"
int main(int argc,char **argv) {
    FILE *f;char buf[32768];size_t n;
    assert(argc==2);
    remove(argv[1]);
    assert(log_open(argv[1])==0);
    log_printf("video_diag: X=720 edge_border=40");
    log_printf("snes: DIAG_APP_SELF=VERIFIED");
    log_printf("audio: DIAG_SHOULD_NOT_LEAK");
    log_printf("music: SHOULD_NOT_LEAK");
    log_printf("usb_diag: SHOULD_NOT_LEAK");
    log_close();
    assert(log_open(argv[1])==0);
    log_printf("video_diag: SESSION_2=YES");
    log_close();
    f=fopen(argv[1],"rb");assert(f);
    n=fread(buf,1,sizeof(buf)-1,f);assert(!ferror(f));fclose(f);buf[n]=0;
    assert(strstr(buf,"V15.48")&&strstr(buf,"VIDEO")&&strstr(buf,"X=720"));
    assert(strstr(buf,"DIAG_APP_SELF=VERIFIED")&&strstr(buf,"SESSION_2=YES"));
    assert(strstr(buf,"NUEVA SESION")&&strstr(buf,"HARDWARE")&&strstr(buf,"NO MIDE PIXELES"));
    assert(!strstr(buf,"SHOULD_NOT_LEAK")&&!strstr(buf,"audio: DIAG"));
    puts("PASS forensic video log persists across app restarts, filters audio/USB and remains single TXT");
    return 0;
}
