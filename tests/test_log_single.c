/* V15.40: one useful SNES launch diagnostic, retained after XMB/restart.
 * The original V15.18 contract still excludes PS1/PS2/PS3/BDISO spam. */
#include "log.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static int checks;
#define CHECK(x) do { if(!(x)) { fprintf(stderr,"SNES log FAIL line %d: %s\n",__LINE__,#x); exit(1); } checks++; } while(0)
static size_t read_report(const char *p, char *buf, size_t cap)
{
    FILE *f=fopen(p,"rb");size_t n;
    CHECK(f!=NULL);
    n=fread(buf,1,cap-1,f);buf[n]=0;
    CHECK(fclose(f)==0);
    return n;
}
int main(void)
{
    static const char *p="build-host/diagnostico_snes.txt";
    char report[8192]; size_t n;
    int i;
    CHECK(log_open(p)==0);
    for (i=0;i<500;i++) {
        log_printf("snes: ROM_FOUND /dev_hdd0/SNES/Test%d.sfc",i);
        log_printf("snes: COVER_GPU slot=%d",i%30);
        log_printf("snes: CACHE_HIT rom=Test%d.sfc",i);
    }
    log_printf("snes: CATALOG_MATCH Super Mario World (USA).png");
    log_printf("snes: HTTP status=404");
    log_printf("snes: COVER_DECODE_ERROR path=missing.png");
    log_printf("snes: DIAG_LAUNCH_SELECTED rom=/dev_hdd0/SNES/Super Mario World.sfc");
    log_printf("snes: DIAG_ENGINE_PREPARE_RESULT=0");
    log_printf("snes: DIAG_SPAWN_CALL exe=/dev_hdd0/game/PSNE00001/USRDIR/EBOOT.BIN");
    log_printf("PS1_DIAG SHOULD_NOT_BE_CAPTURED");
    log_printf("PS1_MOUNT RESULT=ERROR");
    log_printf("PS2_DIAG SHOULD_NOT_BE_CAPTURED");
    log_printf("cobra: PS3 unrelated mount");
    log_printf("BDISO unrelated movie");
    log_printf("render: ordinary frame");
    log_close();

    n=read_report(p,report,sizeof(report));
    CHECK(n>100);
    CHECK(n<4096);
    CHECK(strstr(report,"DIAGNOSTICO DE ARRANQUE SNES V15.40")!=NULL);
    CHECK(strstr(report,"DIAG_LAUNCH_SELECTED")!=NULL);
    CHECK(strstr(report,"DIAG_SPAWN_CALL")!=NULL);
    CHECK(strstr(report,"ROM_FOUND")==NULL);
    CHECK(strstr(report,"CACHE_HIT")==NULL);
    CHECK(strstr(report,"COVER_GPU")==NULL);
    CHECK(strstr(report,"CATALOG_MATCH")==NULL);
    CHECK(strstr(report,"HTTP status=404")==NULL);
    CHECK(strstr(report,"COVER_DECODE_ERROR")==NULL);
    CHECK(strstr(report,"PS1_DIAG")==NULL);
    CHECK(strstr(report,"PS1_MOUNT")==NULL);
    CHECK(strstr(report,"PS2_DIAG")==NULL);
    CHECK(strstr(report,"BDISO unrelated")==NULL);
    CHECK(strstr(report,"PS3 unrelated")==NULL);
    CHECK(strstr(report,"ordinary frame")==NULL);

    CHECK(log_open(p)==0);  /* reboot after PS3 process exit */
    log_printf("snes: DIAG_PREVIOUS_HANDOFF_FOUND ROM=/dev_hdd0/SNES/Super Mario World.sfc");
    log_printf("snes: DIAG_PSNES_BOOT[1]=PSNE00001 ctor first");
    log_printf("snes: DIAG_RESULT=PSNES_STARTED_BUT_NOT_RENDER_LOOP");
    log_close();
    n=read_report(p,report,sizeof(report));
    CHECK(n>100);
    CHECK(strstr(report,"DIAG_LAUNCH_SELECTED")!=NULL); /* survives */
    CHECK(strstr(report,"DIAG_PREVIOUS_HANDOFF_FOUND")!=NULL);
    CHECK(strstr(report,"DIAG_PSNES_BOOT[1]")!=NULL);
    CHECK(strstr(report,"DIAG_RESULT=PSNES_STARTED_BUT_NOT_RENDER_LOOP")!=NULL);
    CHECK(strstr(report,"ROM_FOUND")==NULL);
    CHECK(remove(p)==0);
    printf("SNES focused persistent diagnostic: %d checks passed\n",checks);
    return 0;
}
