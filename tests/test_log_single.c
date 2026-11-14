/* One SNES diagnostic only. Replaced on every start; other systems excluded. */
#include "log.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static int checks;
#define CHECK(x) do {if(!(x)){fprintf(stderr,"SNES log FAIL line %d: %s\n",__LINE__,#x);exit(1);}checks++;}while(0)
static size_t read_report(const char *p,char *buf,size_t cap)
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
    char report[8192];size_t n;
    CHECK(log_open(p)==0);
    log_printf("snes: ROM_FOUND /dev_hdd0/SNES/Super Mario World.sfc");
    log_printf("snes: CATALOG_MATCH Super Mario World (USA).png");
    log_printf("snes: HTTP status=404");
    log_printf("snes: COVER_DECODE_ERROR path=missing.png");
    log_printf("snes: spawn returned unexpectedly");
    log_printf("SNES_DIAG TEST_STAGE=COVER_RETRY");
    log_printf("PS1_DIAG SHOULD_NOT_BE_CAPTURED");
    log_printf("PS1_MOUNT RESULT=ERROR");
    log_printf("PS2_DIAG SHOULD_NOT_BE_CAPTURED");
    log_printf("cobra: PS3 unrelated mount");
    log_printf("BDISO unrelated movie");
    log_printf("render: ordinary frame");
    log_close();
    n=read_report(p,report,sizeof(report));
    CHECK(n>40);
    CHECK(strstr(report,"DIAGNOSTICO SNES (V15.32)")!=NULL);
    CHECK(strstr(report,"ROM_FOUND")!=NULL);
    CHECK(strstr(report,"CATALOG_MATCH")!=NULL);
    CHECK(strstr(report,"HTTP status=404")!=NULL);
    CHECK(strstr(report,"COVER_DECODE_ERROR")!=NULL);
    CHECK(strstr(report,"spawn returned")!=NULL);
    CHECK(strstr(report,"TEST_STAGE=COVER_RETRY")!=NULL);
    CHECK(strstr(report,"PS1_DIAG")==NULL);
    CHECK(strstr(report,"PS1_MOUNT")==NULL);
    CHECK(strstr(report,"PS2_DIAG")==NULL);
    CHECK(strstr(report,"BDISO unrelated")==NULL);
    CHECK(strstr(report,"PS3 unrelated")==NULL);
    CHECK(strstr(report,"ordinary frame")==NULL);
    CHECK(log_open(p)==0);
    log_printf("snes: SECOND_BOOT=1");
    log_close();
    n=read_report(p,report,sizeof(report));
    CHECK(n>40);
    CHECK(strstr(report,"SECOND_BOOT=1")!=NULL);
    CHECK(strstr(report,"ROM_FOUND")==NULL);
    CHECK(remove(p)==0);
    printf("SNES single overwritten diagnostic: %d checks passed\n",checks);
    return 0;
}
