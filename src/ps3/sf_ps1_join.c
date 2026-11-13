/* Experimental PS1 Redump CUE multi-BIN asynchronous native Cobra staging.
 * Cobra PSX opcode 0x7027 consumes ONE physical image and track descriptors.
 * This component assembles only PS1 2352-byte raw BINs, preserving data and
 * CDDA tracks in the original CUE order; source files never changed.
 * Using the proven 64-bit LV2 file I/O adapter prevents PS3 libc offset bugs.
 */
#include "sf_ps1_join.h"
#include "sf_ps1_multibin.h"
#include "sf_ps1_cue_general.h"
#include "sf_ps1_mount.h"
#include "sf_ps1_resolve.h"
#include "sf_ps2_largefile.h"
#include "ps3_platform.h"
#include "log.h"
#include <sys/thread.h>
#include <sys/stat.h>
#include <lv2/sysfs.h>
#include <stdint.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <strings.h>

#define PS1_STAGE_MAX (1400ULL*1024ULL*1024ULL)
#define PS1_STAGE_BUFFER (64u*1024u)
typedef struct {
    SfPs1MultiPlan plan;
    SfPs1Track tracks[SF_PS1_MULTIBIN_MAX];
    uint64_t sizes[SF_PS1_MULTIBIN_MAX];
    uint64_t expected;
    char output[SF_PS1_IMAGE_MAX];
    char part[SF_PS1_IMAGE_MAX];
    volatile int state; /* 0 idle, 2 copying, 1 ready, negative error */
    volatile int percent;
    volatile int cancel;
} SfPs1JoinJob;
static SfPs1JoinJob s_join;
static unsigned char s_copy_buffer[PS1_STAGE_BUFFER];
/* Do not place 32 KiB CUE buffer on the PS3 PPU main-thread stack. */
static char s_join_text[32769];
static char s_join_resolved[SF_PS1_IMAGE_MAX];
/* V15.27 general CUE: files and tracks are not the same thing.
 * Keep large structures in BSS: PS3 main thread stack is limited. */
static SfPs1CuePlan s_general_plan;
static SfPs1CueChunk s_general_chunks[SF_PS1_CUE_MAX_CHUNKS];
static uint64_t s_general_file_sizes[SF_PS1_CUE_MAX_FILES];
static unsigned s_general_chunk_count;
static int s_general_mode;
static int inspect_general_sources(const char *cue,const char *text,size_t n);


