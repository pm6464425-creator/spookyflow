/* SpookyFlow V15.23 PS1 Cobra mount - experimental, no PS3 firmware writes.
 * Cobra's PSX syscall supports one physical image with multiple logical tracks.
 * For multiple BIN files and other unsupported formats, return -41 BEFORE
 * eject/unmount so the unchanged webMAN route can mount it instead.
 * Cobra declarations follow IRISMAN/include/cobra.h (GPL-3.0-or-later).
 */
#include "sf_ps1_mount.h"
#include "sf_ps1_native_plan.h"
#include "sf_ps1_resolve.h"
#include "ps3_platform.h"
#include "log.h"
#include <cobra.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <strings.h>
#include <sys/stat.h>
#include <errno.h>
#include <ppu-lv2.h>

static SfPs1NativePlan s_plan; /* no 6k stack allocations on main PPU thread */
static TrackDef s_cobra_tracks[SF_PS1_MAX_TRACKS];
static char s_cue[SF_PS1_IMAGE_MAX],s_text[32769];
static char s_cue_image_resolved[SF_PS1_IMAGE_MAX];

static int ends(const char *path,const char *ext)
{
    size_t n=strlen(path),l=strlen(ext);
    return n>=l && !strcasecmp(path+n-l,ext);
}
static int is_ps1_path(const char *path)
{
    if(!path || path[0]!='/')return 0;
    if(!strncmp(path,"/dev_hdd0/PSXISO/",17) ||
       !strncmp(path,"/dev_hdd0/PSXGAMES/",19))return 1;
    if(!strncmp(path,"/dev_usb00",10) && path[10]>='0' && path[10]<='7' &&
       (strstr(path+11,"/PSXISO/") || strstr(path+11,"/PSXGAMES/")))return 1;
    return 0;
}
static int valid_image(const char *path,uint32_t last_track)
{
    struct stat st;
    if(stat(path,&st)!=0){
        log_printf("PS1_DIAG STAGE=IMAGE_STAT RESULT=FAIL ERRNO=%d PATH=%s",
                   errno,path);
        return 0;
    }
    if(!S_ISREG(st.st_mode) || st.st_size<0){
        log_printf("PS1_DIAG STAGE=IMAGE_STAT RESULT=NOT_REGULAR PATH=%s",path);
        return 0;
    }
    log_printf("PS1_DIAG STAGE=IMAGE_SIZE BYTES=%llu REMAINDER_2352=%llu "
               "REMAINDER_2048=%llu LAST_TRACK=%u",
               (unsigned long long)st.st_size,
               (unsigned long long)((uint64_t)st.st_size%2352u),
               (unsigned long long)((uint64_t)st.st_size%2048u),last_track);
    if(st.st_size<2352*16 || (st.st_size%2352)!=0 ||
       (uint64_t)(st.st_size/2352)<=last_track){
        log_printf("PS1_DIAG STAGE=IMAGE_VALIDATE RESULT=UNSUPPORTED_SECTOR_OR_TRACK_LAYOUT");
        return 0;
    }
    return 1;
}
static int read_cue(const char *path)
{
    FILE *f=fopen(path,"rb");size_t n;
    int result;
    if(!f){
        log_printf("PS1_DIAG STAGE=CUE_OPEN RESULT=FAIL ERRNO=%d PATH=%s",errno,path);
        return -41;
    }
    n=fread(s_text,1,sizeof(s_text)-1,f);
    result=ferror(f) || (n==sizeof(s_text)-1 && !feof(f)) ? -41:0;
    if(fclose(f)!=0)result=-41;
    if(result || n==0){
        log_printf("PS1_DIAG STAGE=CUE_READ RESULT=FAIL BYTES=%u",
                   (unsigned)n);
        return -41;
    }
    s_text[n]=0;
    if(sf_ps1_plan_cue(path,s_text,n,&s_plan)!=0){
        int files=0,tracks=0;
        const char *p=s_text;
        /* Diagnostic only, never edit the CUE, ISO or track mapping. */
        while(*p){
            const char *line=p;
            while(*p && *p!='\n' && *p!='\r')p++;
            while(*line==' ' || *line=='\t')line++;
            if(!strncasecmp(line,"FILE ",5))files++;
            if(!strncasecmp(line,"TRACK ",6))tracks++;
            while(*p=='\r' || *p=='\n')p++;
        }
        log_printf("PS1_DIAG STAGE=CUE_PARSE RESULT=UNSUPPORTED FILE_COUNT=%d "
                   "TRACK_COUNT=%d MULTI_BIN=%d",
                   files,tracks,files>1);
        return -41;
    }
    log_printf("PS1_DIAG STAGE=CUE_PARSE RESULT=OK TRACKS=%u IMAGE=%s",
               s_plan.num_tracks,s_plan.image);
    if(!is_ps1_path(s_plan.image)){
        log_printf("PS1_DIAG STAGE=CUE_REFERENCED_IMAGE RESULT=UNSUPPORTED_PATH");
        return -41;
    }
    /* The CUE's FILE may use a regional name ("Metal Slug X (Europe)")
       while the actual BIN was renamed to "Metal Slug X.bin". Resolve only
       a UNIQUE same-title match in the original directory; never rewrite
       the user's .CUE or silently mount some unrelated .BIN. */
    if(sf_ps1_resolve_existing(s_plan.image,s_cue_image_resolved,
                              sizeof(s_cue_image_resolved))!=0){
        log_printf("PS1_DIAG STAGE=CUE_REFERENCED_IMAGE RESULT=NO_SAFE_MATCH");
        return -41;
    }
    strcpy(s_plan.image,s_cue_image_resolved);
    if(!valid_image(s_plan.image,s_plan.tracks[s_plan.num_tracks-1].lba))
        return -41;
    return 0;
}
static int ps1_cobra_version(uint32_t *v)
{
    lv2syscall2(8,0x7000,(uint64_t)v);
    return_to_user_prog(int);
}
static int prepare(const char *path,int emulator)
{
    size_t len;
    log_printf("PS1_DIAG STAGE=SELECTION PATH=%s EMULATOR_PREF=%d",path?path:"(null)",emulator);
    if(!is_ps1_path(path)){
        log_printf("PS1_DIAG STAGE=PATH_CHECK RESULT=UNSUPPORTED_ROOT");
        return -41;
    }
    /* Native Cobra PSX mount controls the disc image, not the XMB's choice
       of ps1_emu vs ps1_netemu. V15.23 incorrectly rejected EVERY title
       with per-game Netemu selected, forcing webMAN even for valid BIN/CUE.
       Mount natively and let the system select its available PS1 emulator;
       report that Netemu preference was not forcibly applied. */
    if(emulator==1)
        log_printf("PS1_DIAG EMULATOR_PREF=NETEMU NOTE=NATIVE_COBRA_DOES_NOT_FORCE_EMU");
    if(ends(path,".ntfs[PSXISO]") || ends(path,".iso.0")){
        log_printf("PS1_DIAG STAGE=FORMAT RESULT=UNSUPPORTED_NTFS_OR_SPLIT_IMAGE");
        return -41;
    }
    if(ends(path,".cue"))return read_cue(path);
    if(!ends(path,".bin") && !ends(path,".img") && !ends(path,".iso")){
        log_printf("PS1_DIAG STAGE=FORMAT RESULT=UNSUPPORTED_EXTENSION "
                   "NOTE=CHD_PBP_MULTIFILE_REQUIRE_DIFFERENT_BACKEND");
        return -41;
    }
    /* When accompanying CUE exists, we must use its track table, never
       silently drop Red Book audio. Unsupported CUE => webMAN fallback. */
    len=strlen(path);
    if(len-4+4>=sizeof(s_cue)){
        log_printf("PS1_DIAG STAGE=PATH_LENGTH RESULT=TOO_LONG");
        return -41;
    }
    memcpy(s_cue,path,len-4);
    strcpy(s_cue+len-4,".cue");
    {
        struct stat st;
        if(stat(s_cue,&st)==0)return read_cue(s_cue);
        strcpy(s_cue+len-4,".CUE");
        if(stat(s_cue,&st)==0)return read_cue(s_cue);
    }
    memset(&s_plan,0,sizeof(s_plan));
    if(len>=sizeof(s_plan.image) || !valid_image(path,0))return -41;
    strcpy(s_plan.image,path);
    s_plan.num_tracks=1;
    s_plan.tracks[0].lba=0;
    s_plan.tracks[0].is_audio=0;
    log_printf("PS1_DIAG STAGE=PLAN RESULT=OK TRACKS=1 MODE=2352_SINGLE_DATA IMAGE=%s",
               s_plan.image);
    return 0;
}
static int mount_prepared(void)
{
    const char *path=s_plan.image;
    uint32_t v=0xffffffffu,real=0,effective=0,virt=0;
    int rc,ejected=0,mounted=0;
    unsigned i;
    /* Cobra must be present. Failed preflight has no disc-side effects. */
    rc=ps1_cobra_version(&v);
    log_printf("PS1_DIAG STAGE=COBRA_VERSION SYS_RC=%d VERSION=%08x",
               rc,(unsigned)v);
    if(rc!=0 || v==0xffffffffu || v==0){
        log_printf("PS1_DIAG STAGE=COBRA_PREFLIGHT RESULT=UNAVAILABLE "
                   "HINT=VERIFY_HEN_ENABLED RC=%d",rc);
        return -41;
    }
    rc=cobra_get_disc_type(&real,&effective,&virt);
    if(rc!=0){
        log_printf("PS1_DIAG STAGE=DISC_TYPE RESULT=ERROR SYS_RC=%d",rc);
        return -42;
    }
    log_printf("PS1_DIAG STAGE=DISC_TYPE RESULT=OK PHYSICAL=%08x "
               "EFFECTIVE=%08x VIRTUAL=%08x",real,effective,virt);
    for(i=0;i<s_plan.num_tracks;i++){
        s_cobra_tracks[i].lba=s_plan.tracks[i].lba;
        s_cobra_tracks[i].is_audio=s_plan.tracks[i].is_audio;
    }
    log_printf("PS1_MOUNT BEGIN IMAGE=%s TRACKS=%u COBRA=%08x DISC=%08x",
               s_plan.image,s_plan.num_tracks,(unsigned)v,(unsigned)real);
    ejected=1;
    rc=cobra_send_fake_disc_eject_event();
    log_printf("PS1_DIAG STAGE=FAKE_EJECT SYS_RC=%d",rc);
    if(rc!=0){rc=-43;goto fail;}
    {
        int unmount_rc=cobra_umount_disc_image();
        log_printf("PS1_DIAG STAGE=UNMOUNT_PREVIOUS SYS_RC=%d",unmount_rc);
        /* Nothing previously mounted is a valid state. */
    }
    rc=cobra_mount_psx_disc_image(s_plan.image,s_cobra_tracks,s_plan.num_tracks);
    log_printf("PS1_DIAG STAGE=MOUNT_PSX_7027 SYS_RC=%d TRACKS=%u IMAGE=%s",
               rc,s_plan.num_tracks,s_plan.image);
    if(rc!=0){rc=-44;goto fail;}
    mounted=1;
    rc=cobra_send_fake_disc_insert_event();
    log_printf("PS1_DIAG STAGE=FAKE_INSERT SYS_RC=%d",rc);
    if(rc!=0){rc=-45;goto fail;}
    log_printf("PS1_MOUNT RESULT=OK TRACKS=%u IMAGE=%s",s_plan.num_tracks,s_plan.image);
    return 0;
fail:
    if(mounted)(void)cobra_umount_disc_image();
    if(ejected)(void)cobra_send_fake_disc_insert_event();
    log_printf("PS1_DIAG STAGE=ROLLBACK RESULT=ERROR RC=%d IMAGE=%s",
               rc,s_plan.image);
    log_printf("PS1_MOUNT RESULT=ERROR RC=%d IMAGE=%s",rc,s_plan.image);
    return rc;
}

