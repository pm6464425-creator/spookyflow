/* Real directory integration for renamed PS1 BIN references.
 * Only test fixture files are created/deleted. */
#include "sf_ps1_resolve.h"
#include "log.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <errno.h>
static int checks;
#define CHECK(x) do{if(!(x)){fprintf(stderr,"PS1 real filesystem resolver FAIL line %d: %s (errno=%d)\n",__LINE__,#x,errno);exit(1);}checks++;}while(0)
static void file(const char *p)
{
    FILE *f=fopen(p,"wb");
    CHECK(f!=NULL);
    CHECK(fwrite("PSX RAW SECTORS",1,15,f)==15);
    CHECK(fclose(f)==0);
}
int main(void)
{
    char cwd[1024],dir[1200],a[1400],b[1400],c[1400],expected[1400],result[1400];
    CHECK(getcwd(cwd,sizeof(cwd))!=NULL);
    CHECK(snprintf(dir,sizeof(dir),"%s/build-host/ps1_resolver_fixture",cwd)
          <(int)sizeof(dir));
    if(mkdir(dir,0777)!=0)CHECK(errno==EEXIST);
    snprintf(a,sizeof(a),"%s/Metal Slug X.bin",dir);
    snprintf(b,sizeof(b),"%s/Metal Slug X (USA).bin",dir);
    snprintf(c,sizeof(c),"%s/METAL SLUG X (EUROPE).BIN",dir);
    snprintf(expected,sizeof(expected),"%s/Metal Slug X (Europe).bin",dir);
    CHECK(log_open("build-host/ps1_resolver_fixture_report.txt")==0);
    file(a);
    CHECK(sf_ps1_resolve_existing(expected,result,sizeof(result))==0);
    CHECK(!strcmp(result,a));
    file(b);
    CHECK(sf_ps1_resolve_existing(expected,result,sizeof(result))==-2);
    CHECK(unlink(b)==0);
    file(c);
    CHECK(sf_ps1_resolve_existing(expected,result,sizeof(result))==0);
    CHECK(!strcmp(result,c)); /* same title + exact casefold is stronger */
    CHECK(unlink(c)==0);
    CHECK(sf_ps1_resolve_existing(expected,result,sizeof(result))==0);
    CHECK(!strcmp(result,a));
    snprintf(expected,sizeof(expected),"%s/Unrelated Game (Europe).bin",dir);
    CHECK(sf_ps1_resolve_existing(expected,result,sizeof(result))==-1);
    CHECK(unlink(a)==0);
    CHECK(sf_ps1_resolve_existing(expected,result,sizeof(result))==-1);
    /* Generalized real directory smoke test. Different title metadata,
       same physical PS1 game image; no per-title game dictionaries. */
    snprintf(a,sizeof(a),"%s/Disney Tarzan (Europe) (En,Fr,De,Es,It).bin",dir);
    snprintf(expected,sizeof(expected),
             "%s/(PS1) Disney Tarzan [PAL] [SCES-01519].BIN",dir);
    file(a);
    CHECK(sf_ps1_resolve_existing(expected,result,sizeof(result))==0);
    CHECK(!strcmp(result,a));
    CHECK(unlink(a)==0);
    snprintf(a,sizeof(a),"%s/Grand Theft Auto 2.bin",dir);
    snprintf(expected,sizeof(expected),
             "%s/Grand Theft Auto 2 (Europe) (En,Fr,De,Es,It) (Rev 1).bin",dir);
    file(a);
    CHECK(sf_ps1_resolve_existing(expected,result,sizeof(result))==0);
    CHECK(!strcmp(result,a));
    CHECK(unlink(a)==0);
    log_close();
    CHECK(remove("build-host/ps1_resolver_fixture_report.txt")==0);
    CHECK(rmdir(dir)==0);
    printf("PS1 CUE real filesystem integration: %d checks PASS\n",checks);
    return 0;
}
