/* Native NTFS file operations for movie artwork.
 * Adapted from the libntfs discovery pattern used in FLOW MOVIE 3D.
 * IMPORTANT: the actual BDISO mount is delegated to Cobra/webMAN; this
 * helper NEVER changes the Cobra plugin slots or the game mount logic. */
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <malloc.h>
#include <unistd.h>
#include <cobra.h>
#include "util.h"
#include <ntfs.h>
#include "sf_bdiso_ntfs.h"
#include "bdiso_usb.h"
#include "log.h"

static const DISC_INTERFACE *const s_devices[8] = {
    &__io_ntfs_usb000,&__io_ntfs_usb001,&__io_ntfs_usb002,&__io_ntfs_usb003,
    &__io_ntfs_usb004,&__io_ntfs_usb005,&__io_ntfs_usb006,&__io_ntfs_usb007
};
/* Physical NTFS state. A missing drive must invalidate the in-memory mount
 * even when the UI reconnects before its async library scan gets to cleanup.
 * The lightweight foreground poll only RECORDS this edge; native volume
 * cleanup and remounting occur on the normal library loading path. */
static ntfs_md *s_mount[8];
static int s_count[8];
static int s_last_mount_rc[8]; /* 0 = no mount attempted yet */
static volatile uint32_t s_detach_seen;

/* Physical presence only; mounting/releasing remains on the scan thread. */
uint32_t sf_bdntfs_usb_present_mask(void)
{
    uint32_t mask=0;
    int port;
    for(port=0;port<8;port++){
        if(PS3_NTFS_IsInserted(port))
            mask |= (uint32_t)1u<<port;
        else if(s_count[port]>0)
            __sync_fetch_and_or(&s_detach_seen,(uint32_t)1u<<port);
    }
    return mask;
}

/* V15.2 rejected every webMAN .ntfs[BDISO] whose device header differed
 * from the expected 64-bit value. In the field, that hid valid movies.
 * V15.3 checks physical presence first and uses a tolerant pointer parser.
 * Recognized IDs must match a connected port; unknown legacy IDs are
 * kept when any NTFS drive is inserted, preserving pre-V15.2 behavior. */
int sf_bdntfs_index_online(const unsigned char *data, size_t len)
{
    uint32_t present=sf_bdntfs_usb_present_mask();
    int port=bdiso_usb_pointer_port(data,len);
    int visible=bdiso_usb_pointer_visible(data,len,present);
    if(visible && port<0)
        log_printf("bdiso: legacy webMAN pointer accepted (physical USB present mask=0x%02x)",
                   (unsigned)present);
    return visible;
}

/* FLOW MOVIE scans every partition of each attached USB device.  Keep the
 * partition containing /BDISO, not an arbitrary first partition. */
static int s_partition[8] = {-1,-1,-1,-1,-1,-1,-1,-1};

/* Do not disturb mounted game volumes; these handles belong only to BDISO. */
static void bd_release_port(int u,int physical_removed)
{
    int p;
    if(u<0 || u>=8) return;
    if(s_mount[u]) {
        /* A physically removed disk must not be accessed during cleanup. */
        if(!physical_removed)
            for(p=0;p<s_count[u];p++)
                if(s_mount[u][p].name[0])
                    ntfsUnmount(s_mount[u][p].name,true);
        free(s_mount[u]);
        s_mount[u]=NULL;
    }
    s_count[u]=0;
    s_partition[u]=-1;
    s_last_mount_rc[u]=0;
    PS3_NTFS_Shutdown(u);
}

static int bd_find_partition(int u)
{
    int p;
    if(u<0 || u>=8 || !s_mount[u] || s_count[u]<=0) return -1;
    for(p=0;p<s_count[u];p++) {
        char dir[512];
        size_t n=strlen(s_mount[u][p].name);
        DIR_ITER *it;
        int len;
        if(n==0)continue;
        len=snprintf(dir,sizeof(dir),"%s%sBDISO",
                     s_mount[u][p].name,
                     s_mount[u][p].name[n-1]==':'?"/":":/");
        if(len<=0 || (size_t)len>=sizeof(dir))continue;
        it=ps3ntfs_diropen(dir);
        if(it){ps3ntfs_dirclose(it);return p;}
    }
    return -1;
}

/* Preserve FLOW MOVIE's NTFS mount flags, and scan all partitions.  Device
 * errors are diagnosed rather than silently returning an empty BDISO list. */
