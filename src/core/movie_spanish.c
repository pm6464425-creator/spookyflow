/* Offline BDISO Spanish localization.  The user's actual USB film names
 * are paired with common Spanish release titles. External APIs still
 * provide artwork; this module controls only the visible title. */
#include "movie_spanish.h"
#include <ctype.h>
#include <stdio.h>
#include <string.h>

typedef struct {const char *original,*english,*spanish;} MovieEs;
static const MovieEs k_es[]={
 {"terminator 2 el juicio final","terminator 2 judgment day","Terminator 2: El juicio final"},
 {"shrek","shrek","Shrek"},
 {"yo robot","i robot","Yo, robot"},
 {"cars","cars","Cars: Una aventura sobre ruedas"},
 {"journey to the center of the earth","journey to the center of the earth","Viaje al centro de la Tierra"},
 {"jumper","jumper","Jumper"},
 {"kung fu panda","kung fu panda","Kung Fu Panda"},
 {"avatar","avatar","Avatar"},
 {"ice age dawn of the dinosaurs","ice age dawn of the dinosaurs","La era de hielo 3: El origen de los dinosaurios"},
 {"under the sea imax","under the sea","Bajo el mar"},
 {"clash of the titans","clash of the titans","Furia de titanes"},
 {"how to train your dragon","how to train your dragon","Cómo entrenar a tu dragón"},
 {"final destination v","final destination 5","Destino final 5"},
 {"rio","rio","Río"},
 {"thor","thor","Thor"},
 {"dredd","dredd","Dredd"},
 {"47 ronin","47 ronin","47 Ronin"},
 {"how to train your dragon ii","how to train your dragon 2","Cómo entrenar a tu dragón 2"},
 {"mr peabody sherman","mr peabody & sherman","Las aventuras de Peabody y Sherman"},
 {"x men days of future past","x-men: days of future past","X-Men: Días del futuro pasado"},
 {"in the heart of the sea","in the heart of the sea","En el corazón del mar"},
 {"pixels","pixels","Pixeles"},
 {"batman v superman dawn of justice","batman v superman: dawn of justice","Batman vs. Superman: El origen de la justicia"},
 {"doctor strange","doctor strange","Doctor Strange: Hechicero supremo"},
 {"suicide squad","suicide squad","Escuadrón suicida"},
 {"warcraft the beginning","warcraft","Warcraft: El primer encuentro de dos mundos"}
};
static int is_sep(int c)
{
    return c==' ' || c=='_' || c=='-' || c=='.' || c==',' ||
           c==':' || c=='(' || c==')' || c=='&';
}
int movie_filename_is_3d(const char *path)
{
    size_t i,n;
    if(!path)return 0;
    n=strlen(path);
    for(i=0;i+1<n;i++)
        if(path[i]=='3' && (path[i+1]=='D'||path[i+1]=='d') &&
           (i==0||is_sep((unsigned char)path[i-1])) &&
           (i+2==n||is_sep((unsigned char)path[i+2])))return 1;
    return 0;
}
/* Normalizes the filename to its movie-name key.  No allocations. */
static void normal(const char *src,char *dst,size_t cap,int filename)
{
    const char *base=src;size_t n=0,i;
    if(!src||!cap)return;
    if(filename){
        const char *slash=strrchr(src,'/');
        if(slash)base=slash+1;
    }
    for(i=0;base[i]&&n+1<cap;i++){
        unsigned char c=(unsigned char)base[i];
        if(c>='A'&&c<='Z')c=(unsigned char)tolower(c);
        if((c>='a'&&c<='z')||(c>='0'&&c<='9'))
            dst[n++]=(char)c;
        else if(is_sep(c)&&n&&dst[n-1]!=' ')
            dst[n++]=' ';
    }
    while(n&&dst[n-1]==' ')n--;
    dst[n]=0;
    if(filename){
        /* The ISO name starts with YYYY. Remove its 3D suffix and disc
         * counter without touching the physical file or its artwork key. */
        if(n>=4 && dst[0]>='1'&&dst[0]<='2' &&
           isdigit((unsigned char)dst[1])&&isdigit((unsigned char)dst[2])&&
           isdigit((unsigned char)dst[3])&&dst[4]==' '){
            memmove(dst,dst+5,strlen(dst+5)+1);
        }
        n=strlen(dst);
        if(n>=4 && strcmp(dst+n-4," iso")==0)dst[n-4]=0;
        n=strlen(dst);
        /* .iso.0 and split-disc names */
        if(n>=2 && strcmp(dst+n-2," 0")==0)dst[n-2]=0;
        n=strlen(dst);
        while(n>0 && dst[n-1]==' ')dst[--n]=0;
        if(n>=2 && strcmp(dst+n-2,"3d")==0){dst[n-2]=0;}
        else{
            /* 3D-001 / 3D_2 / BD3D suffix */
            char *p=strstr(dst," 3d ");
            if(p && p[4] && strspn(p+4,"0123456789")==strlen(p+4))*p=0;
            else if(n>=3 && strcmp(dst+n-3," 3d")==0)dst[n-3]=0;
        }
        n=strlen(dst);while(n&&dst[n-1]==' ')dst[--n]=0;
        /* 3D-001 can occur in .iso.0 or other tag forms. */
        if(n>=5&&strcmp(dst+n-5," bd3d")==0)dst[n-5]=0;
    }
}
int movie_spanish_title(const char *iso_filename,const char *sidecar_title,
                        char *out,size_t cap)
{
    char key[256],cur[256],eng[256];unsigned i;
    if(!iso_filename||!out||cap<8)return 0;
    normal(iso_filename,key,sizeof(key),1);
    if(sidecar_title&&sidecar_title[0])normal(sidecar_title,cur,sizeof(cur),0);
    else cur[0]=0;
    for(i=0;i<sizeof(k_es)/sizeof(k_es[0]);i++){
        if(strcmp(key,k_es[i].original)!=0)continue;
        if(cur[0]){
            normal(k_es[i].english,eng,sizeof(eng),0);
            /* Respect non-English custom .title files unless they match
             * the known original/canonical source title. */
            if(strcmp(cur,eng)!=0 && strcmp(cur,key)!=0)return 0;
        }
        snprintf(out,cap,"%s",k_es[i].spanish);
        return 1;
    }
    return 0;
}
