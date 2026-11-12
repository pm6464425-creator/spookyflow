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
#include "log.h"

static const DISC_INTERFACE *const s_devices[8] = {
    &__io_ntfs_usb000,&__io_ntfs_usb001,&__io_ntfs_usb002,&__io_ntfs_usb003,
    &__io_ntfs_usb004,&__io_ntfs_usb005,&__io_ntfs_usb006,&__io_ntfs_usb007
};
static ntfs_md *s_mount[8];
static int s_count[8];
static volatile int s_initialized;

/* Deliberately mount only CLEAN NTFS filesystems; no recover/repair writes. */
int sf_bdntfs_start(void)
{
    int u,good=0;
    /* Recheck only absent ports, so Rescan Library can detect USB drives
       plugged in AFTER the app starts. Mounted partitions are not disturbed. */
    for (u=0;u<8;u++) {
        int n;
        if(s_count[u]>0){good++;continue;}
        if (!PS3_NTFS_IsInserted(u)) continue;
        n=ntfsMountDevice(s_devices[u],&s_mount[u],NTFS_DEFAULT);
        if (n>0 && s_mount[u]) {
            s_count[u]=n;
            good++;
            log_printf("bdiso: native NTFS USB%03d mounted (%d partitions)",u,n);
        } else {
            if(s_mount[u]) {free(s_mount[u]);s_mount[u]=NULL;}
            s_count[u]=0;
            log_printf("bdiso: USB%03d NTFS unavailable or dirty, disk left unchanged",u);
        }
    }
    s_initialized=1;
    return good;
}
int sf_bdntfs_has_port(int port) { return port>=0 && port<8 && s_count[port]>0; }

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
    len=strlen(s_mount[u][0].name);
    if(!len) return 0;
    if(strcmp(sub,"/")==0) {
        r=snprintf(out,cap,"%s%s",s_mount[u][0].name,
                   s_mount[u][0].name[len-1]==':'?"/":":/");
    } else {
        r=snprintf(out,cap,"%s%s%s",s_mount[u][0].name,
                   s_mount[u][0].name[len-1]==':'?"":":",sub);
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
 * slot is available. It never calls cobra_unload_vsh_plugin(). */
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