int sf_bdntfs_start(void)
{
    int u,good=0;
    for(u=0;u<8;u++) {
        int n,action,part=-1;
        uint32_t bit=(uint32_t)1u<<u;
        int inserted=PS3_NTFS_IsInserted(u);
        /* Mark a detached port without ever touching that missing disk. */
        if(!inserted && s_count[u]>0)
            __sync_fetch_and_or(&s_detach_seen,bit);
        /* Consume remembered absence, even if the disk has just come back. */
        int lost=(__sync_fetch_and_and(&s_detach_seen,~bit)&bit)!=0;
        int previous=s_count[u]>0 && s_mount[u]!=NULL;
        if(inserted && previous && !lost)
            part=bd_find_partition(u);  /* stale NTFS handles can be unreadable */
        /* A mounted NTFS partition that never had /BDISO is normal:
         * do not remount it on every USB-root probe just because the
         * optional movie directory is missing. Only recover a previously
         * readable BDISO volume that has become inaccessible. */
        action=bdiso_usb_mount_action(inserted,previous,lost,
                                     part>=0 || (previous && s_partition[u]<0));
        if(action==BDUSB_DISCONNECTED){
            if(previous){
                log_printf("bdiso: USB%03d detached, clearing stale native handles",u);
                bd_release_port(u,1); /* no filesystem operations after unplug */
            }
            continue;
        }
        if(action==BDUSB_KEEP_MOUNT){
            s_partition[u]=part;
            if(part>=0)good++;
            continue;
        }
        if(action==BDUSB_RESET_REMOUNT){
            log_printf("bdiso: USB%03d reconnected or stale native BDISO (detached_seen=%d partition=%d); remounting",
                       u,lost,part);
            /* An old mount may reference a device that is no longer there.
             * Do NOT attempt ntfsUnmount() on a possibly removed drive. */
            bd_release_port(u,1);
        }
        s_partition[u]=-1;
        log_printf("bdiso: USB%03d native mount attempt (reason=%s)",u,
                   action==BDUSB_RESET_REMOUNT?"RECOVERY":"NEW_USB");
        n=ntfsMountDevice(s_devices[u],&s_mount[u],NTFS_DEFAULT | NTFS_RECOVER);
        s_last_mount_rc[u]=n;
        if(n>0 && s_mount[u]) {
            s_count[u]=n;
            s_partition[u]=bd_find_partition(u);
            if(s_partition[u]>=0) {
                good++;
                log_printf("bdiso: USB%03d NTFS mounted, %d partitions; BDISO on partition %d",
                           u,n,s_partition[u]);
            } else {
                log_printf("bdiso: USB%03d NTFS mounted, %d partitions; no /BDISO directory",
                           u,n);
            }
        } else {
            if(s_mount[u]){free(s_mount[u]);s_mount[u]=NULL;}
            s_count[u]=0;
            log_printf("bdiso: USB%03d NTFS mount failed rc=%d after %s (check USB state and native mount registry)",u,n,action==BDUSB_RESET_REMOUNT?"RECONNECT":"FIRST_MOUNT");
        }
    }
    return good;
}
void sf_bdntfs_probe_port(int port,int *nparts,int *bd_partition,int *last_mount_rc)
{
    if(nparts)*nparts=(port>=0&&port<8)?s_count[port]:0;
    if(bd_partition)*bd_partition=(port>=0&&port<8)?s_partition[port]:-1;
    if(last_mount_rc)*last_mount_rc=(port>=0&&port<8)?s_last_mount_rc[port]:0;
}

int sf_bdntfs_has_port(int port)
{
    return port>=0 && port<8 && s_count[port]>0 &&
           s_mount[port] && s_partition[port]>=0;
}

/* The public scanner path remains /dev_usb00N/BDISO/, matching FAT32.
 * Map it to the private libntfs prefix only after strict validation. */
