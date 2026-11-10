/*
 * SpookyFlow direct PS3 Cobra mount backend. EXPERIMENTAL / UNTESTED ON HARDWARE.
 * The disc-image and folder-mapping techniques follow IRISMAN/source/cobra.c
 * (GPL-3.0-or-later); retain licenses on redistribution.
 * No fan control, network server, webMAN, or boot-time plugin is installed.
 */
#include "sf_cobra_mount.h"
#include "log.h"
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
/* The original 64 x 1024-byte local matrix consumed 64 KiB of main-thread
 * stack on EVERY launch, even folder games. It may overflow the PS3 PPU
 * thread stack before BEGIN_MOUNT can be persisted. Allocate it in BSS:
 * launch is main-thread synchronous, so there are no concurrent uses. */
static char s_iso_part_paths[SF_PARTS][SF_PATH];

/* Early diagnostic probe is INDEPENDENT of the Cobra mount journal.
 * It is first written at process entry before any Tiny3D/UI initialization.
 * We close after every checkpoint so a crash leaves its last stage visible.
 * If the normal directory is unavailable, fall back to /dev_hdd0/tmp/. */
#define SF_BOOT_PROBE "/dev_hdd0/tmp/spookyflow/cobra_startup_probe.txt"
#define SF_BOOT_PROBE_ALT "/dev_hdd0/tmp/cobra_startup_probe_fallback.txt"
#define SF_SELECT_PROBE "/dev_hdd0/tmp/spookyflow/cobra_selection_probe.txt"
#define SF_SELECT_PROBE_ALT "/dev_hdd0/tmp/cobra_selection_probe_fallback.txt"
#define SF_DIAG_BUILD "EARLY_PROBE_20261109_R1"
static int sf_probe_write(const char *where, const char *stage,
                          const char *path, int folder)
{
    FILE *fp=fopen(where,"wb");
    int ok=1;
    if (!fp) return -1;
    if (fprintf(fp,"BUILD=%s\nSTAGE=%s\nFORMAT=%s\nPATH=%s\n",
                SF_DIAG_BUILD, stage ? stage : "?",
                folder ? "FOLDER" : "ISO", path ? path : "(none)") < 0) ok=0;
    if (fflush(fp)!=0) ok=0;
    if (fclose(fp)!=0) ok=0;
    return ok ? 0 : -1;
}
int sf_cobra_diag_startup(const char *stage)
{
    int rc=sf_probe_write(SF_BOOT_PROBE,stage,NULL,0);
    if (rc!=0) rc=sf_probe_write(SF_BOOT_PROBE_ALT,stage,NULL,0);
    return rc;
}
int sf_cobra_diag_selection(const char *path, int folder)
{
    int rc=sf_probe_write(SF_SELECT_PROBE,"GAME_SELECTED_BEFORE_MOUNT",
                          path,folder);
    if (rc!=0)
        rc=sf_probe_write(SF_SELECT_PROBE_ALT,
                          "GAME_SELECTED_BEFORE_MOUNT",path,folder);
    log_printf("cobra: pre-mount probe %s for %s (%s)",
               rc==0?"saved":"FAILED",path?path:"(null)",
               folder?"FOLDER":"ISO");
    return rc;
}


/* Persistent per-syscall journal. A snapshot is CLOSED before each risky
 * syscall; the last BEFORE_* step remains if the PS3 freezes during it. */
#define SF_DIAG_LAST "/dev_hdd0/tmp/spookyflow/cobra_last_mount.txt"
#define SF_DIAG_TRACE "/dev_hdd0/tmp/spookyflow/cobra_trace.txt"
#define SF_DIAG_INTERRUPTED "/dev_hdd0/tmp/spookyflow/cobra_previous_interruption.txt"
static char sf_diag_path[SF_PATH];
static int sf_diag_folder;
static int sf_diag_active;

