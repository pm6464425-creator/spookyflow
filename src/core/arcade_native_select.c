/* SpookyFlow V15.67: ORIGINAL PS3 MAME0125 automatic fallback.
 *
 * Proven against original MAME 0.125 neodrvr.c (tag mame0125) and FBNeo
 * fccba95f2: modern FBNeo demands SM1 94416d67, SFIX c2ea0cfd and
 * LO 5a86cff2; original MAME0125 uses exactly the user's old bios:
 * SM1 97cf998b, SFIX 354029fc and LO e09e253c (64 KiB).
 *
 * For MAME0125 NeoGeo games, verify *every* original driver's declared
 * chip using central-directory CRC+uncompressed size, including BIOS.
 * Only then prefer MAME0125. This fixes KOF2002 without renaming or
 * rewriting any 80MB user archive, or supplying proprietary BIOS data.
 *
 * Every other platform and non-NeoGeo ARCADE routing stays unchanged.
 */
#include "arcade_native_select.h"
#include "arcade_engine.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <stdint.h>
#include <ctype.h>

#ifndef ARCADE_MAME125_CATALOG_PATH
#define ARCADE_MAME125_CATALOG_PATH SF_MAME125_NEOGEO_CATALOG
#endif
#ifndef ARCADE_NATIVE_BIOS_PATH
#define ARCADE_NATIVE_BIOS_PATH "/dev_hdd0/ARCADE/neogeo.zip"
#endif
#ifndef ARCADE_NATIVE_ROM_ROOT
#define ARCADE_NATIVE_ROM_ROOT "/dev_hdd0/ARCADE/"
#endif
#ifndef ARCADE_LEGACY_GAMES_PATH
#define ARCADE_LEGACY_GAMES_PATH "/dev_hdd0/game/SPKF00001/USRDIR/ARCADE/fbneo_legacy_games.tsv"
#endif
/* Strict catalog generated from original PS3 FBNeo compiled ROM descriptors
 * and MAME 0.125 CRCs. Not a global allow-all fallback: unsupported games
 * must not be passed to a core that cannot parse them. */
static int legacy_allowlisted(const char *code)
{
    FILE *f=fopen(ARCADE_LEGACY_GAMES_PATH,"rb");
    char line[128];
    int valid=0;
    if(!f || !code)return 0;
    while(fgets(line,sizeof line,f)){
        char *tab=strchr(line,'\t');
        if(!tab)continue;
        *tab=0;
        if(strcmp(code,line)==0){valid=1;break;}
    }
    fclose(f);
    return valid;
}

#ifndef ARCADE_NATIVE_PARENT_MAP_PATH
#define ARCADE_NATIVE_PARENT_MAP_PATH "/dev_hdd0/game/SPKF00001/USRDIR/ARCADE/arcade_parents.tsv"
#endif
#ifndef ARCADE_NATIVE_LEGACY_CLONES_PATH
#define ARCADE_NATIVE_LEGACY_CLONES_PATH "/dev_hdd0/game/SPKF00001/USRDIR/ARCADE/fbneo_legacy_clones.tsv"
#endif
/* Read exact frozen 2021 FBNeo clone->parent metadata; no filename guessing,
 * no parent/BIOS substitution, no changes to user ZIP or native emulators. */
static int arcade_parent_lookup(const char *code,char *parent,size_t cap)
{
    FILE *f=fopen(ARCADE_NATIVE_PARENT_MAP_PATH,"rb");
    char line[192];
    if(parent && cap)parent[0]=0;
    if(!f || !code || !parent || cap<4){if(f)fclose(f);return 0;}
    while(fgets(line,sizeof line,f)){
        char *tab=strchr(line,'\t');
        if(!tab || line[0]=='#')continue;
        *tab++=0;
        tab[strcspn(tab,"\r\n")]=0;
        if(!strcmp(line,code)){
            size_t i,n=strlen(tab);
            if(n==0 || n>=cap || n>90 || !strcmp(tab,code))break;
            for(i=0;i<n;i++){
                unsigned char c=(unsigned char)tab[i];
                if(!isalnum(c)&&c!='_'&&c!='+'&&c!='-'&&c!='.')break;
            }
            if(i==n)snprintf(parent,cap,"%s",tab);
            break;
        }
    }
    fclose(f);
    return parent[0]!=0;
}
/* Strictly require the actual PS3 FBNeo driver and its clone's own exact
 * chip names/CRC matches in the original frozen DAT, not only a parent name. */