int sf_bdntfs_path(const char *path, char *out, size_t cap)
{
    int u,r;
    const char *sub;
    size_t len;
    if(!path || !out || cap<24 || strncmp(path,"/dev_usb00",10)!=0) return 0;
    if(path[10]<'0' || path[10]>'7') return 0;
    u=path[10]-'0';
    if(!sf_bdntfs_has_port(u)) return 0;
    sub=path+11;
    if (*sub=='\0') sub="/";
    if (*sub!='/' || (*sub!='\0' && strstr(sub,"/../")) ||
        strstr(sub,"/./") || strstr(sub,"//") || strchr(sub,'\\')) return 0;
    if(strncmp(sub,"/BDISO",6)!=0 && strcmp(sub,"/")!=0) return 0;
    if (sub[0]=='/' && sub[1]=='B' && sub[6] && sub[6]!='/') return 0;
    len=strlen(s_mount[u][s_partition[u]].name);
    if(!len) return 0;
    if(strcmp(sub,"/")==0) {
        r=snprintf(out,cap,"%s%s",s_mount[u][s_partition[u]].name,
                   s_mount[u][s_partition[u]].name[len-1]==':'?"/":":/");
    } else {
        r=snprintf(out,cap,"%s%s%s",s_mount[u][s_partition[u]].name,
                   s_mount[u][s_partition[u]].name[len-1]==':'?"":":",sub);
    }
    return r>0 && (size_t)r<cap;
}
int sf_bdntfs_list(const char *dir, FsEntry *out, int max)
{
    DIR_ITER *it;
    struct stat st;
    char path[512],name[FS_NAME_MAX];
    int n=0;
    if(!dir || !out || max<=0) return -1;
    /* Root probes are scan-thread-only, and can discover hot-plugged NTFS. */
    if(strlen(dir)==11 && strncmp(dir,"/dev_usb00",10)==0)
        (void)sf_bdntfs_start();
    /* USB root probe: advertise the BDISO folder for the library scanner. */
    if(strlen(dir)==11 && strncmp(dir,"/dev_usb00",10)==0 &&
       dir[10]>='0' && dir[10]<='7' &&
       sf_bdntfs_has_port(dir[10]-'0')) {
        memset(&out[0],0,sizeof(out[0]));
        strcpy(out[0].name,"BDISO");
        out[0].is_dir=1;
        return 1;
    }
    if(!sf_bdntfs_path(dir,path,sizeof(path))) return -1;
    it=ps3ntfs_diropen(path);
    if(!it) return -1;
    while(n<max) {
        memset(&st,0,sizeof(st));
        if(ps3ntfs_dirnext(it,name,&st)!=0) break;
        if(!name[0] || strcmp(name,".")==0 || strcmp(name,"..")==0 ||
           strlen(name)>=FS_NAME_MAX) continue;
        memset(&out[n],0,sizeof(out[n]));
        strcpy(out[n].name,name);
        out[n].is_dir=S_ISDIR(st.st_mode)!=0;
        out[n].size=out[n].is_dir?0:(uint64_t)st.st_size;
        n++;
    }
    ps3ntfs_dirclose(it);
    return n;
}
static int bd_stat(const char *path, struct stat *st)
{
    char native[512];
    if(!sf_bdntfs_path(path,native,sizeof(native))) return -1;
    return ps3ntfs_stat(native,st);
}
int sf_bdntfs_is_file(const char *path)
{ struct stat st;return bd_stat(path,&st)==0 && S_ISREG(st.st_mode); }
int sf_bdntfs_is_dir(const char *path)
{ struct stat st;return bd_stat(path,&st)==0 && S_ISDIR(st.st_mode); }
int sf_bdntfs_read(const char *path, uint64_t off, void *buf, uint32_t len)
{
    char native[512];
    int fd,r,total=0;
    if(!buf || !sf_bdntfs_path(path,native,sizeof(native))) return -1;
    fd=ps3ntfs_open(native,O_RDONLY,0);
    if(fd<0) return -1;
    if(off && ps3ntfs_seek64(fd,(s64)off,SEEK_SET)<0) { ps3ntfs_close(fd); return -1; }
    while((uint32_t)total<len) {
        r=ps3ntfs_read(fd,(char*)buf+total,len-(uint32_t)total);
        if(r<=0) break;
        total+=r;
    }
    ps3ntfs_close(fd);
    return total;
}
int sf_bdntfs_load(const char *path, unsigned char **bytes, uint32_t *len, uint32_t max_bytes)
{
    struct stat st;
    unsigned char *data;
    int n;
    if(!bytes || !len || bd_stat(path,&st)!=0 || st.st_size<=0 ||
       (uint64_t)st.st_size>max_bytes) return -1;
    data=(unsigned char*)malloc((size_t)st.st_size);
    if(!data) return -1;
    n=sf_bdntfs_read(path,0,data,(uint32_t)st.st_size);
    if(n!=(int)st.st_size){free(data);return -1;}
    *bytes=data; *len=(uint32_t)st.st_size; return 0;
}
int sf_bdntfs_write(const char *path, const void *a, uint32_t alen, const void *b, uint32_t blen)
{
    char native[512];
    int fd,r,ok=1;
    if(!strstr(path,"/BDISO/COVERS/") ||
       !sf_bdntfs_path(path,native,sizeof(native))) return -1;
    fd=ps3ntfs_open(native,O_WRONLY|O_CREAT|O_TRUNC,0666);
    if(fd<0) return -1;
    if(alen) {r=ps3ntfs_write(fd,(const char*)a,alen);if(r!=(int)alen)ok=0;}
    if(ok && blen) {r=ps3ntfs_write(fd,(const char*)b,blen);if(r!=(int)blen)ok=0;}
    if(ok && ps3ntfs_fsync(fd)!=0)ok=0;
    ps3ntfs_close(fd);
    if(!ok) {ps3ntfs_unlink(native);return -1;}
    return 0;
}
int sf_bdntfs_unlink(const char *path)
{
    char native[512];
    if(!strstr(path,"/BDISO/COVERS/") ||
       !sf_bdntfs_path(path,native,sizeof(native))) return -1;
    return ps3ntfs_unlink(native);
}
int sf_bdntfs_mkdir(const char *path)
{
    char native[512];
    if(!strstr(path,"/BDISO/COVERS") ||
       !sf_bdntfs_path(path,native,sizeof(native))) return -1;
    return ps3ntfs_mkdir(native,0777);
}


