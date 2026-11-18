/* Arcade ROM identification: name-based FBNeo DAT catalogue.
 * This module is used ONLY by the PLAT_ARCADE scanner path.
 * ROM contents, filename, directories, emulators and console paths are untouched.
 * The packaged FBNeo arcade DAT is compiled into a compact sorted TSV. */
#include "arcade_catalog.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef ARCADE_DB
#define ARCADE_DB "/dev_hdd0/game/SPKF00001/USRDIR/ARCADE/arcade_titles.tsv"
#endif
#define ARCADE_MAX_DATA (1600L*1024L)
#define ARCADE_MAX_ENTRIES 16000
typedef struct { const char *rom; const char *title; } ArcadeEntry;
static ArcadeEntry *g_entries;
static char *g_bytes;
static int g_count, g_attempted;

static int by_rom(const void *a, const void *b)
{
    return strcmp(((const ArcadeEntry *)a)->rom, ((const ArcadeEntry *)b)->rom);
}
void arcade_catalog_init(void)
{
    FILE *f;
    long sz;
    char *p, *end;
    if (g_attempted) return;
    g_attempted=1;
    f=fopen(ARCADE_DB,"rb");
    if(!f) return; /* compile-time fallback IDs still available */
    if(fseek(f,0,SEEK_END)!=0 || (sz=ftell(f))<=0 || sz>ARCADE_MAX_DATA ||
       fseek(f,0,SEEK_SET)!=0){ fclose(f); return; }
    g_bytes=(char *)malloc((size_t)sz+1u);
    g_entries=(ArcadeEntry *)malloc(sizeof(ArcadeEntry)*ARCADE_MAX_ENTRIES);
    if(!g_bytes || !g_entries || fread(g_bytes,1,(size_t)sz,f)!=(size_t)sz){
        free(g_bytes); free(g_entries); g_bytes=0; g_entries=0; fclose(f); return;
    }
    fclose(f);
    g_bytes[sz]=0;
    p=g_bytes; end=g_bytes+sz;
    while(p<end && g_count<ARCADE_MAX_ENTRIES){
        char *line=p, *tab, *nl=strchr(p,'\n');
        if(nl){*nl=0; p=nl+1;} else p=end;
        if(!line[0] || line[0]=='#')continue;
        tab=strchr(line,'\t');
        if(!tab || tab==line || !tab[1])continue;
        *tab++=0;
        if(strchr(tab,'\r'))*strchr(tab,'\r')=0;
        if(!tab[0])continue;
        g_entries[g_count].rom=line;
        g_entries[g_count].title=tab;
        g_count++;
    }
    if(g_count) qsort(g_entries,(size_t)g_count,sizeof(g_entries[0]),by_rom);
}
static const ArcadeEntry k_legacy[]={
    {"1942","1942"},
    {"aburner","After Burner"},
    {"aburner2","After Burner II"},
    {"bangbead","Bang Bead"},
    {"commando","Commando"},
    {"digdug","Dig Dug"},
    {"frogger","Frogger"},
    {"galaga","Galaga"},
    {"ghouls","Ghouls'n Ghosts"},
    {"gng","Ghosts'n Goblins"},
    {"hunchbak","Hunchback"},
    {"klax","Klax"},
    {"macross","Macross"},
    {"mustang","U.N. Squadron / Mustang"},
    {"offroad","Super Off Road"},
    {"pacman","Pac-Man"},
    {"pass","Pass"},
    {"peterpak","Peter Pack Rat"},
    {"pitfall2","Pitfall II"},
    {"popeye","Popeye"},
    {"ribbit","Ribbit!"},
    {"salamand","Salamander"},
    {"starforc","Star Force"},
    {"strahl","Strahl"},
    {"sub","Submarine"},
    {"tinklpit","Tinkle Pit"},
    {"tubep","Tube Panic"},
    {"vandyke","Vandyke"},
    {"vimana","Vimana"},
    {"wc90b1","Tecmo World Cup '90"},
    {"wcbowl","World Class Bowling"},
    {"wh1","World Heroes"},
    {"wingforc","Wing Force"},
    {"youjyudn","Youjyuden"}
};
const char *arcade_catalog_title(const char *filename)
{
    char rom[128];
    const char *base=filename, *dot;
    size_t n, i;
    ArcadeEntry key, *hit;
    if(!filename || !*filename)return NULL;
    for(const char *q=filename; *q; ++q) if(*q=='/' || *q=='\\')base=q+1;
    dot=strrchr(base,'.');
    n=dot?(size_t)(dot-base):strlen(base);
    if(n==0 || n>=sizeof rom)return NULL;
    for(i=0;i<n;i++){
        unsigned char c=(unsigned char)base[i];
        rom[i]=(char)((c>='A' && c<='Z')? c+'a'-'A':c);
    }
    rom[n]=0;
    if(!g_attempted)arcade_catalog_init();
    if(g_count){
        key.rom=rom; key.title=0;
        hit=(ArcadeEntry *)bsearch(&key,g_entries,(size_t)g_count,sizeof(g_entries[0]),by_rom);
        if(hit)return hit->title;
    }
    for(i=0;i<sizeof(k_legacy)/sizeof(k_legacy[0]);i++)
        if(strcmp(rom,k_legacy[i].rom)==0)return k_legacy[i].title;
    return NULL; /* unknown set stays visible under original ROM name */
}
int arcade_catalog_count(void){ if(!g_attempted)arcade_catalog_init(); return g_count; }