static int arcade_legacy_clone_allowed(const char *code,const char *parent)
{
    FILE *f=fopen(ARCADE_NATIVE_LEGACY_CLONES_PATH,"rb");
    char line[190];
    int ok=0;
    if(!f || !code || !parent){if(f)fclose(f);return 0;}
    while(fgets(line,sizeof line,f)){
        char *tab,*rest;
        if(line[0]=='#')continue;
        tab=strchr(line,'\t');if(!tab)continue;
        *tab++=0;
        if(strcmp(line,code))continue;
        rest=strchr(tab,'\t');if(!rest)break;
        *rest=0;
        ok=(strcmp(tab,parent)==0);
        break;
    }
    fclose(f);
    return ok;
}

#define ARC_MAX_ZIP 512
#define ARC_TAIL_SIZE 65557u

typedef struct {uint32_t crc,size;} ArcPart;
typedef struct {
    ArcPart a[ARC_MAX_ZIP];
    int n,complete,encrypted;
} ArcZip;

static uint16_t rd16(const unsigned char *p){
    return (uint16_t)((uint16_t)p[0] | ((uint16_t)p[1]<<8));
}
static uint32_t rd32(const unsigned char *p){
    return (uint32_t)p[0] | ((uint32_t)p[1]<<8) |
           ((uint32_t)p[2]<<16) | ((uint32_t)p[3]<<24);
}
static void set_reason(char *reason,size_t cap,const char *msg){
    if(!reason || !cap)return;
    if(!msg)msg="-";
    snprintf(reason,cap,"%s",msg);
}
static int zip_probe(const char *path,ArcZip *z)
{
    FILE *f=NULL;
    unsigned char *buf=NULL;
    uint32_t off,csize;
    long total_len,tail_len,start,i,at=-1;
    unsigned declared,j;
    int success=0;
    memset(z,0,sizeof(*z));
    if(!path || !(f=fopen(path,"rb")))return 0;
    if(fseek(f,0,SEEK_END)!=0 || (total_len=ftell(f))<22)goto done;
    tail_len=total_len<(long)ARC_TAIL_SIZE?total_len:(long)ARC_TAIL_SIZE;
    start=total_len-tail_len;
    buf=(unsigned char *)malloc((size_t)tail_len);
    if(!buf || fseek(f,start,SEEK_SET)!=0 ||
       fread(buf,1,(size_t)tail_len,f)!=(size_t)tail_len)goto done;
    for(i=tail_len-22;i>=0;i--)if(memcmp(buf+i,"PK\005\006",4)==0){
        at=i;break;
    }
    if(at<0)goto done;
    declared=rd16(buf+at+10);
    csize=rd32(buf+at+12);
    off=rd32(buf+at+16);
    if(declared==0 || declared>=0xffff || declared>ARC_MAX_ZIP ||
       off==0xffffffffu || csize==0xffffffffu ||
       (uint64_t)off+csize>(uint64_t)total_len)goto done;
    if(fseek(f,(long)off,SEEK_SET)!=0)goto done;
    for(j=0;j<declared;j++){
        unsigned char h[46];
        unsigned name_len,extra,comment,flags;
        if(fread(h,1,sizeof h,f)!=sizeof h ||
           memcmp(h,"PK\001\002",4)!=0)goto done;
        name_len=rd16(h+28);
        extra=rd16(h+30);
        comment=rd16(h+32);
        flags=rd16(h+8);
        if(flags & 1)z->encrypted++;
        if(fseek(f,(long)name_len+extra+comment,SEEK_CUR)!=0)goto done;
        z->a[z->n].crc=rd32(h+16);
        z->a[z->n].size=rd32(h+24);
        z->n++;
    }
    z->complete=1;
    success=1;
done:
    free(buf);
    if(f)fclose(f);
    if(!success)memset(z,0,sizeof(*z));
    return success;
}
static int contains(const ArcZip *z,uint32_t crc,uint32_t size)
{
    int i;
    if(!z->complete)return 0;
    for(i=0;i<z->n;i++)
        if(z->a[i].crc==crc && z->a[i].size==size)return 1;
    return 0;
}
static int old_neogeo_bios(const ArcZip *bios)
{
    return contains(bios,0x97cf998bu,131072u) &&
           contains(bios,0x354029fcu,131072u) &&
           contains(bios,0xe09e253cu,65536u);
}
static int fbneo_neogeo_bios(const ArcZip *bios)
{
    return contains(bios,0x94416d67u,131072u) &&
           contains(bios,0xc2ea0cfdu,131072u) &&
           contains(bios,0x5a86cff2u,131072u);
}
static int rom_code(const char *path,char *out,size_t max)
{
    const char *p;
    size_t i,n;
    if(!path || !out || max<4)return 0;
    if(strncmp(path,ARCADE_NATIVE_ROM_ROOT,sizeof(ARCADE_NATIVE_ROM_ROOT)-1))return 0;
    if(strstr(path,"/../") || strstr(path,"/./"))return 0;
    p=strrchr(path,'/');p=p?p+1:path;
    n=strlen(p);
    if(n<5 || strcasecmp(p+n-4,".zip"))return 0;
    n-=4;
    if(n>=max || n>90)return 0;
    for(i=0;i<n;i++){
        int c=(unsigned char)p[i];
        if(!isalnum(c) && c!='-' && c!='_' && c!='+' && c!='.')return 0;
        out[i]=(char)tolower(c);
    }
    out[n]=0;
    return 1;
}
static int mame0125_check(const char *gamecode,const ArcZip *game,
                         const ArcZip *parent,const ArcZip *bios,
                         int *total,int *good,int *from_parent)
{
    FILE *f=fopen(ARCADE_MAME125_CATALOG_PATH,"rb");
    char line[240];
    int header=0,count=0,matched=0;
    if(total)*total=0;
    if(good)*good=0;
    if(from_parent)*from_parent=0;
    if(!f)return 0;
    while(fgets(line,sizeof line,f)){
        char *code,*a,*b,*c,*e;
        unsigned long crc,sz;
        code=line;
        if(*code=='#')continue;
        a=strchr(code,'\t');if(!a)continue;
        *a++=0;
        if(strcmp(code,gamecode)<0)continue;
        if(strcmp(code,gamecode)>0){
            if(header)break;
            continue;
        }
        b=strchr(a,'\t');if(!b)continue;
        *b++=0;
        c=strchr(b,'\t');if(!c)continue;
        *c++=0;
        if(a[0]=='@'){header=1;continue;}
        if(!header || strlen(a)!=8)continue;
        crc=strtoul(a,&e,16);
        if(*e)continue;
        sz=strtoul(b,&e,10);
        if(*e || sz==0 || sz>64*1024*1024u)continue;
        count++;
        /* Clone ZIPs often contain just 1-3 modified ROMs. The original
         * MAME driver enumerates parent-shared chips as well. Validate all
         * requirements against the child, exact parent and BIOS ZIP union. */
        if(contains(game,(uint32_t)crc,(uint32_t)sz))matched++;
        else if(contains(parent,(uint32_t)crc,(uint32_t)sz)){
            matched++;
            if(from_parent)(*from_parent)++;
        }else if(contains(bios,(uint32_t)crc,(uint32_t)sz))matched++;
    }
    fclose(f);
    if(total)*total=count;
    if(good)*good=matched;
    /* Avoid routing to MAME just because a shortname happens to exist.
     * Every original MAME 0.125 driver chip must be present by CRC/size. */
    return header && count>=5 && matched==count;
}
int arcade_native_engine_select(const char *game_path,int original_engine,
                                char *reason,size_t reason_cap)
{
    ArcZip game={0},bios={0},parent={0};
    char code[96],parentcode[96]="",parentpath[256]="";
    int expected=0,found=0,from_parent=0,parent_found=0;
    int has_mame;
    if(original_engine==ARCADE_ENGINE_MAME125){
        set_reason(reason,reason_cap,"ORIGINAL_ROUTER_MAME_UNCHANGED");
        return original_engine;
    }
    if(!rom_code(game_path,code,sizeof code)){
        set_reason(reason,reason_cap,
                   game_path && strlen(game_path)>3 &&
                   !strcasecmp(game_path+strlen(game_path)-3,".7z")
                   ? "ARCADE_FORMATO_7Z_NO_ADMITIDO_EN_MOTORES_NATIVOS"
                   : "ARCADE_RUTA_O_EXTENSION_NO_ADMITIDA");
        return original_engine;
    }
    /* Only original MAME0.125 NeoGeo drivers trigger BIOS-specific routing.
     * All other arcade and console titles remain bit-for-bit unaffected. */
    if(!zip_probe(ARCADE_NATIVE_BIOS_PATH,&bios)){
        set_reason(reason,reason_cap,"NEOGEO_BIOS_NOT_OPENABLE_KEEP_ORIGINAL");
        return original_engine;
    }
    if(!old_neogeo_bios(&bios)){
        if(fbneo_neogeo_bios(&bios))
            set_reason(reason,reason_cap,"BIOS_MODERN_CORRECT_FOR_FBNEO");
        else
            set_reason(reason,reason_cap,"BIOS_NOT_OLD_MAME125_OR_MODERN_FBNEO");
        return original_engine;
    }
    if(!zip_probe(game_path,&game)){
        set_reason(reason,reason_cap,"ROM_ZIP_UNREADABLE_KEEP_ORIGINAL");
        return original_engine;
    }
    if(arcade_parent_lookup(code,parentcode,sizeof parentcode)){
        snprintf(parentpath,sizeof parentpath,ARCADE_NATIVE_ROM_ROOT "%s.zip",parentcode);
        parent_found=zip_probe(parentpath,&parent);
    }
    has_mame=mame0125_check(code,&game,&parent,&bios,
                            &expected,&found,&from_parent);
    if(has_mame){
        /* The exact PS3-version FBNeo and original MAME catalogs both
         * confirm all game chips. Use a SECOND core with exact old BIOS
         * and ROM filenames, never touch the original ROM, or normal FBNeo.
         * KOF2002 is one of these verified sets; 70+ other Neo Geo games
         * can now follow exactly the same previously working route. */
        if(legacy_allowlisted(code)){
            set_reason(reason,reason_cap,
                       "NEOGEO_OLD_BIOS_ALL_CHIPS_MATCH_SELECT_FBNEO_LEGACY");
            return ARCADE_ENGINE_FBNEO_LEGACY;
        }
        if(parentcode[0] && original_engine==ARCADE_ENGINE_FBNEO &&
           legacy_allowlisted(parentcode) &&
           arcade_legacy_clone_allowed(code,parentcode)){
            char msg[160];
            snprintf(msg,sizeof msg,
                     "NEOGEO_CLON_CRC_OK_PADRE=%s HEREDADOS=%d/%d FBNEO_LEGACY",
                     parentcode,from_parent,expected);
            set_reason(reason,reason_cap,msg);
            return ARCADE_ENGINE_FBNEO_LEGACY;
        }
        set_reason(reason,reason_cap,
                   "MAME0125_GAME_MATCH_BUT_NOT_VERIFIED_IN_PS3_FBNEO_LEGACY");
        return ARCADE_ENGINE_MAME125;
    }
    /* If the old BIOS matches but the game is absent from the original
     * MAME125 driver's ROMset, DO NOT guess that MAME can run it. */
    if(expected>0){
        char message[150];
        if(parentcode[0] && !parent_found)
            snprintf(message,sizeof message,
                     "NEOGEO_CLON_FALTA_PADRE=%s ESPERADOS=%d ENCONTRADOS=%d",
                     parentcode,expected,found);
        else if(parentcode[0])
            snprintf(message,sizeof message,
                     "NEOGEO_CLON_CRC_INCOMPLETO PADRE=%s %d_DE_%d",
                     parentcode,found,expected);
        else
            snprintf(message,sizeof message,
                     "NEOGEO_OLD_BIOS_MAME0125_CRC_MISMATCH %d_OF_%d",
                     found,expected);
        set_reason(reason,reason_cap,message);
        /* FBNeo also requires three modern BIOS CRCs that are not present.
         * Do not hand off to a core that will certainly reject this set. */
        return ARCADE_ENGINE_NONE;
    }
    set_reason(reason,reason_cap,
        original_engine==ARCADE_ENGINE_NONE
        ? "ROM_NO_INCLUIDA_EN_BASE_MOTORES_PS3"
        : "MAME0125_GAME_NOT_IN_ORIGINAL_DRIVER");
    /* A missing PS3 engine mapping is a real limitation of this older
     * core. Never claim new DAT games are supported without a driver. */
    return original_engine;
}