static int sf_diag_write(const char *stage, int rc, int state)
{
    const char *status = state==1?"IN_PROGRESS":(state==2?"ERROR":"SUCCESS");
    FILE *f;
    int good;
    long size;
    if (!sf_diag_active) return 0;
    f=fopen(SF_DIAG_LAST,"wb");
    if (!f) return -1;
    good = fprintf(f,"STATE=%s\nSTAGE=%s\nRC=%d\nFORMAT=%s\nPATH=%s\n",
                   status,stage,rc,sf_diag_folder?"FOLDER":"ISO",sf_diag_path)>0;
    if (fflush(f)!=0) good=0;
    if (fclose(f)!=0) good=0;
    if (!good) return -1;
    f=fopen(SF_DIAG_TRACE,"ab");
    if (f) {
        size=-1;
        if (fseek(f,0,SEEK_END)==0) size=ftell(f);
        if (size>96L*1024L) {
            fclose(f);
            f=fopen(SF_DIAG_TRACE,"wb");
            if (f) (void)fprintf(f,"# history rotated after 96 KiB\n");
        }
        if (f) {
            (void)fprintf(f,"%s | %s | rc=%d | %s | %s\n",
                          status,stage,rc,sf_diag_folder?"FOLDER":"ISO",sf_diag_path);
            (void)fflush(f);
            (void)fclose(f);
        }
    }
    return 0;
}
static int sf_diag_error(const char *stage, int rc)
{
    (void)sf_diag_write(stage,rc,2);
    log_printf("cobra: ERROR %s rc=%d for %s",stage,rc,sf_diag_path);
    return rc;
}
void sf_cobra_diag_report_previous(void)
{
    char text[1600];
    FILE *f=fopen(SF_DIAG_LAST,"rb"),*out;
    size_t n;
    if (!f) return;
    n=fread(text,1,sizeof(text)-1,f);
    fclose(f);
    text[n]='\0';
    if (strncmp(text,"STATE=IN_PROGRESS\n",18)!=0) return;
    out=fopen(SF_DIAG_INTERRUPTED,"wb");
    if (out) {
        (void)fprintf(out,"PREVIOUS_MOUNT_INTERRUPTED=1\n"
                 "WARNING=Possible system hang, power loss or forced exit.\n%s",text);
        (void)fclose(out);
    }
    log_printf("cobra: prior mount interrupted; examine cobra_previous_interruption.txt");
}
static int sf_diag_before(const char *call)
{
    char stage[100];
    snprintf(stage,sizeof(stage),"BEFORE_%s",call);
    return sf_diag_write(stage,0,1);
}
static void sf_diag_after(const char *call, int rc)
{
    char stage[100];
    snprintf(stage,sizeof(stage),"AFTER_%s",call);
    (void)sf_diag_write(stage,rc,1);
}