/* The exact sector-list format and EMU_BD value are inherited from
 * FLOW MOVIE 3D's validated raw_iso.sprx implementation. This works
 * independently of webMAN's .ntfs[BDISO] index once a free VSH plugin
 * slot is available. It never calls the VSH plugin unload syscall. */
typedef struct {
    uint64_t device;
    uint32_t emu_mode;
    uint32_t num_sections;
    uint32_t num_tracks;
} __attribute__((packed)) SfRawIsoArgs;

#define SF_BD_RAWISO_PATH "/dev_hdd0/game/SPKF00001/USRDIR/raw_iso.sprx"
#define SF_BD_RAWISO_BUF 0x10000u
#define SF_BD_RAWISO_MAX ((int)((SF_BD_RAWISO_BUF - sizeof(SfRawIsoArgs)) / 8u))

int sf_bdntfs_mount_raw_iso(const char *path)
{
    char native[512];
    struct stat st;
    void *plugin_args=NULL;
    uint32_t *sectors=NULL, *counts=NULL;
    SfRawIsoArgs *args;
    int port,nparts,slot,rc=-21,success=0;
    const unsigned int candidate_slots[]={6,5,4,3,2,1};
    unsigned int k;

    if(!path || strncmp(path,"/dev_usb00",10)!=0 ||
       path[10]<'0'||path[10]>'7' || path[11]!='/' ||
       !strstr(path,"/BDISO/") || !sf_ends_with_i(path,".iso")) return -1;
    port=path[10]-'0';
    if(!sf_bdntfs_has_port(port) ||
       !sf_bdntfs_path(path,native,sizeof(native))) return -2;
    if(ps3ntfs_stat(native,&st)!=0 || st.st_size<2048) return -3;
    if(stat(SF_BD_RAWISO_PATH,&st)!=0) {
        log_printf("bdiso: missing embedded raw_iso.sprx");
        return -4;
    }
    plugin_args=memalign(128,SF_BD_RAWISO_BUF);
    sectors=(uint32_t*)malloc(SF_BD_RAWISO_MAX*sizeof(uint32_t));
    counts=(uint32_t*)malloc(SF_BD_RAWISO_MAX*sizeof(uint32_t));
    if(!plugin_args || !sectors || !counts) {rc=-5;goto cleanup;}
    memset(plugin_args,0,SF_BD_RAWISO_BUF);
    nparts=ps3ntfs_file_to_sectors(native,sectors,counts,SF_BD_RAWISO_MAX,1);
    if(nparts<=0 || nparts>=SF_BD_RAWISO_MAX) {
        log_printf("bdiso: NTFS sector map invalid/fragmented (%d)",nparts);
        rc=-6;goto cleanup;
    }
    args=(SfRawIsoArgs*)plugin_args;
    args->device=USB_MASS_STORAGE(port);
    args->emu_mode=EMU_BD;
    args->num_sections=(uint32_t)nparts;
    args->num_tracks=0;
    memcpy((uint8_t*)plugin_args+sizeof(SfRawIsoArgs),
           sectors,(size_t)nparts*sizeof(uint32_t));
    memcpy((uint8_t*)plugin_args+sizeof(SfRawIsoArgs)+
           (size_t)nparts*sizeof(uint32_t),
           counts,(size_t)nparts*sizeof(uint32_t));
    /* IMPORTANT: Never unload slot 0 (webMAN) or any occupied plugin.
       A failed load leaves the occupied slot untouched. */
    for(k=0;k<sizeof(candidate_slots)/sizeof(candidate_slots[0]);k++){
        slot=(int)candidate_slots[k];
        rc=cobra_load_vsh_plugin((unsigned int)slot,(char*)SF_BD_RAWISO_PATH,
                                  plugin_args,SF_BD_RAWISO_BUF);
        if(rc==0){
            log_printf("bdiso: raw ISO mounted USB%03d via free Cobra plugin slot %d (%d extents)",
                       port,slot,nparts);
            success=1;break;
        }
    }
    if(!success) log_printf("bdiso: raw ISO plugin could not load, rc=%d",rc);
cleanup:
    free(plugin_args);free(sectors);free(counts);
    return success?0:(rc?rc:-21);
}
