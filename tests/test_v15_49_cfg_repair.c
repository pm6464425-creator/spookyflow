/* SpookyFlow V15.49: PS3 HEN config rename failure regression.
 * Compile with -DSF_SN_TEST_RENAME_EEXIST or -DSF_SN_TEST_DENY_TEMP to
 * simulate the two distinct causes of the observed -6.
 * This is a file-replacement test, not PS3 HDMI pixel verification.
 */
#include "sf_snes_screen.h"
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#ifndef SF_SN_CFG_BACKUP_DIR
#define SF_SN_CFG_BACKUP_DIR "/tmp"
#endif
static void fill_config(const char *path)
{
    FILE *f=fopen(path,"wb");size_t n=0;const char *h=
        "PSNES : { VERSION = 702; EMULATION : { SCALING = \"FIT\"; "
        "FILTER = \"LINEAR\"; SCALING_MODE=\"ASPECT\"; }; "
        "GAMEPAD : { JOY_A=\"4\"; }; };\n";
    assert(f);
    n=fwrite(h,1,strlen(h),f);assert(n==strlen(h));
    while(n<2695){assert(fputc(' ',f)!=EOF);n++;}
    assert(fclose(f)==0);
}
static size_t slurp(const char *p,char *buf,size_t cap) {
    FILE *f=fopen(p,"rb");size_t n;assert(f);
    n=fread(buf,1,cap-1,f);assert(!ferror(f));fclose(f);buf[n]=0;
    return n;
}
static void backup_name(const char *path,char *out,size_t n) {
    const unsigned char *p=(const unsigned char*)path;
    uint64_t hash=UINT64_C(14695981039346656037);
    for(;*p;p++){hash^=*p;hash*=UINT64_C(1099511628211);}
    snprintf(out,n,SF_SN_CFG_BACKUP_DIR "/snes_cfg_recovery_%016llx.dat",(unsigned long long)hash);
}
static void write_text(const char *p,const char *s){
    FILE *f=fopen(p,"wb");assert(f);
    assert(fwrite(s,1,strlen(s),f)==strlen(s));assert(fclose(f)==0);
}
int main(int argc,char **argv)
{
    char conf[600],game[600],sram[600],tmp[650],bak[650],now[5000],old[5000],state[256];
    int rc;
    assert(argc==2);
    assert(snprintf(conf,sizeof(conf),"%s/config.cfg",argv[1])<(int)sizeof(conf));
    assert(snprintf(game,sizeof(game),"%s/rom.cfg",argv[1])<(int)sizeof(game));
    assert(snprintf(sram,sizeof(sram),"%s/game.srm",argv[1])<(int)sizeof(sram));
    assert(snprintf(tmp,sizeof(tmp),"%s.sf49.tmp",conf)<(int)sizeof(tmp));
    backup_name(conf,bak,sizeof(bak));
    remove(conf);remove(game);remove(sram);remove(tmp);remove(bak);
    fill_config(conf);
    assert(slurp(conf,old,sizeof(old))==2695);
    write_text(sram,"PRESERVED SRAM DATA");
    rc=sf_snes_config_set_full(conf,1);
#ifdef SF_SN_TEST_DENY_DIRECT
    assert(rc==-6);
    assert(slurp(conf,now,sizeof(now))==2695 && strcmp(now,old)==0);
    assert(access(bak,F_OK)==0);
    remove(bak);
#else
    assert(rc==0);
    assert(sf_snes_config_video_probe(conf,state,sizeof(state))==0);
    assert(strstr(state,"expected_FULL=YES"));
    assert(slurp(conf,now,sizeof(now))==2696);
    assert(strstr(now,"SCALING = \"FULL\""));
    assert(strstr(now,"FILTER = \"LINEAR\""));
    assert(strstr(now,"JOY_A=\"4\""));
    assert(access(bak,F_OK)!=0);
    assert(access(tmp,F_OK)!=0);
    assert(sf_snes_config_set_full(conf,1)==0);
    assert(slurp(conf,now,sizeof(now))==2696);
    /* An existing per-ROM override also becomes FULL with all other keys. */
    write_text(game,"PSNES_GAME : { EMULATION : { SCALING = \"FIT\"; FILTER = \"POINT\"; }; GAMEPAD:{JOY_B=\"8\";};};\n");
    assert(sf_snes_config_set_full(game,0)==0);
    slurp(game,now,sizeof(now));
    assert(strstr(now,"SCALING = \"FULL\""));
    assert(strstr(now,"FILTER = \"POINT\""));
    assert(strstr(now,"JOY_B=\"8\""));
    remove(game);
    assert(sf_snes_config_set_full(game,0)==0);
    assert(access(game,F_OK)!=0);
    /* Simulate a power cut after the original was saved but the CFG got damaged.
     * Next launch restores it, then applies FULL. */
    write_text(bak,old);
    write_text(conf,"broken config");
    assert(sf_snes_config_set_full(conf,1)==0);
    assert(sf_snes_config_video_probe(conf,state,sizeof(state))==0);
    assert(access(bak,F_OK)!=0);
#endif
    assert(strcmp((slurp(sram,now,sizeof(now)),now),"PRESERVED SRAM DATA")==0);
    remove(conf);remove(game);remove(sram);remove(tmp);remove(bak);
    printf("PASS V15.49: PS3 HEN rename/temp failure, config FULL verified, backup restored, game data untouched\n");
    return 0;
}
