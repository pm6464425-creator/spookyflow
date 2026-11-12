/* movie_catalog.c - small, safe parser for Cinemeta's JSON movie metadata.
 * The old code used strchr(array, ']') and strchr(object, '}'), which
 * stopped at a nested genres or behaviorHints field and rejected all
 * 26 movies in the real user's PS3 diagnostics. No full JSON dependency.
 */
#include "movie_catalog.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MC_MAX_POSTER 1024
#define MC_MAX_NAME 256

/* Match balanced nested object/array delimiters, skipping string escapes. */
static const char *mc_end(const char *p)
{
    unsigned depth=0;int quote=0,escape=0;
    if(!p||(*p!='{'&&*p!='['))return NULL;
    for(;*p;p++){
        unsigned char c=(unsigned char)*p;
        if(quote){
            if(escape)escape=0;
            else if(c=='\\')escape=1;
            else if(c=='"')quote=0;
        }else if(c=='"')quote=1;
        else if(c=='{'||c=='['){
            if(++depth>64)return NULL;
        }else if(c=='}'||c==']'){
            if(!depth)return NULL;
            if(--depth==0)return p+1;
        }
    }
    return NULL;
}

/* JSON strings in the chosen fields are typically UTF-8. Decode JSON's
 * escaped slashes/quotes, while rejecting malformed control escapes. */
static int mc_text(const char *obj,const char *end,const char *key,
                   char *out,size_t cap)
{
    char field[48];const char *p;size_t k=0;
    if(!obj||!end||!key||!cap)return 0;
    if(snprintf(field,sizeof(field),"\"%s\"",key)>=(int)sizeof(field))return 0;
    p=obj;
    while(p<end && (p=strstr(p,field))!=NULL){
        const char *v;size_t i=0;
        if(p>=end)return 0;
        v=p+strlen(field);
        while(v<end&&(*v==' '||*v=='\t'||*v=='\r'||*v=='\n'))v++;
        if(v>=end||*v!=':'){p=v;continue;}
        v++;
        while(v<end&&(*v==' '||*v=='\t'||*v=='\r'||*v=='\n'))v++;
        if(v>=end||*v!='"'){p=v;continue;}
        v++;
        while(v<end&&i+1<cap&&*v!='"'){
            unsigned char c=(unsigned char)*v++;
            if(c=='\\'){
                if(v>=end)return 0;
                c=(unsigned char)*v++;
                if(c=='u'){
                    unsigned cp=0;int j;
                    if(end-v<4)return 0;
                    for(j=0;j<4;j++){
                        unsigned char h=(unsigned char)v[j];
                        if(h>='0'&&h<='9')cp=cp*16+h-'0';
                        else if(h>='a'&&h<='f')cp=cp*16+h-'a'+10;
                        else if(h>='A'&&h<='F')cp=cp*16+h-'A'+10;
                        else return 0;
                    }
                    v+=4;
                    if(cp<128)c=(unsigned char)cp;
                    else if(cp<=0x7ff && i+2<cap){
                        out[i++]=(char)(0xc0|(cp>>6));
                        out[i++]=(char)(0x80|(cp&63));
                        continue;
                    }else if(cp>=0x800 && cp<0xd800 && i+3<cap){
                        out[i++]=(char)(0xe0|(cp>>12));
                        out[i++]=(char)(0x80|((cp>>6)&63));
                        out[i++]=(char)(0x80|(cp&63));
                        continue;
                    }else return 0;
                }else if(c=='n'||c=='r'||c=='t')c=' ';
                else if(c!='"'&&c!='\\'&&c!='/')return 0;
            }
            out[i++]=(char)c;
        }
        if(v>=end||*v!='"')return 0;
        out[i]=0;
        return i>0;
    }
    (void)k;return 0;
}

static int mc_host_ok(const char *url)
{
    static const char *const allowed[]={
        "https://image.tmdb.org/",
        "https://images.metahub.space/",
        "https://m.media-amazon.com/"
    };
    unsigned i;
    if(!url||strlen(url)>=MC_MAX_POSTER)return 0;
    for(i=0;i<sizeof(allowed)/sizeof(allowed[0]);i++)
        if(strncmp(url,allowed[i],strlen(allowed[i]))==0)return 1;
    return 0;
}

