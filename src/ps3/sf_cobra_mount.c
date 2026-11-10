/*
 * SpookyFlow direct PS3 Cobra mount backend. EXPERIMENTAL / UNTESTED ON HARDWARE.
 * The disc-image and folder-mapping techniques follow IRISMAN/source/cobra.c
 * (GPL-3.0-or-later); retain licenses on redistribution.
 * No fan control, network server, webMAN, or boot-time plugin is installed.
 */
#include "sf_cobra_mount.h"
#include <ppu-lv2.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/stat.h>

#define SF_PARTS 64
#define SF_PATH 1024
#define SF_BDVD_DRIVE 0x101000000000006ULL
#define SF_BLANK "/dev_hdd0/tmp/spookyflow/sf_blank.iso"

static int sf_cobra_version(uint32_t *ver)
{
    lv2syscall2(8, 0x7000, (uint64_t)ver);
    return_to_user_prog(int);
}
static int sf_cobra_disc_type(uint32_t *type)
{
    lv2syscall4(8, 0x7020, (uint64_t)type, 0, 0);
    return_to_user_prog(int);
}
static int sf_cobra_event(uint64_t event, uint64_t arg)
{
    lv2syscall4(8, 0x7022, event, arg, SF_BDVD_DRIVE);
    return_to_user_prog(int);
}
static int sf_cobra_umount(void)
{
    lv2syscall1(8, 0x702C);
    return_to_user_prog(int);
}
static int sf_cobra_map(const char *from, const char *to)
{
    /* Cobra syscall 35 path redirect, identical method to IRISMAN. */
    lv2syscall2(35, (uint64_t)from, (uint64_t)to);
    return_to_user_prog(int);
}
static int sf_cobra_mount_iso(const char **paths, int count)
{
    uint32_t ptr[SF_PARTS];
    int i;
    if (!paths || count < 1 || count > SF_PARTS) return -1;
    for (i = 0; i < count; ++i) ptr[i] = (uint32_t)(uintptr_t)paths[i];
    lv2syscall3(8, 0x7024, (uint64_t)count, (uint64_t)ptr);
    return_to_user_prog(int);
}
static int sf_exists(const char *path, int folder)
{
    struct stat s;
    if (stat(path, &s) != 0) return 0;
    return folder ? S_ISDIR(s.st_mode) : S_ISREG(s.st_mode);
}
static int sf_ends(const char *path, const char *ext)
{
    size_t a = strlen(path), b = strlen(ext);
    return a >= b && strcasecmp(path+a-b, ext) == 0;
}
static int sf_safe_path(const char *path, int folder)
{
    const char *tail;
    size_t n;
    if (!path || !(n=strlen(path)) || n > 960) return 0;
    if (!strncmp(path, "/dev_hdd0/", 10)) tail = path+10;
    else if (!strncmp(path, "/dev_usb00", 10) && path[10]>='0' && path[10]<='7' && path[11]=='/')
        tail = path+12;
    else return 0;
    if (strstr(path,"/../") || strstr(path,"/./") || strstr(path,"//") ||
        strstr(path,"\\") || strchr(path,'\n') || strchr(path,'\r') ||
        sf_ends(path,"/..") || sf_ends(path,"/.")) return 0;
    if (folder)
        return (!strncmp(tail,"GAMES/",6) || !strncmp(tail,"GAMEZ/",6)) && strlen(tail)>6;
    return !strncmp(tail,"PS3ISO/",7) && (sf_ends(path,".iso") || sf_ends(path,".iso.0"));
}
static void sf_fake_eject(void)
{
    (void)sf_cobra_event(4,0);
    (void)sf_cobra_event(8,0);
}
static int sf_fake_insert(uint32_t old_type)
{
    int a = sf_cobra_event(7,0);
    int b = sf_cobra_event(3,((uint64_t)old_type)<<32);
    return (a==0 && b==0) ? 0 : -1;
}

/* Minimal PS3 blank disc scaffold, originally based on IRISMAN/cobra.c.
 * The folder containing PS3_GAME is mapped over /dev_bdvd. */