static uint64_t fnv(const void *data,size_t n,uint64_t h)
{
    const unsigned char *p=(const unsigned char*)data;
    while(n--){h^=*p++;h*=1099511628211ULL;}
    return h;
}
static int allowed(const char *p)
{
    if(!p)return 0;
    if(!strncmp(p,"/dev_hdd0/PSXISO/",17) ||
       !strncmp(p,"/dev_hdd0/PSXGAMES/",19))return 1;
    if(!strncmp(p,"/dev_usb00",10) && p[10]>='0'&&p[10]<='7' &&
       (strstr(p+11,"/PSXISO/") || strstr(p+11,"/PSXGAMES/")))return 1;
    return 0;
}
static int inspect_sources(const char *cue,const char *contents,size_t n)
{
    uint64_t hash=14695981039346656037ULL;
    int rc;
    s_general_mode=0;
    rc=sf_ps1_multibin_parse(cue,contents,n,&s_join.plan);
    if(rc!=0){
        log_printf("PS1_DIAG STAGE=MULTIBIN_LEGACY_PARSE RC=%d "
                   "ACTION=GENERAL_CUE_ENGINE",rc);
        /* Older Tekken layout still uses the field-tested joiner.
         * Mixed FILE/TRACK groupings and embedded PREGAP now use
         * the general file/track planner rather than webMAN. */
        return inspect_general_sources(cue,contents,n);
    }
    hash=fnv(contents,n,hash);
    hash=fnv("PS1_JOIN_V15_25",14,hash);
    for(unsigned i=0;i<s_join.plan.count;i++){
        struct stat st;
        const char *p=s_join.plan.part[i].path;
        if(!allowed(p)){
            log_printf("PS1_DIAG STAGE=MULTIBIN_SOURCE RESULT=UNSAFE_ROOT "
                       "TRACK=%u FILE=%s",i+1,p);
            return -71;
        }
        if(sf_ps1_resolve_existing(p,s_join_resolved,
                                   sizeof(s_join_resolved))!=0){
            log_printf("PS1_DIAG STAGE=MULTIBIN_SOURCE RESULT=NO_SAFE_BIN "
                       "TRACK=%u FILE=%s",i+1,p);
            return -71;
        }
        for(unsigned j=0;j<i;j++){
            if(!strcasecmp(s_join.plan.part[j].path,s_join_resolved)){
                log_printf("PS1_DIAG STAGE=MULTIBIN_SOURCE RESULT=DUPLICATE_RESOLVED_BIN "
                           "TRACK=%u PREVIOUS_TRACK=%u FILE=%s",
                           i+1,j+1,s_join_resolved);
                return -71;
            }
        }
        strcpy(s_join.plan.part[i].path,s_join_resolved);
        p=s_join.plan.part[i].path;
        if(stat(p,&st)!=0 || !S_ISREG(st.st_mode) || st.st_size<=0){
            log_printf("PS1_DIAG STAGE=MULTIBIN_SOURCE RESULT=NOT_READABLE "
                       "TRACK=%u ERRNO=%d FILE=%s",i+1,errno,p);
            return -71;
        }
        s_join.sizes[i]=(uint64_t)st.st_size;
        if(s_join.sizes[i]%2352u!=0){
            log_printf("PS1_DIAG STAGE=MULTIBIN_SOURCE RESULT=BAD_SECTOR_SIZE "
                       "TRACK=%u BYTES=%llu",i+1,
                       (unsigned long long)s_join.sizes[i]);
            return -72;
        }
        hash=fnv(p,strlen(p),hash);
        hash=fnv(&s_join.sizes[i],sizeof(s_join.sizes[i]),hash);
        /* Cache key changes when the underlying file is replaced. */
        {
            uint64_t mtime=(uint64_t)st.st_mtime;
            hash=fnv(&mtime,sizeof(mtime),hash);
        }
    }
    if(sf_ps1_multibin_layout(&s_join.plan,s_join.sizes,s_join.tracks,
                              &s_join.expected)!=0 ||
       s_join.expected<2352*100 || s_join.expected>PS1_STAGE_MAX){
        log_printf("PS1_DIAG STAGE=MULTIBIN_LAYOUT RESULT=INVALID_LAYOUT");
        return -72;
    }
    if(snprintf(s_join.output,sizeof(s_join.output),
                SF_DATA_DIR "/ps1_unido_%016llx.bin",
                (unsigned long long)hash)>=(int)sizeof(s_join.output))return -72;
    if(snprintf(s_join.part,sizeof(s_join.part),
                "%s.sfpart",s_join.output)>=(int)sizeof(s_join.part))return -72;
    log_printf("PS1_DIAG STAGE=MULTIBIN_PLAN RESULT=OK FILES=%u TRACKS=%u "
               "BYTES=%llu CACHE=%s",s_join.plan.count,s_join.plan.count,
               (unsigned long long)s_join.expected,s_join.output);
    for(unsigned i=0;i<s_join.plan.count;i++)
        log_printf("PS1_DIAG TRACK=%u TYPE=%s START_LBA=%u INDEX01=%u "
                   "PREGAP=%u INPUT_BYTES=%llu",
                   i+1,s_join.tracks[i].is_audio?"AUDIO":"DATA",
                   s_join.tracks[i].lba,s_join.plan.part[i].index01,
                   s_join.plan.part[i].pregap,
                   (unsigned long long)s_join.sizes[i]);
    return 0;
}
/* General PS1 CUE preflight. The CUE is authoritative for both data
 * and audio: names are resolved only if a unique filename exists.
 * One FILE may have several TRACKs; this is the case V15.25 refused. */