static void mc_norm(const char *src,char *out,size_t cap)
{
    size_t n=0;char word[80];unsigned w=0;
    if(cap==0)return;
    while(1){
        unsigned char c=(unsigned char)*src;
        if(c && ((c>='a'&&c<='z')||(c>='A'&&c<='Z')||
                 (c>='0'&&c<='9'))){
            if(w+1<sizeof(word))word[w++]=(char)tolower(c);
        }else{
            if(w){
                word[w]=0;
                if(strcmp(word,"and")!=0 && strcmp(word,"v")!=0){
                    const char *add=word;
                    if(strcmp(word,"ii")==0)add="2";
                    else if(strcmp(word,"iii")==0)add="3";
                    else if(strcmp(word,"iv")==0)add="4";
                    if(n && n+1<cap)out[n++]=' ';
                    while(*add && n+1<cap)out[n++]=*add++;
                }
                w=0;
            }
        }
        if(!c)break;
        src++;
    }
    out[n]=0;
}

static void mc_alias(const char *norm,char *out,size_t cap)
{
    static const struct {const char *from,*to;} aliases[]={
        {"yo robot","i robot"},
        {"terminator 2 el juicio final","terminator 2 judgment day"},
        {"final destination 5","final destination 5"},
        {"final destination v","final destination 5"},
        {"warcraft the beginning","warcraft"},
        {"under the sea imax","under the sea"}
    };
    unsigned i;
    snprintf(out,cap,"%s",norm);
    for(i=0;i<sizeof(aliases)/sizeof(aliases[0]);i++)
        if(strcmp(norm,aliases[i].from)==0){
            snprintf(out,cap,"%s",aliases[i].to);
            break;
        }
}

static int mc_year(const char *obj,const char *end)
{
    char release[64];
    if((mc_text(obj,end,"releaseInfo",release,sizeof(release)) ||
        mc_text(obj,end,"year",release,sizeof(release))) &&
       isdigit((unsigned char)release[0])){
        int y=atoi(release);
        if(y>1900 && y<2100)return y;
    }
    /* Cinemeta's year can also be a numeric JSON value. */
    {
        const char *p=strstr(obj,"\"year\"");
        if(p&&p<end){
            p+=6;while(p<end && (*p==' '||*p==':'||*p=='\t'))p++;
            if(p<end && isdigit((unsigned char)*p)){
                int y=atoi(p);
                if(y>1900 && y<2100)return y;
            }
        }
    }
    return 0;
}

int movie_catalog_pick(const char *json,const char *query,int year,
                       char *poster,size_t poster_cap,
                       char *title,size_t title_cap)
{
    const char *array,*end,*p;char qnorm[256],qalias[256];
    char chosen[MC_MAX_POSTER]="",chosen_title[MC_MAX_NAME]="";
    int best=0,seen=0;
    if(!json||!query||!poster||!poster_cap||!title||!title_cap)return 0;
    poster[0]=0;title[0]=0;
    p=strstr(json,"\"metas\"");
    if(!p || !(array=strchr(p,'[')) || !(end=mc_end(array)))return 0;
    mc_norm(query,qnorm,sizeof(qnorm));
    mc_alias(qnorm,qalias,sizeof(qalias));
    for(p=array+1;p<end && seen<60;){
        char name[MC_MAX_NAME],url[MC_MAX_POSTER],norm[256];
        int y,score=0;
        const char *stop;
        while(p<end && (*p==' '||*p==','||*p=='\r'||*p=='\n'||*p=='\t'))p++;
        if(p>=end||*p==']')break;
        if(*p!='{')break;
        stop=mc_end(p);
        if(!stop||stop>end)break;
        seen++;
        if(mc_text(p,stop,"name",name,sizeof(name)) &&
           mc_text(p,stop,"poster",url,sizeof(url)) &&
           mc_host_ok(url)){
            mc_norm(name,norm,sizeof(norm));
            if(strcmp(qalias,norm)==0 || strcmp(qnorm,norm)==0)score=20;
            else if(strlen(qalias)>5 &&
                    (strstr(norm,qalias)==norm || strstr(qalias,norm)==qalias))
                    score=8;
            y=mc_year(p,stop);
            if(year && y && y!=year)score=0;
            else if(score && year && y==year)score+=10;
            if(score>best){
                best=score;
                snprintf(chosen,sizeof(chosen),"%s",url);
                snprintf(chosen_title,sizeof(chosen_title),"%s",name);
            }
        }
        p=stop;
    }
    if(!best || strlen(chosen)>=poster_cap || strlen(chosen_title)>=title_cap)return 0;
    strcpy(poster,chosen);strcpy(title,chosen_title);
    return 1;
}
