/* Verified 3D edition sources. Spanish first via Mubis (Spanish release
 * catalog), then specific cataloged Blu-ray.com releases. No claim that
 * generic TMDB/Cinemeta movie posters are scans of 3D editions. */
#include "movie_edition.h"
#include <stdio.h>
#include <ctype.h>
#include <string.h>

static int lowerc(int c){return (c>='A'&&c<='Z')?c+32:c;}
static int samei(const char *a,const char *b)
{
    while(*a&&*b){if(lowerc((unsigned char)*a)!=lowerc((unsigned char)*b))return 0;a++;b++;}
    return !*a&&!*b;
}
int movie_edition_slug(const char *s,char *out,size_t cap)
{
    size_t j=0;int dash=0;
    if(!s||!out||cap<4)return 0;
    while(*s&&j+2<cap){
        unsigned char c=(unsigned char)*s++;
        if(c==0xc3 && *s){
            unsigned char v=(unsigned char)*s++;
            if(v==0xa1||v==0x81||v==0xa0||v==0x80||v==0xa4||v==0x84)c='a';
            else if(v==0xa9||v==0x89||v==0xa8||v==0x88)c='e';
            else if(v==0xad||v==0x8d)c='i';
            else if(v==0xb3||v==0x93||v==0xb6||v==0x96)c='o';
            else if(v==0xba||v==0x9a||v==0xbc||v==0x9c)c='u';
            else if(v==0xb1||v==0x91)c='n';
            else continue;
        }
        c=(unsigned char)lowerc(c);
        if((c>='a'&&c<='z')||(c>='0'&&c<='9')){
            if(dash&&j)out[j++]='-';
            out[j++]=(char)c;dash=0;
        }else if(j)dash=1;
    }
    out[j]=0;
    return j>0;
}
/* Read an exact HTML meta tag's content. Mubis public title pages have
 * <meta property="og:title" content="Avatar Blu-ray 3D" />
 * <meta property="og:image" content="https://www.mubis.es/media/releases/...jpg" />.
 * It is unsafe to scrape any image from a whole page (ads, suggestions).
 */
static int meta(const char *page,const char *property,char *out,size_t cap)
{
    char needle[55];const char *p,*e,*v;size_t n;
    if(snprintf(needle,sizeof(needle),"property=\"%s\"",property)>=(int)sizeof(needle))
       return 0;
    p=strstr(page,needle);
    if(!p)return 0;
    e=strchr(p,'>');
    if(!e||e-p>512)return 0;
    v=strstr(p,"content=\"");
    if(!v||v>=e)return 0;
    v+=9;
    p=strchr(v,'"');
    if(!p||p>e)return 0;
    n=(size_t)(p-v);
    if(!n||n>=cap)return 0;
    memcpy(out,v,n);out[n]=0;
    return 1;
}
int movie_edition_mubis_html(const char *page,char *poster,size_t cap)
{
    char name[256],src[1024];
    size_t n;
    if(!page||!poster||cap==0)return 0;
    poster[0]=0;
    if(!meta(page,"og:title",name,sizeof(name)))return 0;
    /* Accept only actual Blu-ray 3D editions, never ordinary 2D media. */
    if(!strstr(name,"Blu-ray 3D")&&!strstr(name,"3D Blu-ray")&&
       !strstr(name,"Blu-ray 3d"))return 0;
    if(!meta(page,"og:image",src,sizeof(src)))return 0;
    if(strncmp(src,"https://www.mubis.es/media/releases/",36)!=0)return 0;
    if(!strstr(src,"-3d-") && !strstr(src,"-3D-"))return 0;
    n=strlen(src);
    if(n<5||strcmp(src+n-4,".jpg")!=0||n>=cap)return 0;
    memcpy(poster,src,n+1);
    return 1;
}

/* Catalog release IDs individually verified against pages declaring
 * Blu-ray 3D (never a movie ID or 2D edition). On Blu-ray.com, _front.jpg
 * is the edition's flat front scan, not a blue plastic case template.
 * Use it ONLY when the exact ISO movie/year is matched below.
 */
typedef struct {const char *name;int year;int es,en;} Edition;
static const Edition k_editions[]={
 {"avatar",2009,0,210144},
 {"thor",2011,0,82808},
 {"shrek",2001,0,30402},
 {"cars",2006,0,82250},
 {"rio",2011,33406,24347},
 {"dredd",2012,0,309431},
 {"how to train your dragon 2",2014,115682,106131},
 {"warcraft",2016,164503,0},
 {"the avengers",2012,0,37027},
 {"avengers",2012,0,37027},
 {"los vengadores",2012,0,37027}
};
int movie_edition_bluray_id(const char *title,int year,int spanish)
{
    unsigned i;
    if(!title||!year)return 0;
    for(i=0;i<sizeof(k_editions)/sizeof(k_editions[0]);i++)
       if(year==k_editions[i].year && samei(title,k_editions[i].name))
          return spanish?k_editions[i].es:k_editions[i].en;
    return 0;
}
