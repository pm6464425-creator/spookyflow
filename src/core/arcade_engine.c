/* Arcade engine router. Only the ARCADE tab calls this module.
 * Genuine FBNeoRLPlus v2.30 retro.db is compiled into shortname<TAB>engine
 * without adding 30 MB of SQLite to the user's PKG. */
#include "arcade_engine.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#define MAX_ENGINE_CODES 7500
typedef struct {char code[96];unsigned char engine;} ArcadeRoute;
static ArcadeRoute *routes;
static int count,initialized;
static int cmp_route(const void *a,const void *b)
{
    return strcmp(((const ArcadeRoute *)a)->code,((const ArcadeRoute *)b)->code);
}
static void load_routes(void)
{
    FILE *f;
    char line[180];
    if(initialized)return;
    initialized=1;
    f=fopen(ARCADE_ENGINE_MAP_PATH,"rb");
    if(!f)return;
    routes=(ArcadeRoute *)calloc(MAX_ENGINE_CODES,sizeof *routes);
    if(!routes){fclose(f);return;}
    while(count<MAX_ENGINE_CODES && fgets(line,sizeof line,f)){
        char *tab,*end;
        size_t n;
        if(line[0]=='#')continue;
        tab=strchr(line,'\t');
        if(!tab)continue;
        *tab++=0;
        n=strlen(line);
        if(n==0 || n>=sizeof(routes[0].code))continue;
        end=tab+strcspn(tab,"\r\n");
        *end=0;
        if(strcmp(tab,"F")!=0 && strcmp(tab,"M")!=0)continue;
        memcpy(routes[count].code,line,n+1);
        routes[count].engine=tab[0]=='M'?ARCADE_ENGINE_MAME125:ARCADE_ENGINE_FBNEO;
        count++;
    }
    fclose(f);
    qsort(routes,(size_t)count,sizeof *routes,cmp_route);
}
static int code_for_path(const char *path,char *code,size_t cap)
{
    const char *base,*p;
    size_t n,i;
    if(!path||strncmp(path,"/dev_hdd0/ARCADE/",16)!=0)return 0;
    base=strrchr(path,'/');
    base=base?base+1:path;
    n=strlen(base);
    /* The actual native PS3 cores expect ZIP archives, not 7z. */
    if(n<=4 || strcasecmp(base+n-4,".zip")!=0)return 0;
    n-=4;
    if(!n || n>=cap || n>90)return 0;
    for(i=0;i<n;i++){
        unsigned char c=(unsigned char)base[i];
        if(!(isalnum(c)||c=='_'||c=='.'||c=='+'||c=='-'))return 0;
        code[i]=(char)tolower(c);
    }
    code[n]=0;
    /* BIOS archives are used as dependencies, not selectable games. */
    if(!strcmp(code,"neogeo")||!strcmp(code,"pgm")||!strcmp(code,"cps3")||
       !strcmp(code,"qsound")||!strcmp(code,"skns")||!strcmp(code,"decocass"))
        return 0;
    return 1;
}
int arcade_engine_for_rom(const char *path)
{
    char code[96]; ArcadeRoute key,*found;
    if(!code_for_path(path,code,sizeof code))return 0;
    load_routes();
    if(!count)return 0;
    memset(&key,0,sizeof key);
    memcpy(key.code,code,strlen(code)+1);
    found=bsearch(&key,routes,(size_t)count,sizeof *routes,cmp_route);
    return found?found->engine:ARCADE_ENGINE_NONE;
}
int arcade_engine_list_count(void){load_routes();return count;}
