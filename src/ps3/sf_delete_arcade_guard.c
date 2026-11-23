/* V15.79: protect shared FBNeo/MAME parent and BIOS ROMs.
 * Read the SAME frozen clone->parent catalogue as arcade_native_select.c.
 * No writes, no game patching, no emulator or cover changes.
 */
#include "sf_delete_arcade_guard.h"
#include <sys/file.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <stddef.h>

#define ARCADE_PATH_MAX 512
#define ARCADE_MAX_CHILDREN 512
#ifndef SF_DELETE_ARCADE_MAP_PATH
#define SF_DELETE_ARCADE_MAP_PATH "/dev_hdd0/game/SPKF00001/USRDIR/ARCADE/arcade_parents.tsv"
#endif
#ifdef SF_DELETE_HOST_TEST
/* Inject a sandbox fixture without weakening production file paths. */
extern const char *sf_delete_arcade_test_map(void);
#define ARCADE_MAP sf_delete_arcade_test_map()
#else
#define ARCADE_MAP SF_DELETE_ARCADE_MAP_PATH
#endif

/* Static storage: do not put ~50KiB of clone metadata on the PS3 stack. */
static char s_children[ARCADE_MAX_CHILDREN][96];

static int eqi(const char *a,const char *b)
{
    while(*a && *b) {
        if(tolower((unsigned char)*a)!=tolower((unsigned char)*b)) return 0;
        a++;b++;
    }
    return !*a && !*b;
}
static int ends_i(const char *s,const char *suffix)
{
    size_t n=strlen(s),k=strlen(suffix);
    return n>=k && eqi(s+n-k,suffix);
}
static int valid_code(const char *s)
{
    size_t i,n=strlen(s);
    if(n<1 || n>90)return 0;
    for(i=0;i<n;i++){
        unsigned char ch=(unsigned char)s[i];
        if(!(isalnum(ch) || ch=='_' || ch=='+' || ch=='-' || ch=='.')) return 0;
    }
    return 1;
}
static int is_shared_bios(const char *rom)
{
    static const char *const bios[]={
      "neogeo","pgm","cps3","skns","sknsbios","naomi","naomi2",
      "awbios","stvbios","megatech","megadriv","konamigx",
      "taitofx1","tps","tpsbios","decocass","segasp","hng64",
      "hod2bios","f355bios","hikaru","chihiro","triforce",
      "sys573","atomiswave","naomigd","atlantis",
      "taitogn","nss","playch10","vsnes","cps1qs","qsound",
      "coh1000c","coh1000a","coh1002e","coh1002m","coh1002v",
      "coh3002c","coh3002t","konamigv"
    };
    unsigned i;
    if(strstr(rom,"bios")!=NULL)return 1;
    for(i=0;i<sizeof(bios)/sizeof(bios[0]);i++)
        if(eqi(rom,bios[i]))return 1;
    return 0;
}
int sf_delete_arcade_allowed(const char *path)
{
    const char *base,*ext;
    char dir[ARCADE_PATH_MAX],code[96],line[192];
    FILE *f;
    size_t stem,dirlen,i;
    int children=0,rows=0,fd,io_rc;
    u64 got=0;
    sysFSDirent ent;

    if(!path || !*path || strlen(path)>=ARCADE_PATH_MAX)return 0;
    base=strrchr(path,'/');
    if(!base || base==path || !base[1])return 0;
    dirlen=(size_t)(base-path);
    if(dirlen>=sizeof(dir))return 0;
    memcpy(dir,path,dirlen);dir[dirlen]=0;
    base++;
    ext=ends_i(base,".zip")?base+strlen(base)-4:
        ends_i(base,".7z")?base+strlen(base)-3:NULL;
    if(!ext)return 0;
    stem=(size_t)(ext-base);
    if(stem==0 || stem>=sizeof(code))return 0;
    memcpy(code,base,stem);code[stem]=0;
    for(i=0;i<stem;i++) code[i]=(char)tolower((unsigned char)code[i]);
    if(!valid_code(code) || is_shared_bios(code))return 0;

    f=fopen(ARCADE_MAP,"rb");
    if(!f)return 0; /* no metadata: fail CLOSED, never erase unknown BIOS */
    while(fgets(line,sizeof(line),f)) {
        char *sep, *child, *parent;
        size_t len=strlen(line);
        if(!len || line[0]=='#')continue;
        if(len==sizeof(line)-1 && line[len-1]!='\n'){
            fclose(f);
            return 0; /* unparseable metadata: never gamble */
        }
        sep=strchr(line,'\t');
        if(!sep)continue;
        *sep=0;child=line;parent=sep+1;
        parent[strcspn(parent,"\r\n")]=0;
        if(!valid_code(child) || !valid_code(parent))continue;
        rows++;
        if(eqi(code,parent) && !eqi(code,child)){
            if(children>=ARCADE_MAX_CHILDREN){fclose(f);return 0;}
            strcpy(s_children[children++],child);
        }
    }
    if(ferror(f)){fclose(f);return 0;}
    fclose(f);
    if(rows<10)return 0; /* reject incomplete/corrupt mapping */
    if(children==0)return 1; /* verified non-parent (or clone) */
    if(sysFsOpendir(dir,&fd)!=0)return 0;
    io_rc=0;
    while((io_rc=sysFsReaddir(fd,&ent,&got))==0 && got) {
        const char *nme=ent.d_name;
        size_t slen;
        char other[96];
        if(!strcmp(nme,".") || !strcmp(nme,".."))continue;
        slen=strlen(nme);
        if(ends_i(nme,".zip"))slen-=4;
        else if(ends_i(nme,".7z"))slen-=3;
        else continue;
        if(slen<1 || slen>=sizeof(other))continue;
        memcpy(other,nme,slen);other[slen]=0;
        for(i=0;i<(size_t)children;i++)
            if(eqi(other,s_children[i])){
                sysFsClosedir(fd);
                return 0; /* deleting parent would break an installed clone */
            }
    }
    if(sysFsClosedir(fd)!=0 || io_rc!=0)return 0;
    return 1;
}