int sf_ps1_mount(const char *path,int emulator)
{
    int rc=prepare(path,emulator);
    if(rc!=0){
        log_printf("PS1_DIAG STAGE=PREFLIGHT RESULT=FALLBACK RC=%d PATH=%s",
                   rc,path?path:"(null)");
        return -41;
    }
    return mount_prepared();
}
int sf_ps1_mount_staged(const char *path,const SfPs1Track *tracks,unsigned n)
{
    size_t len;
    static const char prefix[]=SF_DATA_DIR "/ps1_unido_";
    if(!path || !tracks || n<2 || n>SF_PS1_MAX_TRACKS ||
       strncmp(path,prefix,sizeof(prefix)-1)!=0)return -81;
    len=strlen(path);
    if(len>=sizeof(s_plan.image) || !ends(path,".bin"))return -81;
    if(!valid_image(path,tracks[n-1].lba))return -82;
    memset(&s_plan,0,sizeof(s_plan));
    strcpy(s_plan.image,path);
    s_plan.num_tracks=n;
    for(unsigned i=0;i<n;i++){
        if((i==0 && (tracks[i].lba!=0 || tracks[i].is_audio)) ||
           (i>0 && tracks[i].lba<=tracks[i-1].lba))
            return -83;
        s_plan.tracks[i]=tracks[i];
    }
    log_printf("PS1_DIAG STAGE=MULTIBIN_STAGED RESULT=VERIFIED "
               "TRACKS=%u IMAGE=%s",n,path);
    return mount_prepared();
}
