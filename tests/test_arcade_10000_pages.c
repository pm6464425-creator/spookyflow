/* Arcade 10000 game regression: 7500 ZIP/7z plus a 10k boundary test.
 * Tests pagination without touching console folder scanning. */
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "library.h"
#include "scanner.h"
#include "arcade_catalog.h"

static int s_total;
static int s_page_calls;
static int s_nonarcade_calls;
static int s_legacy_calls;

static int fake_unpaged(void *ctx,const char *dir,FsEntry *out,int max)
{
    (void)ctx;(void)dir;(void)out;(void)max;
    s_legacy_calls++;
    return -1; /* ARCADEROOT must never hit 4096 limit */
}
static int fake_arcade_page(void *ctx,const char *dir,FsEntry *out,int max,int off)
{
    int n=0,i;
    (void)ctx;
    assert(max==SCAN_ARCADE_PAGE_SIZE);
    assert(!strcmp(dir,"/dev_hdd0/ARCADE"));
    assert(off>=0 && off%SCAN_ARCADE_PAGE_SIZE==0);
    s_page_calls++;
    /* 2 non-ROM entries (COVERS directory and txt file) mixed into ROMs */
    for(i=off; i<s_total+2 && n<max; i++){
        memset(&out[n],0,sizeof(out[n]));
        if(i==1000){strcpy(out[n].name,"COVERS");out[n].is_dir=1;}
        else if(i==1001){strcpy(out[n].name,"readme.txt");out[n].size=300;}
        else{
            int index=i-(i>1000)-(i>1001);
            if(index==0)strcpy(out[n].name,"gng.zip");
            else if(index==1)strcpy(out[n].name,"ghouls.zip");
            else if(index==2)strcpy(out[n].name,"wh1.zip");
            else snprintf(out[n].name,sizeof(out[n].name),"arcade%05d.%s",index,index%7==0?"7z":"zip");
            out[n].size=1000+(uint64_t)index*17u;
        }
        n++;
    }
    return n;
}
static int fake_console_list(void *ctx,const char *dir,FsEntry *out,int max)
{
    (void)ctx;(void)out;(void)max;
    if(!strcmp(dir,"/dev_hdd0/PS3ISO")){s_nonarcade_calls++;return 0;}
    return -1;
}
static int hide_even(void *ctx,const Library *lib,const Game *g)
{
    (void)ctx;(void)lib;
    return (g->size%2)==0;
}
int main(void)
{
    Library *lib=(Library *)calloc(1,sizeof(*lib));
    Scanner *scan=(Scanner *)calloc(1,sizeof(*scan));
    const ScanRoot arcade={PLAT_ARCADE,"/dev_hdd0/ARCADE",SRC_HDD_ISO_DIR};
    const ScanRoot ps3={PLAT_PS3,"/dev_hdd0/PS3ISO",SRC_HDD_ISO_DIR};
    int cycles,i,hidden;
    assert(lib && scan);
    lib_clear(lib);
    s_total=7500;
    scanner_begin(scan,&arcade,1);
    scanner_set_arcade_page_reader(scan,fake_arcade_page);
    cycles=0;
    while(!scanner_step(scan,lib,fake_unpaged,NULL) && cycles<25)cycles++;
    assert(scan->done && cycles>=7);
    assert(s_legacy_calls==0);
    assert(s_page_calls>=8);
    assert(scan->found[PLAT_ARCADE]==7500);
    assert(lib->tab[PLAT_ARCADE].count==7500);
    for(i=0;i<PLAT_COUNT;i++)if(i!=PLAT_ARCADE)assert(lib->tab[i].count==0);
    lib_sort(lib,PLAT_ARCADE,0);
    assert(lib_find_key(lib,PLAT_ARCADE,"gng.zip")>=0);
    assert(lib_find_key(lib,PLAT_ARCADE,"ghouls.zip")>=0);
    assert(lib_find_key(lib,PLAT_ARCADE,"wh1.zip")>=0);
    hidden=lib_apply_hidden(lib,PLAT_ARCADE,hide_even,NULL,0);
    assert(hidden>0 && hidden<7500);
    assert(lib->tab[PLAT_ARCADE].count+lib->tab[PLAT_ARCADE].nhidden==7500);
    lib_sort(lib,PLAT_ARCADE,0);
    assert(lib->tab[PLAT_ARCADE].count==7500);
    printf("PASS: %d arcade ROMs via %d paged listing calls, sorted+hidden safely\n",
           lib->tab[PLAT_ARCADE].count,s_page_calls);

    /* Test hard stop: only ARCADE increases from 2000 to 10000. */
    lib_clear(lib);
    s_total=10200;s_page_calls=0;s_legacy_calls=0;
    scanner_begin(scan,&arcade,1);
    scanner_set_arcade_page_reader(scan,fake_arcade_page);
    for(i=0;i<25 && !scanner_step(scan,lib,fake_unpaged,NULL);i++){}
    assert(scan->done && s_page_calls>10);
    assert(lib->tab[PLAT_ARCADE].count==LIB_ARCADE_MAX_GAMES);
    assert(lib->tab[PLAT_ARCADE].dropped==200);
    printf("PASS: 10200 arcade archives safely limited to %d without memory write past array\n",
           lib->tab[PLAT_ARCADE].count);

    lib_clear(lib);
    scanner_begin(scan,&ps3,1);
    for(i=0;i<5 && !scanner_step(scan,lib,fake_console_list,NULL);i++){}
    assert(scan->done && s_nonarcade_calls==1);
    for(i=0;i<LIB_MAX_GAMES+1;i++){
        char name[64],path[96];
        snprintf(name,sizeof name,"Game%04d.iso",i);
        snprintf(path,sizeof path,"/dev_hdd0/PS3ISO/%s",name);
        lib_add(lib,PLAT_PS3,path,name,1000,NULL,0,0,0);
    }
    assert(lib->tab[PLAT_PS3].count==LIB_MAX_GAMES);
    assert(lib->tab[PLAT_PS3].dropped==1);
    for(i=0;i<PLAT_COUNT;i++)if(i!=PLAT_PS3)assert(lib->tab[i].count==0);
    puts("PASS: PS3 original 2000 cap, all legacy game folders and code unchanged");
    free(scan);free(lib);
    return 0;
}
