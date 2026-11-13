#include "sf_ps1_multibin.h"
#include <string.h>
#include <strings.h>
#include <stdio.h>
#include <limits.h>
#include <ctype.h>

#define PS1_SECTOR 2352u
static const char *ws(const char *s){while(*s==' '||*s=='\t')s++;return s;}
static int key(const char *p,const char *w)
{
    size_t n=strlen(w);
    return !strncasecmp(p,w,n) && (p[n]==' ' || p[n]=='\t' || p[n]==0);
}
static int cue_file(const char *cue,const char *p,char *out)
{
    char name[512];size_t n,dir;const char *end,*slash;
    p=ws(p);
    if(*p=='"'){
        p++;end=strchr(p,'"');
        if(!end)return -1;
    }else{
        end=p;while(*end && *end!=' ' && *end!='\t')end++;
    }
    n=(size_t)(end-p);
    if(n==0 || n>=sizeof(name))return -1;
    memcpy(name,p,n);name[n]=0;
    if(*end=='"')end++;
    if(!key(ws(end),"BINARY"))return -1;
    if(name[0]=='.' || strstr(name,"..") || strchr(name,'/') ||
       strchr(name,'\\') || strchr(name,':'))return -1;
    {
        const char *e=strrchr(name,'.');
        if(!e || (strcasecmp(e,".bin") && strcasecmp(e,".img")))return -1;
    }
    if(cue[0]!='/' || !(slash=strrchr(cue,'/')))return -1;
    dir=(size_t)(slash-cue+1);
    if(dir+n>=SF_PS1_IMAGE_MAX)return -1;
    memcpy(out,cue,dir);memcpy(out+dir,name,n+1);
    return 0;
}
static int cue_lba(const char *txt,uint32_t *out)
{
    unsigned m,s,f;char tail;
    if(sscanf(ws(txt),"%u:%u:%u %c",&m,&s,&f,&tail)!=3 ||
       m>950 || s>=60 || f>=75)return -1;
    *out=(m*60u+s)*75u+f;
    return 0;
}
int sf_ps1_multibin_parse(const char *cue,const char *text,size_t size,
                          SfPs1MultiPlan *out)
{
    const char *p=text;size_t left=size;int t=-1;
    int have_track[SF_PS1_MULTIBIN_MAX]={0};
    int have_index01[SF_PS1_MULTIBIN_MAX]={0};
    if(!cue || !text || !out || !size || size>32768)return -1;
    memset(out,0,sizeof(*out));
    while(left){
        char line[640];size_t n=0;const char *cur;
        while(n<left && p[n]!='\n' && p[n]!='\r')n++;
        if(n>=sizeof(line))return -2;
        memcpy(line,p,n);line[n]=0;p+=n;left-=n;
        while(left && (*p=='\n'||*p=='\r'))p++,left--;
        cur=ws(line);
        if(!*cur || key(cur,"REM") || key(cur,"TITLE") ||
           key(cur,"PERFORMER") || key(cur,"SONGWRITER") ||
           key(cur,"ISRC") || key(cur,"FLAGS") ||
           key(cur,"CATALOG") || key(cur,"CDTEXTFILE"))continue;
        if(key(cur,"FILE")){
            if(t+1>=SF_PS1_MULTIBIN_MAX)return -3;
            t++;
            if(cue_file(cue,cur+4,out->part[t].path)!=0)return -4;
        }else if(key(cur,"TRACK")){
            unsigned number=0;char mode[40],tail;
            if(t<0 || have_track[t])return -5;
            if(sscanf(ws(cur+5),"%u %39s %c",&number,mode,&tail)!=2 ||
               number!=(unsigned)t+1)return -6;
            have_track[t]=1;
            if(!strcasecmp(mode,"AUDIO"))out->part[t].is_audio=1;
            else if((!strcasecmp(mode,"MODE1/2352") ||
                     !strcasecmp(mode,"MODE2/2352")) && t==0)
                out->part[t].is_audio=0;
            else return -7; /* one data track, subsequent audio */
        }else if(key(cur,"INDEX")){
            unsigned index=0;char ts[40],tail;uint32_t lba;
            if(t<0 || !have_track[t] ||
               sscanf(ws(cur+5),"%u %39s %c",&index,ts,&tail)!=2 ||
               cue_lba(ts,&lba)!=0)return -8;
            if(index==0){
                if(out->part[t].has_index00)return -9;
                out->part[t].has_index00=1;
            }else if(index==1){
                if(have_index01[t])return -10;
                have_index01[t]=1;out->part[t].index01=lba;
            }else return -11;
        }else if(key(cur,"PREGAP")){
            uint32_t gap;
            if(t<=0 || !have_track[t] ||
               cue_lba(cur+6,&gap)!=0 || !gap || out->part[t].pregap)
                return -12;
            out->part[t].pregap=gap;
        }else if(key(cur,"POSTGAP"))return -13;
        else return -14;
    }
    if(t<1)return -15;
    out->count=(unsigned)t+1;
    if(out->part[0].is_audio || out->part[0].index01!=0 ||
       out->part[0].pregap)return -16;
    for(unsigned i=0;i<out->count;i++){
        if(!have_track[i] || !have_index01[i] ||
           (out->part[i].pregap && out->part[i].has_index00))return -17;
        /* Multiple references to the same file are not safe to concatenate. */
        for(unsigned j=0;j<i;j++)
            if(!strcasecmp(out->part[j].path,out->part[i].path))
                return -18;
    }
    return 0;
}
int sf_ps1_multibin_layout(const SfPs1MultiPlan *p,const uint64_t *sizes,
                           SfPs1Track *tracks,uint64_t *total)
{
    uint64_t off=0;
    if(!p || !sizes || !tracks || !total ||
       p->count<2 || p->count>SF_PS1_MULTIBIN_MAX)return -1;
    for(unsigned i=0;i<p->count;i++){
        uint64_t sectors;
        if(!sizes[i] || sizes[i]%PS1_SECTOR)return -2;
        sectors=sizes[i]/PS1_SECTOR;
        if(p->part[i].index01>=sectors)return -3;
        if(off/PS1_SECTOR + p->part[i].pregap + p->part[i].index01 > UINT32_MAX)return -4;
        off+=(uint64_t)p->part[i].pregap*PS1_SECTOR;
        tracks[i].lba=(uint32_t)(off/PS1_SECTOR+p->part[i].index01);
        tracks[i].is_audio=p->part[i].is_audio;
        if(UINT64_MAX-off<sizes[i])return -5;
        off+=sizes[i];
    }
    if(off>UINT32_MAX * (uint64_t)PS1_SECTOR)return -6;
    *total=off;
    return 0;
}
