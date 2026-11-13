/* 22 host-level safety regressions; no LV2 syscalls are executed. */
#include "ps2_mount_plan.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static int checks=0;
#define CHECK(expr) do { if(!(expr)){fprintf(stderr,"FAIL %s:%d: %s\n",__FILE__,__LINE__,#expr);exit(1);} checks++; }while(0)
typedef struct { int have; int calls; } Fake;
static int fake_file(void *ctx,const char *path)
{
    Fake *f=(Fake*)ctx;
    const char *p=strrchr(path,'.');
    int suffix=-1;
    f->calls++;
    if(p && p[1]>='0' && p[1]<='9' && p[2]=='\0') suffix=p[1]-'0';
    return (suffix<0)? f->have>0 : suffix<f->have;
}
int main(void)
{
    char paths[SF_PS2_MAX_PARTS][SF_PS2_MAX_PATH];
    const char *plain="/dev_hdd0/PS2ISO/Winning Eleven 10.iso";
    const char *split="/dev_hdd0/PS2ISO/Winning Eleven 10.iso.0";
    Fake f={2,0};
    int n;
    CHECK(sf_ps2_valid_iso_path(plain));
    CHECK(sf_ps2_valid_iso_path(split));
    CHECK(sf_ps2_valid_iso_path("/dev_hdd0/PS2ISO/a.iso"));
    CHECK(sf_ps2_valid_iso_path("/dev_hdd0/PS2ISO/a.ISO"));
    CHECK(sf_ps2_valid_iso_path("/dev_hdd0/PS2ISO/a.iSo.0"));
    CHECK(!sf_ps2_valid_iso_path(NULL));
    CHECK(!sf_ps2_valid_iso_path(""));
    CHECK(!sf_ps2_valid_iso_path("/dev_usb000/PS2ISO/g.iso"));
    CHECK(!sf_ps2_valid_iso_path("/dev_hdd0/PS3ISO/g.iso"));
    CHECK(!sf_ps2_valid_iso_path("/dev_hdd0/PS2ISOx/g.iso"));
    CHECK(!sf_ps2_valid_iso_path("/dev_hdd0/PS2ISO/../PS3ISO/g.iso"));
    CHECK(!sf_ps2_valid_iso_path("/dev_hdd0/PS2ISO/./g.iso"));
    CHECK(!sf_ps2_valid_iso_path("/dev_hdd0/PS2ISO/g.iso/../b.iso"));
    CHECK(!sf_ps2_valid_iso_path("/dev_hdd0/PS2ISO/a\\b.iso"));
    CHECK(!sf_ps2_valid_iso_path("/dev_hdd0/PS2ISO/b.iso\ng.iso"));
    CHECK(!sf_ps2_valid_iso_path("/dev_hdd0/PS2ISO/g.BIN.ENC"));
    CHECK(!sf_ps2_valid_iso_path("/dev_hdd0/PS2ISO/g.iso.1"));
    n=sf_ps2_collect_parts(plain,fake_file,&f,paths);
    CHECK(n==1 && strcmp(paths[0],plain)==0);
    n=sf_ps2_collect_parts(split,fake_file,&f,paths);
    CHECK(n==2 && strstr(paths[1],".iso.1")!=NULL);
    f.have=0;
    CHECK(sf_ps2_collect_parts(plain,fake_file,&f,paths)==-2);
    f.have=1;
    CHECK(sf_ps2_collect_parts(split,fake_file,&f,paths)==1);
    CHECK(sf_ps2_collect_parts(plain,NULL,&f,paths)==-1);
    printf("PS2 portable safety: %d/%d checks passed\n",checks,22);
    return checks==22?0:1;
}
