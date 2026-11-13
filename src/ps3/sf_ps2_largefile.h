/* sf_ps2_largefile.h — 64-bit file I/O for PS2 Classics on PS3 HEN.
 * PSL1GHT stdio FILE offsets can be 32-bit on PPU. Calling ftello/fseeko
 * on an output beyond 2 GiB returned RC=-34 near 44% for a 4.53 GiB ISO.
 * Use LV2's real 64-bit file APIs for the source, output, and diagnostics.
 * Portable host path remains stdio with a 64-bit off_t for regression tests.
 * Zero buffering on PS3: sysLv2FsRead/Write return actual byte counts.
 */
#ifndef SF_PS2_LARGEFILE_H
#define SF_PS2_LARGEFILE_H

#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
#include <errno.h>
#include <limits.h>
#ifdef SF_PS2_HEN_HOST_TEST
#include <sys/types.h>
#else
#include <sys/file.h>  /* sysLv2FsLSeek64 + sysLv2FsRead/Write/Fsync */
#include <lv2/sysfs.h> /* SYS_O_* */
#endif

typedef struct {
#ifdef SF_PS2_HEN_HOST_TEST
    FILE *fp;
#else
    s32 fd;
#endif
    uint64_t position;
    int error;
    int active;
} SfPS2LargeFile;

static void sf_ps2_large_init(SfPS2LargeFile *f)
{
    f->position=0;f->error=0;f->active=0;
#ifdef SF_PS2_HEN_HOST_TEST
    f->fp=NULL;
#else
    f->fd=-1;
#endif
}
static int sf_ps2_large_open(SfPS2LargeFile *f,const char *path,int writable)
{
    sf_ps2_large_init(f);
#ifdef SF_PS2_HEN_HOST_TEST
    f->fp=fopen(path,writable?"wb+":"rb");
    if(!f->fp){f->error=errno?errno:-1;return -1;}
#else
    s32 fd=-1;
    s32 flags=writable?(SYS_O_RDWR|SYS_O_CREAT|SYS_O_TRUNC):SYS_O_RDONLY;
    s32 rc=sysLv2FsOpen(path,flags,&fd,0666,NULL,0);
    if(rc!=0){f->error=rc;return -1;}
    f->fd=fd;
#endif
    f->active=1;return 0;
}
static int sf_ps2_large_seek(SfPS2LargeFile *f,uint64_t at)
{
    if(!f->active || at>INT64_MAX){f->error=-1;return -1;}
#ifdef SF_PS2_HEN_HOST_TEST
    if(fseeko(f->fp,(off_t)at,SEEK_SET)!=0){f->error=errno?errno:-1;return -1;}
#else
    u64 newpos=0;
    s32 rc=sysLv2FsLSeek64(f->fd,(u64)at,SEEK_SET,&newpos);
    if(rc!=0 || newpos!=at){f->error=rc?rc:-1;return -1;}
#endif
    f->position=at;return 0;
}
static uint64_t sf_ps2_large_tell(const SfPS2LargeFile *f)
{
    return f->position;
}
static int sf_ps2_large_read(SfPS2LargeFile *f,void *buffer,size_t count)
{
    size_t done=0;
    if(!f->active){f->error=-1;return -1;}
    while(done<count){
        size_t step=0;
#ifdef SF_PS2_HEN_HOST_TEST
        step=fread((unsigned char *)buffer+done,1,count-done,f->fp);
        if(step==0){f->error=ferror(f->fp)?(errno?errno:-1):-2;return -1;}
#else
        u64 received=0;
        s32 rc=sysLv2FsRead(f->fd,(unsigned char *)buffer+done,
                            (u64)(count-done),&received);
        if(rc!=0 || received==0 || received>(u64)(count-done)){
            f->error=rc?rc:-2;return -1;
        }
        step=(size_t)received;
#endif
        done+=step;f->position+=step;
    }
    return 0;
}
static int sf_ps2_large_write(SfPS2LargeFile *f,const void *buffer,size_t count)
{
    size_t done=0;
    if(!f->active){f->error=-1;return -1;}
    while(done<count){
        size_t step=0;
#ifdef SF_PS2_HEN_HOST_TEST
        step=fwrite((const unsigned char *)buffer+done,1,count-done,f->fp);
        if(step==0){f->error=errno?errno:-1;return -1;}
#else
        u64 written=0;
        s32 rc=sysLv2FsWrite(f->fd,(const unsigned char *)buffer+done,
                             (u64)(count-done),&written);
        if(rc!=0 || written==0 || written>(u64)(count-done)){
            f->error=rc?rc:-2;return -1;
        }
        step=(size_t)written;
#endif
        done+=step;f->position+=step;
    }
    return 0;
}
static int sf_ps2_large_flush(SfPS2LargeFile *f)
{
#ifdef SF_PS2_HEN_HOST_TEST
    if(!f->active || fflush(f->fp)!=0){f->error=errno?errno:-1;return -1;}
#else
    if(!f->active){f->error=-1;return -1;}
    /* LV2 writes are unbuffered; only sync to disk once at finalization. */
#endif
    return 0;
}
static int sf_ps2_large_sync(SfPS2LargeFile *f)
{
    if(sf_ps2_large_flush(f)!=0)return -1;
#ifndef SF_PS2_HEN_HOST_TEST
    {
        s32 rc=sysLv2FsFsync(f->fd);
        if(rc!=0){f->error=rc;return -1;}
    }
#endif
    return 0;
}
static int sf_ps2_large_close(SfPS2LargeFile *f)
{
    int rc=0;
    if(!f->active)return 0;
#ifdef SF_PS2_HEN_HOST_TEST
    if(fclose(f->fp)!=0){f->error=errno?errno:-1;rc=-1;}
    f->fp=NULL;
#else
    {
        s32 status=sysLv2FsClose(f->fd);
        if(status!=0){f->error=status;rc=-1;}
        f->fd=-1;
    }
#endif
    f->active=0;
    return rc;
}
static int sf_ps2_large_read_at(SfPS2LargeFile *f,uint64_t at,void *buf,size_t len)
{
    return sf_ps2_large_seek(f,at)==0 && sf_ps2_large_read(f,buf,len)==0?0:-1;
}
#endif /* SF_PS2_LARGEFILE_H */
