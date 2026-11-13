#include "sf_ps1_native_plan.h"
#include <ctype.h>
#include <stdio.h>
#include <string.h>
#include <strings.h>
#include <stdlib.h>
#include <limits.h>
static int word(const char *p,const char *x)
{
    size_t n=strlen(x);
    return strncasecmp(p,x,n)==0 && (p[n]==' ' || p[n]=='\t' || p[n]==0);
}
static const char *spaces(const char *p)
{
    while(*p==' ' || *p=='\t')p++;
    return p;
}
static int cue_file(const char *cue,const char *p,char *out)
{
    const char *name,*slash;size_t d,n;
    char base[512];
    p=spaces(p);
    if(*p=='"'){
        const char *q=strchr(++p,'"');
        if(!q || q==p || (size_t)(q-p)>=sizeof(base))return -1;
        n=(size_t)(q-p);memcpy(base,p,n);base[n]=0;
        if(!word(spaces(q+1),"BINARY"))return -1;
    }else{
        const char *q=p;
        while(*q && *q!=' ' && *q!='\t')q++;
        if(q==p || (size_t)(q-p)>=sizeof(base))return -1;
        n=(size_t)(q-p);memcpy(base,p,n);base[n]=0;
        if(!word(spaces(q),"BINARY"))return -1;
    }
    /* No cross-directory paths, drive letters or traversal. A fallback
       can handle complex external references without changing any file. */
    if(strstr(base,"..") || strchr(base,'/') || strchr(base,'\\') ||
       strchr(base,':') || base[0]=='.')return -1;
    if(!strrchr(base,'.') || !( !strcasecmp(strrchr(base,'.'),".bin") ||
                                 !strcasecmp(strrchr(base,'.'),".img") ))return -1;
    slash=strrchr(cue,'/');
    if(!slash || cue[0]!='/' || !slash[1])return -1;
    d=(size_t)(slash-cue+1);name=base;
    if(d+strlen(name)>=SF_PS1_IMAGE_MAX)return -1;
    memcpy(out,cue,d);strcpy(out+d,name);
    return 0;
}
static int cue_time(const char *p,uint32_t *lba)
{
    unsigned m,s,f;char tail=0;
    if(sscanf(p,"%u:%u:%u%c",&m,&s,&f,&tail)!=3 ||
       s>=60 || f>=75 || m>950)return -1;
    *lba=(m*60u+s)*75u+f;
    return 0;
}
int sf_ps1_plan_cue(const char *cue_path,const char *text,size_t length,
                    SfPs1NativePlan *out)
{
    const char *p=text;size_t remaining=length;int files=0,track=-1;
    int have_index[SF_PS1_MAX_TRACKS]={0};
    if(!cue_path || !text || !out || length==0 || length>32768)return -1;
    memset(out,0,sizeof(*out));
    while(remaining){
        char line[640];size_t n=0;const char *cur;
        while(n<remaining && p[n]!='\n' && p[n]!='\r')n++;
        if(n>=sizeof(line))return -1;
        memcpy(line,p,n);line[n]=0;
        p+=n;remaining-=n;
        while(remaining && (*p=='\r' || *p=='\n')){p++;remaining--;}
        cur=spaces(line);
        if(!*cur || word(cur,"REM") || word(cur,"TITLE") || word(cur,"PERFORMER") ||
           word(cur,"SONGWRITER") || word(cur,"FLAGS") || word(cur,"ISRC") ||
           word(cur,"CATALOG"))continue;
        if(word(cur,"FILE")){
            if(++files!=1 || track>=0 ||
               cue_file(cue_path,cur+4,out->image)!=0)return -1;
        }else if(word(cur,"TRACK")){
            int t=0;char mode[48]={0};
            if(files!=1 || ++track>=SF_PS1_MAX_TRACKS)return -1;
            if(sscanf(cur+5,"%d %47s",&t,mode)!=2 || t!=track+1)return -1;
            if(!strcasecmp(mode,"AUDIO"))out->tracks[track].is_audio=1;
            else if((!strcasecmp(mode,"MODE1/2352") ||
                     !strcasecmp(mode,"MODE2/2352")) && track==0)
                out->tracks[track].is_audio=0;
            else if(!strcasecmp(mode,"MODE1/2352") ||
                    !strcasecmp(mode,"MODE2/2352"))
                out->tracks[track].is_audio=0;
            else return -1;
        }else if(word(cur,"INDEX")){
            unsigned i;char time[64];
            uint32_t lba;
            if(track<0 || sscanf(cur+5,"%u %63s",&i,time)!=2 ||
               i>1 || cue_time(time,&lba)!=0)return -1;
            if(i==1){
                if(have_index[track])return -1;
                out->tracks[track].lba=lba;
                have_index[track]=1;
            }
        }else if(word(cur,"PREGAP") || word(cur,"POSTGAP"))return -1;
        else return -1;
    }
    if(files!=1 || track<0 || out->tracks[0].is_audio ||
       out->tracks[0].lba!=0)return -1;
    out->num_tracks=(unsigned)(track+1);
    for(int i=0;i<=track;i++){
        if(!have_index[i] || (i>0 && out->tracks[i].lba<=out->tracks[i-1].lba))
            return -1;
    }
    return 0;
}