static void sf_root_record(unsigned char *p, size_t o, int sector)
{
    p[o]=0x28; p[o+2]=p[o+9]=(unsigned char)sector;
    p[o+0xB]=p[o+0x10]=8; p[o+0x12]=0x6f; p[o+0x13]=7;
    p[o+0x14]=0x16; p[o+0x15]=2;
    p[o+0x16]=0x2b; p[o+0x17]=p[o+0x19]=2;
    p[o+0x1c]=p[o+0x1f]=p[o+0x20]=1;
}
static int sf_make_blank(const char *title_id)
{
    unsigned char *b = (unsigned char *)calloc(1,128*1024);
    const char *id=(title_id && strlen(title_id)==9) ? title_id : "BLUS00000";
    const char *vol="PS3VOLUME";
    FILE *fp; size_t written; int i;
    if (!b) return -1;
    b[3]=2; b[0x17]=0x3f;
    memcpy(b+0x800,"PlayStation3",12);
    memcpy(b+0x810,id,4); b[0x814]='-'; memcpy(b+0x815,id+4,5);
    memset(b+0x81A,' ',0x16);
    b[0x8000]=1; memcpy(b+0x8001,"CD001",5); b[0x8006]=1;
    memset(b+0x8008,' ',0x20); memcpy(b+0x8028,vol,9);
    memset(b+0x8031,' ',0x17);
    b[0x8050]=b[0x8057]=0x40;
    b[0x8078]=b[0x807B]=b[0x807C]=b[0x807F]=1;
    b[0x8081]=b[0x8082]=8;
    b[0x8084]=b[0x808B]=0xA; b[0x808C]=0x14;
    b[0x8097]=0x15; b[0x809C]=0x22;
    b[0x809E]=b[0x80A5]=0x18;
    b[0x80A7]=b[0x80AC]=8; b[0x80AE]=0x6f;
    b[0x80AF]=7; b[0x80B0]=0x16; b[0x80B1]=2;
    b[0x80B2]=0x2b; b[0x80B3]=b[0x80B5]=2;
    b[0x80B8]=b[0x80BB]=b[0x80BC]=1;
    memcpy(b+0x80BE,vol,9); memset(b+0x80C7,' ',0x266);
    memcpy(b+0x832D,"2011072202451800",16);
    memcpy(b+0x833E,"0000000000000000",16);
    memcpy(b+0x834F,"0000000000000000",16);
    memcpy(b+0x8360,"0000000000000000",16);
    b[0x8371]=1;
    b[0x8800]=2; memcpy(b+0x8801,"CD001",5); b[0x8806]=1;
    for(i=0;i<9;i++) b[0x8829+2*i]=(unsigned char)vol[i];
    b[0x8850]=b[0x8857]=0x40;
    memcpy(b+0x8858,"%/@",3);
    b[0x8878]=b[0x887B]=b[0x887C]=b[0x887F]=1;
    b[0x8881]=b[0x8882]=8;
    b[0x8884]=b[0x888B]=0xA; b[0x888C]=0x16;
    b[0x8897]=0x17; b[0x889C]=0x22;
    b[0x889E]=b[0x88A5]=0x19;
    b[0x88A7]=b[0x88AC]=8; b[0x88AE]=0x6f;
    b[0x88AF]=7; b[0x88B0]=0x16; b[0x88B1]=2;
    b[0x88B2]=0x2b; b[0x88B3]=b[0x88B5]=2;
    b[0x88B8]=b[0x88BB]=b[0x88BC]=1;
    for(i=0;i<9;i++) b[0x88BF+2*i]=(unsigned char)vol[i];
    memcpy(b+0x8B2D,"2011072202451800",16);
    memcpy(b+0x8B3E,"0000000000000000",16);
    memcpy(b+0x8B4F,"0000000000000000",16);
    memcpy(b+0x8B60,"0000000000000000",16);
    b[0x8B71]=1;
    b[0x9000]=0xff; memcpy(b+0x9001,"CD001",5);
    b[0xA000]=b[0xA006]=b[0xA800]=b[0xA807]=1;
    b[0xA002]=b[0xA805]=0x18;
    b[0xB000]=b[0xB006]=b[0xB800]=b[0xB807]=1;
    b[0xB002]=b[0xB805]=0x19;
    sf_root_record(b,0xC000,0x18); sf_root_record(b,0xC028,0x18);
    sf_root_record(b,0xC800,0x19); sf_root_record(b,0xC828,0x19);
    fp=fopen(SF_BLANK,"wb");
    if (!fp) {free(b);return -1;}
    written=fwrite(b,1,128*1024,fp);
    if (fclose(fp)) written=0;
    free(b);
    return written==128*1024 ? 0 : -1;
}
int sf_cobra_mount_ps3(const char *path, int folder, const char *title_id)
{
    uint32_t version=0xffffffffu, physical=0;
    const char *files[SF_PARTS];
    char parts[SF_PARTS][SF_PATH];
    char sfo[SF_PATH];
    size_t len; int n=0,i,rc=0;
    if (!sf_safe_path(path,folder)) return -2;
    if (!sf_exists(path,folder)) return -3;
    if (folder && (snprintf(sfo,sizeof(sfo),"%s/PS3_GAME/PARAM.SFO",path)>=(int)sizeof(sfo) ||
                   !sf_exists(sfo,0))) return -4;
    if (sf_cobra_version(&version)!=0 || version==0xffffffffu) return -5;
    if (sf_cobra_disc_type(&physical)!=0) return -6;

    if (!folder) {
        if (sf_ends(path,".iso.0")) {
            len=strlen(path);
            for(i=0;i<SF_PARTS;i++) {
                int r=snprintf(parts[i],SF_PATH,"%.*s%d",(int)(len-1),path,i);
                if (r<=0 || r>=SF_PATH || !sf_exists(parts[i],0)) break;
                files[n++]=parts[i];
            }
        } else files[n++]=path;
        if (!n) return -7;
    }

    sf_fake_eject();
    if (sf_cobra_map("/dev_bdvd",NULL)!=0 ||
        sf_cobra_map("//dev_bdvd",NULL)!=0 ||
        sf_cobra_map("/app_home",NULL)!=0) {rc=-8; goto fail;}
    (void)sf_cobra_umount(); /* Nothing mounted is an acceptable state. */
    if (!folder) {
        if (sf_cobra_mount_iso(files,n)!=0) {rc=-9;goto fail;}
    } else {
        if (sf_cobra_map("/dev_bdvd",path)!=0 ||
            sf_cobra_map("//dev_bdvd",path)!=0) {rc=-10;goto fail;}
        if (!physical) {
            const char *blank[1]={SF_BLANK};
            if (sf_make_blank(title_id)!=0 || sf_cobra_mount_iso(blank,1)!=0) {
                rc=-11;goto fail;
            }
        }
    }
    if (sf_fake_insert(physical)!=0) {rc=-12;goto fail;}
    return 0;
fail:
    (void)sf_cobra_map("/dev_bdvd",NULL);
    (void)sf_cobra_map("//dev_bdvd",NULL);
    (void)sf_fake_insert(physical);
    return rc ? rc : -13;
}