static int inspect_general_sources(const char *cue,const char *contents,size_t n)
{
    uint64_t hash=14695981039346656037ULL;
    int rc=sf_ps1_cue_general_parse(cue,contents,n,&s_general_plan);
    if(rc!=0){
        log_printf("PS1_DIAG STAGE=GENERAL_CUE_PARSE RESULT=UNSUPPORTED RC=%d",rc);
        return -70;
    }
    hash=fnv(contents,n,hash);
    hash=fnv("PS1_GENERAL_CUE_V1",17,hash);
    log_printf("PS1_DIAG STAGE=GENERAL_CUE_PARSE RESULT=OK "
               "FILES=%u TRACKS=%u",s_general_plan.file_count,
               s_general_plan.track_count);
    for(unsigned i=0;i<s_general_plan.file_count;i++){
        struct stat st;
        char *p=s_general_plan.file[i].path;
        if(!allowed(p)){
            log_printf("PS1_DIAG STAGE=GENERAL_FILE RESULT=UNSAFE_ROOT "
                       "FILE=%u PATH=%s",i+1,p);
            return -71;
        }
        rc=sf_ps1_resolve_existing(p,s_join_resolved,
                                   sizeof(s_join_resolved));
        if(rc!=0){
            log_printf("PS1_DIAG STAGE=GENERAL_FILE RESULT=%s "
                       "FILE=%u EXPECTED=%s RC=%d",
                       rc==-2?"AMBIGUOUS":"MISSING",i+1,p,rc);
            return -71;
        }
        for(unsigned j=0;j<i;j++)if(!strcasecmp(
           s_general_plan.file[j].path,s_join_resolved)){
            log_printf("PS1_DIAG STAGE=GENERAL_FILE RESULT=DUPLICATE "
                       "FILE=%u",i+1);
            return -71;
        }
        strcpy(p,s_join_resolved);
        if(stat(p,&st)!=0 || !S_ISREG(st.st_mode) || st.st_size<=0){
            log_printf("PS1_DIAG STAGE=GENERAL_FILE RESULT=STAT_FAILURE "
                       "FILE=%u ERRNO=%d",i+1,errno);
            return -71;
        }
        s_general_file_sizes[i]=(uint64_t)st.st_size;
        if(s_general_file_sizes[i]%2352u!=0){
            log_printf("PS1_DIAG STAGE=GENERAL_FILE RESULT=UNSUPPORTED_SECTOR "
                       "FILE=%u BYTES=%llu MOD2352=%llu",
                       i+1,(unsigned long long)s_general_file_sizes[i],
                       (unsigned long long)(s_general_file_sizes[i]%2352u));
            return -72;
        }
        hash=fnv(p,strlen(p),hash);
        hash=fnv(&s_general_file_sizes[i],
                 sizeof(s_general_file_sizes[i]),hash);
        {
            uint64_t mtime=(uint64_t)st.st_mtime;
            hash=fnv(&mtime,sizeof(mtime),hash);
        }
    }
    rc=sf_ps1_cue_general_layout(&s_general_plan,s_general_file_sizes,
                                  s_join.tracks,s_general_chunks,
                                  &s_general_chunk_count,
                                  &s_join.expected);
    if(rc!=0 || s_join.expected<2352*100 ||
       s_join.expected>PS1_STAGE_MAX){
        log_printf("PS1_DIAG STAGE=GENERAL_LAYOUT RESULT=REJECTED "
                   "RC=%d BYTES=%llu",rc,
                   (unsigned long long)s_join.expected);
        return -72;
    }
    /* The mount routine needs logical TRACK count, not FILE count. */
    s_join.plan.count=s_general_plan.track_count;
    s_general_mode=1;
    if(snprintf(s_join.output,sizeof(s_join.output),
                SF_DATA_DIR "/ps1_unido_%016llx.bin",
                (unsigned long long)hash)>=(int)sizeof(s_join.output))
        return -72;
    if(snprintf(s_join.part,sizeof(s_join.part),
                "%s.sfpart",s_join.output)>=(int)sizeof(s_join.part))
        return -72;
    log_printf("PS1_DIAG STAGE=GENERAL_LAYOUT RESULT=OK FILES=%u TRACKS=%u "
               "CHUNKS=%u SIZE=%llu CACHE=%s",
               s_general_plan.file_count,s_general_plan.track_count,
               s_general_chunk_count,
               (unsigned long long)s_join.expected,s_join.output);
    for(unsigned i=0;i<s_general_plan.track_count;i++){
        const SfPs1CueTrack *t=&s_general_plan.track[i];
        log_printf("PS1_DIAG TRACK=%u FILE=%u TYPE=%s LBA=%u "
                   "INDEX00=%s INDEX01=%u PREGAP=%u POSTGAP=%u",
                   i+1,t->file_index+1,
                   t->is_audio?"AUDIO":"DATA",
                   s_join.tracks[i].lba,
                   t->has_index00?"YES":"NO",
                   t->index01,t->pregap,t->postgap);
    }
    return 0;
}
static void ps1_join_worker(void *unused)
{
    SfPS2LargeFile out;
    uint64_t done=0;
    int rc=0,output_active=0;
    (void)unused;
    sf_ps2_large_init(&out);
    /* An interrupted older owned temp can be safely overwritten. */
    if(sf_ps2_large_open(&out,s_join.part,1)!=0){
        log_printf("PS1_DIAG STAGE=MULTIBIN_OPEN_OUTPUT RESULT=FAIL SYS_RC=%d",
                   out.error);
        rc=-74;goto finish;
    }
    output_active=1;
    if(s_general_mode){
        for(unsigned i=0;i<s_general_chunk_count;i++){
            const SfPs1CueChunk *chunk=&s_general_chunks[i];
            uint64_t left=chunk->bytes;
            SfPS2LargeFile in;
            int source_open=0;
            sf_ps2_large_init(&in);
            if(!chunk->is_silence){
                const char *path=s_general_plan.file[chunk->file_index].path;
                if(sf_ps2_large_open(&in,path,0)!=0){
                    rc=-75;
                    log_printf("PS1_DIAG STAGE=GENERAL_OPEN_SOURCE "
                               "CHUNK=%u SYS_RC=%d PATH=%s",
                               i+1,in.error,path);
                    goto close_out;
                }
                source_open=1;
                if(sf_ps2_large_seek(&in,chunk->at)!=0){
                    rc=-75;
                    log_printf("PS1_DIAG STAGE=GENERAL_SEEK_SOURCE "
                               "CHUNK=%u SYS_RC=%d",i+1,in.error);
                }
            }
            while(rc==0 && left){
                size_t amount=left>sizeof(s_copy_buffer)?
                              sizeof(s_copy_buffer):(size_t)left;
                if(s_join.cancel){rc=-79;break;}
                if(chunk->is_silence)memset(s_copy_buffer,0,amount);
                else if(sf_ps2_large_read(&in,s_copy_buffer,amount)!=0){
                    rc=-75;
                    log_printf("PS1_DIAG STAGE=GENERAL_READ "
                               "CHUNK=%u SYS_RC=%d OFFSET=%llu",
                               i+1,in.error,
                               (unsigned long long)sf_ps2_large_tell(&in));
                    break;
                }
                if(sf_ps2_large_write(&out,s_copy_buffer,amount)!=0){
                    rc=-76;
                    log_printf("PS1_DIAG STAGE=GENERAL_WRITE "
                               "CHUNK=%u SYS_RC=%d OFFSET=%llu",
                               i+1,out.error,
                               (unsigned long long)sf_ps2_large_tell(&out));
                    break;
                }
                left-=amount;
                done+=amount;
                s_join.percent=(int)(done*100u/s_join.expected);
            }
            if(source_open && sf_ps2_large_close(&in)!=0 && rc==0)
                rc=-75;
            if(rc!=0)goto close_out;
        }
    }else{
    for(unsigned i=0;i<s_join.plan.count;i++){
        SfPS2LargeFile in;
        uint64_t left=s_join.sizes[i];
        uint64_t gap=(uint64_t)s_join.plan.part[i].pregap*2352u;
        sf_ps2_large_init(&in);
        while(gap){
            size_t n=gap>sizeof(s_copy_buffer)?sizeof(s_copy_buffer):(size_t)gap;
            if(s_join.cancel){rc=-79;goto close_out;}
            memset(s_copy_buffer,0,n);
            if(sf_ps2_large_write(&out,s_copy_buffer,n)!=0){
                rc=-76;log_printf("PS1_DIAG STAGE=MULTIBIN_GAP_WRITE SYS_RC=%d TRACK=%u",
                                  out.error,i+1);
                goto close_out;
            }
            gap-=n;done+=n;
        }
        if(sf_ps2_large_open(&in,s_join.plan.part[i].path,0)!=0){
            rc=-75;
            log_printf("PS1_DIAG STAGE=MULTIBIN_OPEN_SOURCE SYS_RC=%d TRACK=%u",
                       in.error,i+1);
            goto close_out;
        }
        while(left){
            size_t n=left>sizeof(s_copy_buffer)?sizeof(s_copy_buffer):(size_t)left;
            if(s_join.cancel){rc=-79;break;}
            if(sf_ps2_large_read(&in,s_copy_buffer,n)!=0){
                rc=-75;
                log_printf("PS1_DIAG STAGE=MULTIBIN_READ SYS_RC=%d TRACK=%u "
                           "OFFSET=%llu",in.error,i+1,
                           (unsigned long long)sf_ps2_large_tell(&in));
                break;
            }
            if(sf_ps2_large_write(&out,s_copy_buffer,n)!=0){
                rc=-76;
                log_printf("PS1_DIAG STAGE=MULTIBIN_WRITE SYS_RC=%d TRACK=%u "
                           "OFFSET=%llu",out.error,i+1,
                           (unsigned long long)sf_ps2_large_tell(&out));
                break;
            }
            left-=n;done+=n;
            s_join.percent=(int)(done*100u/s_join.expected);
        }
        {
            int close_rc=sf_ps2_large_close(&in);
            if(close_rc!=0 && !rc)rc=-75;
        }
        if(rc!=0)goto close_out;
    }
    } /* end original one-file-per-track copier */
    if(done!=s_join.expected){
        log_printf("PS1_DIAG STAGE=MULTIBIN_SIZE RESULT=MISMATCH "
                   "COPIED=%llu EXPECTED=%llu",
                   (unsigned long long)done,
                   (unsigned long long)s_join.expected);
        rc=-77;goto close_out;
    }
    if(sf_ps2_large_sync(&out)!=0){
        log_printf("PS1_DIAG STAGE=MULTIBIN_FSYNC SYS_RC=%d",out.error);
        rc=-77;
    }
close_out:
    if(output_active && sf_ps2_large_close(&out)!=0 && rc==0)rc=-77;
    if(rc==0){
        struct stat st;
        if(stat(s_join.part,&st)!=0 ||
           (uint64_t)st.st_size!=s_join.expected){
            rc=-77;
            log_printf("PS1_DIAG STAGE=MULTIBIN_FINAL_SIZE RESULT=MISMATCH");
        }
    }
    if(rc==0 && rename(s_join.part,s_join.output)!=0){
        rc=-77;
        log_printf("PS1_DIAG STAGE=MULTIBIN_RENAME RESULT=FAIL ERRNO=%d",errno);
    }
    if(rc!=0)remove(s_join.part); /* exclusively owned temp path */
finish:
    s_join.percent=rc? s_join.percent:100;
    log_printf("PS1_DIAG STAGE=MULTIBIN_COPY RESULT=%s RC=%d "
               "COPIED=%llu EXPECTED=%llu OUTPUT=%s",
               rc?"FAIL":"READY",rc,
               (unsigned long long)done,
               (unsigned long long)s_join.expected,
               s_join.output);
    __sync_synchronize();
    s_join.state=rc?rc:1;
    sysThreadExit(0);
}
int sf_ps1_join_start(const char *cue)
{
    char *text=s_join_text;FILE *f;size_t n;
    uint32_t block=0;u64 available=0;
    sys_ppu_thread_t tid;
    int rc;
    struct stat cached;
    if(s_join.state==2)return -78;
    if(!allowed(cue) || !cue || strlen(cue)>=SF_PS1_IMAGE_MAX ||
       strcasecmp(cue+strlen(cue)-4,".cue"))return -70;
    f=fopen(cue,"rb");
    if(!f){log_printf("PS1_DIAG STAGE=MULTIBIN_CUE_OPEN ERRNO=%d",errno);return -70;}
    n=fread(text,1,sizeof(s_join_text)-1,f);
    if(ferror(f) || (n==sizeof(s_join_text)-1 && !feof(f))){fclose(f);return -70;}
    fclose(f);text[n]=0;
    {
        int source_rc=inspect_sources(cue,text,n);
        if(source_rc!=0)return source_rc;
    }
    if(stat(s_join.output,&cached)==0 && S_ISREG(cached.st_mode) &&
       (uint64_t)cached.st_size==s_join.expected){
        log_printf("PS1_DIAG STAGE=MULTIBIN_CACHE RESULT=VALID BYTES=%llu",
                   (unsigned long long)s_join.expected);
        s_join.state=1;s_join.percent=100;
        return 1;
    }
    rc=sysFsGetFreeSize("/dev_hdd0",&block,&available);
    if(rc!=0 || block==0 || available< (s_join.expected+64ULL*1024*1024+block-1)/block){
        log_printf("PS1_DIAG STAGE=MULTIBIN_FREE_SPACE RESULT=INSUFFICIENT "
                   "SYS_RC=%d BLOCK=%u FREE_BLOCKS=%llu REQUIRED_BYTES=%llu",
                   rc,block,(unsigned long long)available,
                   (unsigned long long)(s_join.expected+64ULL*1024*1024));
        return -73;
    }
    log_printf("PS1_DIAG STAGE=MULTIBIN_FREE_SPACE RESULT=OK "
               "BYTES=%llu", (unsigned long long)(available*(uint64_t)block));
    s_join.cancel=0;s_join.percent=0;s_join.state=2;
    __sync_synchronize();
    rc=sysThreadCreate(&tid,ps1_join_worker,NULL,1600,128u*1024u,0,
                       (char*)"sf_ps1_join");
    if(rc!=0){
        s_join.state=-78;
        log_printf("PS1_DIAG STAGE=MULTIBIN_THREAD RESULT=FAIL SYS_RC=%d",rc);
        return -78;
    }
    log_printf("PS1_DIAG STAGE=MULTIBIN_THREAD RESULT=STARTED");
    return 0;
}
int sf_ps1_join_poll(int *pct)
{
    __sync_synchronize();
    if(pct)*pct=s_join.percent;
    return s_join.state==2?0:s_join.state;
}
int sf_ps1_join_mount(void)
{
    if(s_join.state!=1){
        log_printf("PS1_DIAG STAGE=MULTIBIN_MOUNT RESULT=NOT_READY STATE=%d",
                   s_join.state);
        return -80;
    }
    log_printf("PS1_DIAG STAGE=MULTIBIN_MOUNT IMAGE=%s TRACKS=%u",
               s_join.output,s_join.plan.count);
    return sf_ps1_mount_staged(s_join.output,s_join.tracks,s_join.plan.count);
}
void sf_ps1_join_cancel(void){s_join.cancel=1;}
