/* One PS1 diagnostic only. Every launch overwrites it and never duplicates
 * or captures previous PS2/BDISO test traces in the same file. */
#include "log.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static int checks;
#define CHECK(x) do {if(!(x)){fprintf(stderr,"PS1 log FAIL line %d: %s\n",__LINE__,#x);exit(1);}checks++;}while(0)
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
    static const char *p="build-host/diagnostico_ps1.txt";
    char report[8192];size_t n;
    CHECK(log_open(p)==0);
    log_printf("PS1_DIAG FIRST_ATTEMPT=1");
    log_printf("PS1_MOUNT RESULT=ERROR RC=-41");
    log_printf("PS2_DIAG MUST_NOT_BE_RECORDED=1");
    log_printf("cobra: PS3 unrelated mount");
    log_printf("BDISO unrelated movie");
    log_printf("render: ordinary frame");
    log_close();
    n=read_report(p,report,sizeof(report));
    CHECK(n>40);
    CHECK(strstr(report,"DIAGNOSTICO PS1 (V15.24)")!=NULL);
    CHECK(strstr(report,"FIRST_ATTEMPT=1")!=NULL);
    CHECK(strstr(report,"PS1_MOUNT RESULT=ERROR")!=NULL);
    CHECK(strstr(report,"MUST_NOT_BE_RECORDED")==NULL);
    CHECK(strstr(report,"BDISO unrelated")==NULL);
    CHECK(strstr(report,"PS3 unrelated")==NULL);
    CHECK(strstr(report,"ordinary frame")==NULL);
    CHECK(log_open(p)==0);
    log_printf("PS1_DIAG SECOND_ATTEMPT=1");
    log_printf("PS1_DIAG STAGE=MOUNT_PSX_7027 SYS_RC=-1");
    log_close();
    n=read_report(p,report,sizeof(report));
    CHECK(n>40);
    CHECK(strstr(report,"SECOND_ATTEMPT=1")!=NULL);
    CHECK(strstr(report,"FIRST_ATTEMPT=1")==NULL);
    CHECK(strstr(report,"STAGE=MOUNT_PSX_7027 SYS_RC=-1")!=NULL);
    CHECK(remove(p)==0);
    printf("PS1 single overwritten diagnostic: %d checks passed\n",checks);
    return 0;
}
