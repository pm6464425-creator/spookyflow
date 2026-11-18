/* Arcade-only catalog+scanner regression.
 * Simulates the exact /dev_hdd0/ARCADE folder with real shortened ZIP names,
 * a 7z archive and a COVERS folder that must not appear as a game.
 * PS1, PS2, PS3, BDISO and SNES are never modified by these tests. */
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "arcade_catalog.h"
#include "scanner.h"
#include "library.h"

static int fake_list(void *ctx,const char *dir,FsEntry *out,int max)
{
    int n=0;
    (void)ctx;
    if(max<9)return -1;
#define ADD(N,D,S) do{strcpy(out[n].name,N);out[n].is_dir=D;out[n].size=S;n++;}while(0)
    if(!strcmp(dir,"/dev_hdd0/ARCADE")){
        ADD("gng.zip",0,155000); ADD("ghouls.zip",0,1571000);
        ADD("wh1.zip",0,4688000); ADD("unknown.7z",0,4688000);
        ADD("16).zip",0,555); ADD("17. prototype).zip",0,444);
        ADD("COVERS",1,0); ADD("nested",1,0); ADD("unrelated.txt",0,17);
    }else if(!strcmp(dir,"/dev_hdd0/ARCADE/nested")) {
        ADD("pacman.zip",0,14000);
    }else if(!strcmp(dir,"/dev_hdd0/ARCADE/COVERS")){
        /* This must NEVER be reached; non-arcade artwork is not a ROM. */
        assert(0 && "COVERS mistakenly scanned as games");
    }else return -1;
#undef ADD
    return n;
}
int main(int argc,char **argv)
{
    static const ScanRoot roots[]={{PLAT_ARCADE,"/dev_hdd0/ARCADE",SRC_HDD_ISO_DIR}};
    Scanner *s=(Scanner *)calloc(1,sizeof *s);
    Library *lib=(Library *)calloc(1,sizeof *lib);
    int n=0,i, gng=0,ghouls=0,wh1=0,pacman=0,other=0, malformed=0;
    const ScanRoot *r;
    assert(s && lib);
    r=scanner_default_roots(&n);
    {int h=0,old_bad=0;
      for(i=0;i<n;i++){ if(r[i].plat==PLAT_ARCADE&&strcmp(r[i].path,"/dev_hdd0/ARCADE")==0)h++;
                       if(strstr(r[i].path,"/ROMS/ARCADE"))old_bad++; }
      assert(h==1 && !old_bad);
    }
    assert(arcade_catalog_title("gng.zip") && strstr(arcade_catalog_title("gng.zip"),"Ghost"));
    assert(arcade_catalog_title("wh1.zip") && strstr(arcade_catalog_title("wh1.zip"),"World Heroes"));
    assert(arcade_catalog_title("ghouls.zip") && strstr(arcade_catalog_title("ghouls.zip"),"Ghost"));
    assert(arcade_catalog_title("doesnotexist.zip")==NULL);
    if(argc>1 && !strcmp(argv[1],"full")) assert(arcade_catalog_count()>=4000);
    lib_clear(lib);
    scanner_begin(s,roots,1);
    for(i=0;i<20;i++)if(scanner_step(s,lib,fake_list,NULL))break;
    assert(s->done);
    assert(s->found[PLAT_ARCADE]==7);
    assert(lib->tab[PLAT_ARCADE].count==7);
    for(i=0;i<lib->tab[PLAT_ARCADE].count;i++){
        const Game *g=&lib->tab[PLAT_ARCADE].games[i];
        const char *p=lib_path(lib,g),*t=lib_title(lib,g);
        if(strstr(p,"/gng.zip")){gng++;assert(strstr(t,"Ghost"));}
        if(strstr(p,"/ghouls.zip")){ghouls++;assert(strstr(t,"Ghost"));}
        if(strstr(p,"/wh1.zip")){wh1++;assert(strstr(t,"World Heroes"));}
        if(strstr(p,"/pacman.zip")){pacman++;assert(strstr(t,"Pac"));}
        if(strstr(p,"/unknown.7z")){other++;assert(strcmp(t,"unknown")==0);}
        if(strstr(p,"/16).zip") || strstr(p,"/17. prototype).zip")){
            malformed++;
            assert(strcmp(t,"ROM sin identificar")==0);
        }
        assert(strcmp(t,"16)")!=0 && strcmp(t,"15)")!=0 &&
               strcmp(t,"17. prototype)")!=0);
    }
    assert(gng==1 && ghouls==1 && wh1==1 && pacman==1 && other==1 && malformed==2);
    for(i=0;i<PLAT_COUNT;i++)if(i!=PLAT_ARCADE)assert(lib->tab[i].count==0);
    printf("PASS: Arcade direct folder, 7 ROMs, malformed names neutralized, no COVERS, ZIP+7z, official names=%d, other platforms untouched\n",arcade_catalog_count());
    free(s);free(lib);
    return 0;
}
