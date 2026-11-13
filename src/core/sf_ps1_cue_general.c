/* General PS1 CUE parser and byte-accurate copy planner.
 * No dependence on game title or serial: supports any valid BINARY
 * MODE1/2352 or MODE2/2352 + AUDIO layout with 1-99 tracks.
 * PREGAP is inserted as silent raw CD sectors at the specified INDEX01,
 * INDEX00 sectors already in the source file are kept untouched.
 * POSTGAP is supported at the end of a FILE; mid-file POSTGAP refuses
 * instead of corrupting track timing.
 * This module is portable and regression-tested without a PS3.
 */
#include "sf_ps1_cue_general.h"
#include <stdio.h>
#include <string.h>
#include <strings.h>
#include <ctype.h>
#include <limits.h>

static const char *skip(const char *p) {
    while(*p==' ' || *p=='\t')p++;
    return p;
}
static int begins(const char *p,const char *word) {
    size_t n=strlen(word);
    return !strncasecmp(p,word,n) && (!p[n] || p[n]==' ' || p[n]=='\t');
}
static int parse_time(const char *s,uint32_t *result) {
    unsigned m,sec,frame;char tail;
    if(sscanf(skip(s),"%u:%u:%u %c",&m,&sec,&frame,&tail)!=3 ||
       m>999 || sec>=60 || frame>=75)return -1;
    *result=((m*60u)+sec)*75u+frame;
    return 0;
}
static int parse_file(const char *cue,const char *args,char *out) {
    char relative[512],*cur;
    const char *e,*slash,*extension;
    size_t len,root;
    args=skip(args);
    if(*args=='"') {
        args++;e=strchr(args,'"');
        if(!e)return -1;
    } else {
        e=args;
        while(*e && *e!=' ' && *e!='\t')e++;
    }
    len=(size_t)(e-args);
    if(!len || len>=sizeof(relative))return -1;
    memcpy(relative,args,len);relative[len]=0;
    if(*e=='"')e++;
    if(!begins(skip(e),"BINARY"))return -2;
    /* Relative CUE FILE references may contain subdirectories, but must
     * never escape the directory via traversal or absolute paths. */
    if(relative[0]=='/' || relative[0]=='\\' || strchr(relative,':'))
        return -3;
    for(cur=relative;*cur;cur++)if(*cur=='\\')*cur='/';
    if(strstr(relative,"//"))return -3;
    for(cur=relative;*cur;) {
        char *end=strchr(cur,'/');
        size_t n=end?(size_t)(end-cur):strlen(cur);
        if(n==0 || (n==1 && cur[0]=='.') ||
           (n==2 && cur[0]=='.' && cur[1]=='.'))return -3;
        if(!end)break;
        cur=end+1;
    }
    extension=strrchr(relative,'.');
    if(!extension || (strcasecmp(extension,".bin") &&
                      strcasecmp(extension,".img") &&
                      strcasecmp(extension,".iso")))return -4;
    if(!cue || cue[0]!='/' || !(slash=strrchr(cue,'/')))return -3;
    root=(size_t)(slash-cue)+1;
    if(root+len>=SF_PS1_IMAGE_MAX)return -3;
    memcpy(out,cue,root);
    memcpy(out+root,relative,len+1);
    return 0;
}
int sf_ps1_cue_general_parse(const char *cue_path,const char *text,
                             size_t len,SfPs1CuePlan *out)
{
    const char *p=text;size_t left=len;
    int file=-1,track=-1;
    if(!out || !text || !cue_path || !len || len>32768)return -1;
    memset(out,0,sizeof(*out));
    while(left) {
        char line[1024];size_t n=0;
        const char *q;
        while(n<left && p[n]!='\n' && p[n]!='\r')n++;
        if(n>=sizeof(line))return -2;
        memcpy(line,p,n);line[n]=0;
        p+=n;left-=n;
        while(left && (*p=='\n' || *p=='\r'))p++,left--;
        q=skip(line);
        /* UTF-8 BOM only at beginning of file */
        if(q==line && (unsigned char)q[0]==0xef &&
           (unsigned char)q[1]==0xbb && (unsigned char)q[2]==0xbf)
            q=skip(q+3);
        if(!*q || begins(q,"REM") || begins(q,"TITLE") ||
           begins(q,"PERFORMER") || begins(q,"SONGWRITER") ||
           begins(q,"ISRC") || begins(q,"FLAGS") ||
           begins(q,"CATALOG") || begins(q,"CDTEXTFILE"))continue;
        if(begins(q,"FILE")) {
            if(++file>=SF_PS1_CUE_MAX_FILES)return -3;
            if(parse_file(cue_path,q+4,out->file[file].path)!=0)return -4;
            /* Duplicate FILE references would duplicate physical sectors. */
            for(int i=0;i<file;i++)
                if(!strcasecmp(out->file[i].path,out->file[file].path))
                    return -5;
        } else if(begins(q,"TRACK")) {
            unsigned num=0;char mode[64],tail;
            if(file<0 || ++track>=SF_PS1_CUE_MAX_TRACKS ||
               sscanf(skip(q+5),"%u %63s %c",&num,mode,&tail)!=2 ||
               num!=(unsigned)track+1)return -6;
            out->track[track].file_index=(unsigned)file;
            if(!strcasecmp(mode,"AUDIO"))out->track[track].is_audio=1;
            else if(!strcasecmp(mode,"MODE1/2352") ||
                    !strcasecmp(mode,"MODE2/2352"))
                out->track[track].is_audio=0;
            else return -7;
        } else if(begins(q,"INDEX")) {
            unsigned num;char times[32],tail;
            uint32_t index;
            SfPs1CueTrack *t;
            if(track<0 || sscanf(skip(q+5),"%u %31s %c",&num,times,&tail)!=2 ||
               parse_time(times,&index)!=0)return -8;
            t=&out->track[track];
            if(num==0) {
                if(t->has_index00)return -8;
                t->index00=index;t->has_index00=1;
            } else if(num==1) {
                if(t->has_index01)return -8;
                t->index01=index;t->has_index01=1;
            } /* INDEX 02..99 do not change the first playable sector */
        } else if(begins(q,"PREGAP")) {
            uint32_t val;
            if(track<=0 || out->track[track].pregap ||
               parse_time(q+6,&val)!=0 || !val)return -9;
            out->track[track].pregap=val;
        } else if(begins(q,"POSTGAP")) {
            uint32_t val;
            if(track<0 || out->track[track].postgap ||
               parse_time(q+7,&val)!=0 || !val)return -10;
            out->track[track].postgap=val;
        } else return -11;
    }
    if(file<0 || track<0 || !out->track[0].has_index01 ||
       out->track[0].is_audio || out->track[0].index01!=0 ||
       out->track[0].pregap)return -12;
    out->file_count=(unsigned)file+1;
    out->track_count=(unsigned)track+1;
    /* Each FILE must contribute at least one TRACK, INDEX01s within the
     * same file strictly advance, INDEX00 must precede INDEX01. */
    for(unsigned i=0;i<out->track_count;i++) {
        const SfPs1CueTrack *t=&out->track[i];
        if(!t->has_index01 || (t->has_index00 && t->index00>t->index01) ||
           (t->has_index00 && t->pregap))return -13;
        if(i && t->file_index==out->track[i-1].file_index &&
           t->index01<=out->track[i-1].index01)return -14;
        if(t->postgap && i+1<out->track_count &&
           t->file_index==out->track[i+1].file_index)
            return -15; /* no uncertain mid-file gap rewriting */
    }
    for(unsigned i=0;i<out->file_count;i++){
        unsigned present=0;
        for(unsigned j=0;j<out->track_count;j++)
            if(out->track[j].file_index==i)present++;
        if(!present)return -16;
    }
    return 0;
}
static int add_chunk(SfPs1CueChunk *chunks,unsigned *n,
                     unsigned file,uint64_t at,uint64_t bytes,int silence)
{
    if(!bytes)return 0;
    if(*n>=SF_PS1_CUE_MAX_CHUNKS)return -1;
    chunks[*n].file_index=file;
    chunks[*n].at=at;
    chunks[*n].bytes=bytes;
    chunks[*n].is_silence=silence?1u:0u;
    (*n)++;
    return 0;
}
int sf_ps1_cue_general_layout(const SfPs1CuePlan *p,
                              const uint64_t *sizes,SfPs1Track *tracks,
                              SfPs1CueChunk *chunks,unsigned *nchunks,
                              uint64_t *total_bytes)
{
    uint64_t total=0;unsigned n=0;
    if(!p || !sizes || !tracks || !chunks || !nchunks || !total_bytes ||
       !p->file_count || !p->track_count ||
       p->file_count>SF_PS1_CUE_MAX_FILES ||
       p->track_count>SF_PS1_CUE_MAX_TRACKS)return -1;
    for(unsigned f=0;f<p->file_count;f++) {
        uint64_t cursor=0,size=sizes[f];
        uint64_t file_base=total,added=0;
        if(!size || size%2352u)return -2;
        for(unsigned i=0;i<p->track_count;i++) {
            const SfPs1CueTrack *t=&p->track[i];
            uint64_t target=(uint64_t)t->index01*2352u;
            if(t->file_index!=f)continue;
            if(target>=size || target<cursor)return -3;
            if(t->pregap) {
                uint64_t gap=(uint64_t)t->pregap*2352u;
                if(add_chunk(chunks,&n,f,cursor,target-cursor,0)!=0)return -4;
                cursor=target;
                if(add_chunk(chunks,&n,f,0,gap,1)!=0)return -4;
                added+=gap;
            }
            if((file_base+target+added)/2352u>UINT32_MAX)return -5;
            tracks[i].lba=(uint32_t)((file_base+target+added)/2352u);
            tracks[i].is_audio=t->is_audio;
            if(i+1==p->track_count ||
               p->track[i+1].file_index!=f) {
                /* POSTGAP is silent sectors not contained in the physical file */
                if(add_chunk(chunks,&n,f,cursor,size-cursor,0)!=0)return -4;
                cursor=size;
                if(t->postgap) {
                    uint64_t gap=(uint64_t)t->postgap*2352u;
                    if(add_chunk(chunks,&n,f,0,gap,1)!=0)return -4;
                    added+=gap;
                }
            }
        }
        if(cursor!=size)return -6;
        if(total>UINT64_MAX-size-added)return -7;
        total+=size+added;
    }
    if(total/2352u>UINT32_MAX)return -8;
    *nchunks=n;*total_bytes=total;
    return 0;
}
