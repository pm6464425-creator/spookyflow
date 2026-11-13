/* A single SpookyFlow diagnostic TXT must be overwritten, never multiplied.
 * Host regression of src/core/log.c with the exact release feature flags.
 */
#include "log.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static int checks=0;
#define CHECK(x) do {if(!(x)){fprintf(stderr,"V15.18 log FAIL line %d: %s\n",__LINE__,#x);exit(1);}checks++;}while(0)
static size_t load_file(const char *path,char *dst,size_t cap)
{
    FILE *f=fopen(path,"rb");size_t n;
    CHECK(f!=NULL);
    n=fread(dst,1,cap-1,f);dst[n]=0;
    CHECK(fclose(f)==0);
    return n;
}
int main(void)
{
    static const char *p="build-host/spookyflow_single_diagnostic.txt";
    char data[8192];size_t n;
    CHECK(log_open(p)==0);
    log_printf("PS2_DIAG FIRST_RUN=1");
    log_printf("render: ordinary frame / not relevant to PS2"); /* omit spam */
    log_close();
    n=load_file(p,data,sizeof(data));
    CHECK(n>40);
    CHECK(strstr(data,"DIAGNOSTICO UNICO")!=NULL);
    CHECK(strstr(data,"PS2_DIAG FIRST_RUN=1")!=NULL);
    CHECK(strstr(data,"ordinary frame")==NULL);
    CHECK(log_open(p)==0); /* MUST use wb, not ab */
    log_printf("PS2_DIAG SECOND_RUN=1");
    log_printf("PS2_DIAG CONFIG_PRESENT=1 CONFIG_ENCRYPTED_VALID=1");
    log_close();
    n=load_file(p,data,sizeof(data));
    CHECK(n>40);
    CHECK(strstr(data,"PS2_DIAG SECOND_RUN=1")!=NULL);
    CHECK(strstr(data,"PS2_DIAG FIRST_RUN=1")==NULL);
    CHECK(strstr(data,"CONFIG_ENCRYPTED_VALID=1")!=NULL);
    CHECK(strstr(data,".txt") == NULL);
    CHECK(remove(p)==0);
    printf("Single diagnostic log: %d checks passed\n",checks);
    return 0;
}