/* Raw methods preserve the original Cobra syscall signatures. */
static int sf_raw_version(uint32_t *ver)
{
    lv2syscall2(8, 0x7000, (uint64_t)ver);
    return_to_user_prog(int);
}
static int sf_raw_disc_type(uint32_t *type)
{
    lv2syscall4(8, 0x7020, (uint64_t)type, 0, 0);
    return_to_user_prog(int);
}
static int sf_raw_event(uint64_t event, uint64_t arg)
{
    lv2syscall4(8, 0x7022, event, arg, SF_BDVD_DRIVE);
    return_to_user_prog(int);
}
static int sf_raw_umount(void)
{
    lv2syscall1(8, 0x702C);
    return_to_user_prog(int);
}
static int sf_raw_map(const char *from, const char *to)
{
    lv2syscall2(35, (uint64_t)from, (uint64_t)to);
    return_to_user_prog(int);
}
static int sf_raw_mount_iso(uint32_t *ptr, int count)
{
    lv2syscall3(8, 0x7024, (uint64_t)count, (uint64_t)ptr);
    return_to_user_prog(int);
}
static int sf_cobra_version(uint32_t *ver)
{
    int rc;
    if (sf_diag_before("VERSION_7000")) return -20;
    rc=sf_raw_version(ver);
    sf_diag_after("VERSION_7000",rc);
    return rc;
}
static int sf_cobra_disc_type(uint32_t *type)
{
    int rc;
    if (sf_diag_before("DISC_TYPE_7020")) return -20;
    rc=sf_raw_disc_type(type);
    sf_diag_after("DISC_TYPE_7020",rc);
    return rc;
}
static int sf_cobra_event(uint64_t event, uint64_t arg)
{
    int rc;
    char stage[45];
    snprintf(stage,sizeof(stage),"DISC_EVENT_7022_%u",(unsigned int)event);
    if (sf_diag_before(stage)) return -20;
    rc=sf_raw_event(event,arg);
    sf_diag_after(stage,rc);
    return rc;
}
static int sf_cobra_umount(void)
{
    int rc;
    if (sf_diag_before("UMOUNT_702C")) return -20;
    rc=sf_raw_umount();
    sf_diag_after("UMOUNT_702C",rc);
    return rc;
}
static int sf_cobra_map(const char *from, const char *to)
{
    int rc;
    char stage[65];
    snprintf(stage,sizeof(stage),"MAP_35_%s_%s",
             !strcmp(from,"/dev_bdvd")?"BDVD":
             (!strcmp(from,"//dev_bdvd")?"BDVD2":"APPHOME"),
             to?"SET":"CLEAR");
    if (sf_diag_before(stage)) return -20;
    rc=sf_raw_map(from,to);
    sf_diag_after(stage,rc);
    return rc;
}
static int sf_cobra_mount_iso(const char **paths, int count)
{
    uint32_t ptr[SF_PARTS];
    int rc,i;
    if (!paths || count < 1 || count > SF_PARTS) return -1;
    for (i=0;i<count;i++) ptr[i]=(uint32_t)(uintptr_t)paths[i];
    if (sf_diag_before("MOUNT_PS3ISO_7024")) return -20;
    rc=sf_raw_mount_iso(ptr,count);
    sf_diag_after("MOUNT_PS3ISO_7024",rc);
    return rc;
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
    char sfo[SF_PATH];
    size_t len; int n=0,i,rc=0;
    char eboot[SF_PATH];
    sf_diag_folder = !!folder;
    snprintf(sf_diag_path,sizeof(sf_diag_path),"%s",path?path:"(null)");
    sf_diag_active=1;
    if (sf_diag_write("BEGIN_MOUNT",0,1)!=0) return -20;
    /* The game must be complete before changing virtual disc state. */
    if (!sf_safe_path(path,folder)) return sf_diag_error("INVALID_PATH",-2);
    if (!sf_exists(path,folder)) return sf_diag_error("MISSING_GAME",-3);
    if (folder && (snprintf(sfo,sizeof(sfo),"%s/PS3_GAME/PARAM.SFO",path)>=(int)sizeof(sfo) ||
                   !sf_exists(sfo,0))) return sf_diag_error("MISSING_PARAM_SFO",-4);
    if (folder && (snprintf(eboot,sizeof(eboot),"%s/PS3_GAME/USRDIR/EBOOT.BIN",path)>=(int)sizeof(eboot) ||
                   !sf_exists(eboot,0))) return sf_diag_error("MISSING_EBOOT_BIN",-14);
    if (sf_diag_write("PREFLIGHT_OK",0,1)!=0) return -20;
    if (sf_cobra_version(&version)!=0 || version==0xffffffffu)
        return sf_diag_error("COBRA_NOT_AVAILABLE",-5);
    if (sf_cobra_disc_type(&physical)!=0) return sf_diag_error("DISC_TYPE_FAILED",-6);

    if (!folder) {
        if (sf_ends(path,".iso.0")) {
            len=strlen(path);
            for(i=0;i<SF_PARTS;i++) {
                int r=snprintf(s_iso_part_paths[i],SF_PATH,"%.*s%d",(int)(len-1),path,i);
                if (r<=0 || r>=SF_PATH || !sf_exists(s_iso_part_paths[i],0)) break;
                files[n++]=s_iso_part_paths[i];
            }
        } else files[n++]=path;
        if (!n) return sf_diag_error("NO_ISO_PARTS",-7);
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
            int blank_rc;
            if (sf_diag_before("BUILD_BLANK_ISO")) {rc=-20;goto fail;}
            blank_rc=sf_make_blank(title_id);
            sf_diag_after("BUILD_BLANK_ISO",blank_rc);
            if (blank_rc!=0 || sf_cobra_mount_iso(blank,1)!=0) {
                rc=-11;goto fail;
            }
        }
    }
    if (sf_fake_insert(physical)!=0) {rc=-12;goto fail;}
    (void)sf_diag_write("MOUNT_SUCCEEDED",0,0);
    sf_diag_active=0;
    return 0;
fail:
    (void)sf_cobra_map("/dev_bdvd",NULL);
    (void)sf_cobra_map("//dev_bdvd",NULL);
    (void)sf_fake_insert(physical);
    rc=sf_diag_error("MOUNT_FAILED",rc?rc:-13);
    sf_diag_active=0;
    return rc;
}
