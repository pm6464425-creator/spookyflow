/*
 * SpookyFlow V15.13 EXPERIMENTAL PS2 Cobra mount, PS2ISO internal HDD only.
 * See IRISMAN/source/cobra.c and webMAN-MOD/include/mount/mount.h
 * (GPL-3.0-or-later), respecting their respective license notices.
 *
 * HEN explicitly blocks raw PS2 ISO, as documented in webMAN-MOD's own
 * mount_cobra.h. Do not fake success or patch the kernel to bypass the guard.
 * No change to PS3, PS1, BDISO or webMAN mounting backends.
 */
#include "sf_ps2_mount.h"
#include "ps2_mount_plan.h"
#include "log.h"
#include <stdint.h>
#include <cobra.h>
#include <ppu-lv2.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>

#define PS2_DIAG "/dev_hdd0/tmp/spookyflow/ps2_last_mount.txt"
#define PS2_TRACE "/dev_hdd0/tmp/spookyflow/ps2_mount_trace.txt"
static char s_paths[SF_PS2_MAX_PARTS][SF_PS2_MAX_PATH]; /* BSS; not PPU stack */

static int ps2_file(void *ctx,const char *path)
{
    struct stat st;
    (void)ctx;
    return stat(path,&st)==0 && S_ISREG(st.st_mode) && st.st_size>=2048;
}
static int raw_hen(void)
{
    lv2syscall1(8,0x1337); /* PS3HEN IS_HEN, returns 0x1337 */
    return_to_user_prog(int);
}
static int raw_version(uint32_t *v)
{
    lv2syscall2(8,0x7000,(uint64_t)v);
    return_to_user_prog(int);
}
static int raw_ps2_netemu(int setting)
{
    lv2syscall2(8,0x1ee9,(uint64_t)setting); /* Cobra ENABLE_PS2NETEMU */
    return_to_user_prog(int);
}
static int journal(const char *stage,const char *path,int rc,int active)
{
    FILE *f=fopen(PS2_DIAG,"wb");
    int ok=1;
    if(!f) return -1;
    if(fprintf(f,"BACKEND=PS2_COBRA_DIRECT\nSTATE=%s\nSTAGE=%s\nRC=%d\nPATH=%s\n",
               active?"IN_PROGRESS":rc?"ERROR":"SUCCESS",
               stage?stage:"?",rc,path?path:"(null)")<0) ok=0;
    if(fflush(f)!=0)ok=0;
    if(fclose(f)!=0)ok=0;
    if(!ok)return -1;
    f=fopen(PS2_TRACE,"ab");
    if(f) {
        long sz=-1;
        if(fseek(f,0,SEEK_END)==0)sz=ftell(f);
        if(sz>96L*1024L){fclose(f);f=fopen(PS2_TRACE,"wb");}
        if(f){
            (void)fprintf(f,"%s rc=%d path=%s\n",stage,rc,path?path:"(null)");
            (void)fflush(f);
            (void)fclose(f);
        }
    }
    return 0;
}
int sf_ps2_preflight(const char *path)
{
    uint32_t ver=0xffffffffu;
    int nparts;
    if (!path || !sf_ps2_valid_iso_path(path)) return -42;
    nparts=sf_ps2_collect_parts(path,ps2_file,NULL,s_paths);
    if (nparts<1) return nparts==-2?-43:-44;
    /* Safety: read-only environment check before changing virtual disc state. */
    if (raw_hen()==0x1337) return -41;
    if (raw_version(&ver)!=0 || ver==0xffffffffu || ver==0) return -45;
    return 0;
}
int sf_ps2_mount(const char *path,int emulator)
{
    uint32_t version=0xffffffffu,real=0,effective=0,virtual_disc=0;
    int n,rc,i,ejected=0,mounted=0;
    char *parts[SF_PS2_MAX_PARTS];
    TrackDef tracks[1];
    rc=sf_ps2_preflight(path);
    if(rc!=0) {
        log_printf("ps2: preflight refused rc=%d path=%s",rc,path?path:"(null)");
        (void)journal("PREFLIGHT_REJECTED",path,rc,0);
        return rc;
    }
    n=sf_ps2_collect_parts(path,ps2_file,NULL,s_paths);
    if(n<1)return -44;
    for(i=0;i<n;i++)parts[i]=s_paths[i];
    tracks[0].lba=0;tracks[0].is_audio=0;
    if(journal("PREFLIGHT_OK",path,0,1)!=0)return -46;
    if(raw_version(&version)!=0 || version==0xffffffffu || version==0)
        {rc=-45;goto fail;}
    if(cobra_get_disc_type(&real,&effective,&virtual_disc)!=0)
        {rc=-47;goto fail;}
    log_printf("ps2: Cobra=%08x parts=%d physical=%x effective=%x selected_emu=%d",
               (unsigned)version,n,real,effective,emulator);
    /* Respect the saved default unless user explicitly chose netemu. */
    if(emulator==1) {
        if(journal("BEFORE_SET_NETEMU",path,0,1)!=0)return -46;
        rc=raw_ps2_netemu(1);
        if(rc!=0){rc=-48;goto fail;}
    }
    if(journal("BEFORE_EJECT",path,0,1)!=0)return -46;
    ejected=1;
    rc=cobra_send_fake_disc_eject_event();
    if(rc!=0){rc=-49;goto fail;}
    /* Clear any old PS3 folder mapping before mounting a PS2 disc, matching
     * the existing Cobra PS3 backend. Never map the user's game files here. */
    if(journal("BEFORE_CLEAR_MAPS",path,0,1)!=0){rc=-46;goto fail;}
    if(sys_map_path("/dev_bdvd",NULL)!=0 ||
       sys_map_path("//dev_bdvd",NULL)!=0 ||
       sys_map_path("/app_home",NULL)!=0){rc=-52;goto fail;}
    if(journal("BEFORE_UMOUNT",path,0,1)!=0){rc=-46;goto fail;}
    (void)cobra_umount_disc_image(); /* No previous image is a normal state. */
    if(journal("BEFORE_MOUNT_7028",path,0,1)!=0){rc=-46;goto fail;}
    rc=cobra_mount_ps2_disc_image(parts,n,tracks,1);
    if(rc!=0){log_printf("ps2: Cobra 0x7028 failed raw_rc=%d",rc);rc=-50;goto fail;}
    mounted=1;
    if(journal("BEFORE_INSERT",path,0,1)!=0){rc=-46;goto fail;}
    rc=cobra_send_fake_disc_insert_event();
    if(rc!=0){rc=-51;goto fail;}
    if(journal("MOUNT_SUCCESS",path,0,0)!=0) {
        log_printf("ps2: mount succeeded, but journal final write failed");
    }
    return 0;
fail:
    if(mounted)(void)cobra_umount_disc_image();
    if(ejected)(void)cobra_send_fake_disc_insert_event();
    log_printf("ps2: mount failure rc=%d; reboot may be needed to restore previous disc",rc);
    (void)journal("MOUNT_FAILED",path,rc,0);
    return rc;
}
