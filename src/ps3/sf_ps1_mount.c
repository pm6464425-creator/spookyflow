/* SpookyFlow V15.23 PS1 Cobra mount - experimental, no PS3 firmware writes.
 * Cobra's PSX syscall supports one physical image with multiple logical tracks.
 * For multiple BIN files and other unsupported formats, return -41 BEFORE
 * eject/unmount so the unchanged webMAN route can mount it instead.
 * Cobra declarations follow IRISMAN/include/cobra.h (GPL-3.0-or-later).
 */
#include "sf_ps1_mount.h"
#include "sf_ps1_native_plan.h"
#include "log.h"
#include <cobra.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <strings.h>
#include <sys/stat.h>
#include <ppu-lv2.h>

static SfPs1NativePlan s_plan; /* no 6k stack allocations on main PPU thread */
static TrackDef s_cobra_tracks[SF_PS1_MAX_TRACKS];
static char s_cue[SF_PS1_IMAGE_MAX],s_text[32769];

static int ends(const char *path,const char *ext)
{
    size_t n=strlen(path),l=strlen(ext);
    return n>=l && !strcasecmp(path+n-l,ext);
}
static int is_ps1_path(const char *path)
{
    if(!path || path[0]!='/')return 0;
    if(!strncmp(path,"/dev_hdd0/PSXISO/",16) ||
       !strncmp(path,"/dev_hdd0/PSXGAMES/",18))return 1;
    if(!strncmp(path,"/dev_usb00",10) && path[10]>='0' && path[10]<='7' &&
       (strstr(path+11,"/PSXISO/") || strstr(path+11,"/PSXGAMES/")))return 1;
    return 0;
}
static int valid_image(const char *path,uint32_t last_track)
{
    struct stat st;
    if(stat(path,&st)!=0 || !S_ISREG(st.st_mode) ||
       st.st_size < 2352*16 || (st.st_size % 2352)!=0 ||
       (uint64_t)(st.st_size / 2352) <= last_track)return 0;
    return 1;
}
static int read_cue(const char *path)
{
    FILE *f=fopen(path,"rb");size_t n;
    int result;
    if(!f)return -41;
    n=fread(s_text,1,sizeof(s_text)-1,f);
    result=ferror(f) || (n==sizeof(s_text)-1 && !feof(f)) ? -41:0;
    if(fclose(f)!=0)result=-41;
    if(result || n==0)return -41;
    s_text[n]=0;
    if(sf_ps1_plan_cue(path,s_text,n,&s_plan)!=0)return -41;
    if(!is_ps1_path(s_plan.image) ||
       !valid_image(s_plan.image,s_plan.tracks[s_plan.num_tracks-1].lba))
        return -41;
    return 0;
}
static int prepare(const char *path,int emulator)
{
    size_t len;
    if(!is_ps1_path(path) || emulator==1)return -41;
    if(ends(path,".ntfs[PSXISO]") || ends(path,".iso.0"))return -41;
    if(ends(path,".cue"))return read_cue(path);
    if(!ends(path,".bin") && !ends(path,".img") && !ends(path,".iso"))return -41;
    /* When accompanying CUE exists, we must use its track table, never
       silently drop Red Book audio. Unsupported CUE => webMAN fallback. */
    len=strlen(path);
    if(len-4+4>=sizeof(s_cue))return -41;
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
    return 0;
}
int sf_ps1_mount(const char *path,int emulator)
{
    uint32_t v=0xffffffffu,real=0,effective=0,virt=0;
    int rc,ejected=0,mounted=0;
    unsigned i;
    rc=prepare(path,emulator);
    if(rc!=0){
        log_printf("PS1_MOUNT ROUTE=WEBMAN_FALLBACK REASON=UNSUPPORTED_IMAGE_OR_SETTINGS PATH=%s",
                   path?path:"(null)");
        return -41;
    }
    /* Cobra must be present. Failed preflight has no disc-side effects. */
    lv2syscall2(8,0x7000,(uint64_t)&v);
    __asm__ volatile(""); /* syscall result irrelevant: verify version value */
    if(v==0xffffffffu || v==0){
        log_printf("PS1_MOUNT ROUTE=WEBMAN_FALLBACK REASON=COBRA_UNAVAILABLE");
        return -41;
    }
    if(cobra_get_disc_type(&real,&effective,&virt)!=0)return -42;
    for(i=0;i<s_plan.num_tracks;i++){
        s_cobra_tracks[i].lba=s_plan.tracks[i].lba;
        s_cobra_tracks[i].is_audio=s_plan.tracks[i].is_audio;
    }
    log_printf("PS1_MOUNT BEGIN IMAGE=%s TRACKS=%u COBRA=%08x DISC=%08x",
               s_plan.image,s_plan.num_tracks,(unsigned)v,(unsigned)real);
    ejected=1;
    rc=cobra_send_fake_disc_eject_event();
    if(rc!=0){rc=-43;goto fail;}
    if(cobra_umount_disc_image()!=0){
        /* Nothing previously mounted can be a valid state. Continue. */
    }
    rc=cobra_mount_psx_disc_image(s_plan.image,s_cobra_tracks,s_plan.num_tracks);
    if(rc!=0){log_printf("PS1_MOUNT COBRA_RC=%d",rc);rc=-44;goto fail;}
    mounted=1;
    rc=cobra_send_fake_disc_insert_event();
    if(rc!=0){rc=-45;goto fail;}
    log_printf("PS1_MOUNT RESULT=OK TRACKS=%u IMAGE=%s",s_plan.num_tracks,s_plan.image);
    return 0;
fail:
    if(mounted)(void)cobra_umount_disc_image();
    if(ejected)(void)cobra_send_fake_disc_insert_event();
    log_printf("PS1_MOUNT RESULT=ERROR RC=%d IMAGE=%s",rc,s_plan.image);
    return rc;
}
