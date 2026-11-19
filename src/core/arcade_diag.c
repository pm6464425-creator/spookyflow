/* SpookyFlow Arcade: one authoritative report for native MAME/FBNeo.
 * Compiled with the FRONTEND ONLY. No modifications to emulator cores,
 * PlayStation launchers, SNES, console saves, ROMs or cover art.
 *
 * The report is a pre-launch audit. The native PS3 emulator replaces
 * SpookyFlow via sysProcessExitSpawn2; its on-screen messages CANNOT be
 * captured by this process after replacement. This distinction is crucial
 * to avoid claiming that a successful handoff means a working ROM.
 */
#include "arcade_diag.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <ctype.h>
#include <strings.h>
#define Z_LIMIT 768
#define Z_NAMELEN 128
#define DIAG_MAX_MISMATCHES 38

typedef struct {
    char name[Z_NAMELEN];
    uint32_t crc,size;
    uint16_t flags,method;
} ZipMember;
typedef struct {
    ZipMember *entries;
    int count, total, bad_name;
    long file_bytes;
    int state;
} ZipArchive;

static uint16_t le16(const unsigned char *s)
{ return (uint16_t)(s[0] | ((unsigned)s[1]<<8)); }
static uint32_t le32(const unsigned char *s)
{ return (uint32_t)s[0] | ((uint32_t)s[1]<<8) |
          ((uint32_t)s[2]<<16) | ((uint32_t)s[3]<<24); }
static int file_size(const char *p,long *n)
{
    FILE *f;
    long len;
    if(n)*n=-1;
    if(!p || !(f=fopen(p,"rb")))return 0;
    if(fseek(f,0,SEEK_END)!=0){fclose(f);return 0;}
    len=ftell(f);
    fclose(f);
    if(n)*n=len;
    return len>=0;
}
static int zip_load(const char *path,ZipArchive *z)
{
    FILE *f;
    unsigned char *tail=NULL;
    long size, start, i, found=-1;
    uint32_t cd_offset,cd_size;
    unsigned total;
    int ok=0;
    memset(z,0,sizeof(*z));
    z->state=-1;
    if(!path || !(f=fopen(path,"rb")))return 0;
    if(fseek(f,0,SEEK_END)!=0 || (size=ftell(f))<22)goto out;
    z->file_bytes=size;
    {
        long cap=size<65557 ? size:65557;
        start=size-cap;
        tail=(unsigned char *)malloc((size_t)cap);
        if(!tail || fseek(f,start,SEEK_SET)!=0 ||
           fread(tail,1,(size_t)cap,f)!=(size_t)cap)goto out;
        for(i=cap-22;i>=0;i--) {
            if(memcmp(tail+i,"PK\005\006",4)==0) {found=i;break;}
        }
        if(found<0)goto out;
        total=le16(tail+found+10);
        cd_size=le32(tail+found+12);
        cd_offset=le32(tail+found+16);
    }
    if(!total || total==65535 || cd_offset==0xffffffffu ||
       cd_size==0xffffffffu ||
       (uint64_t)cd_offset+cd_size>(uint64_t)size)goto out;
    z->entries=(ZipMember *)calloc(Z_LIMIT,sizeof(ZipMember));
    if(!z->entries)goto out;
    if(fseek(f,(long)cd_offset,SEEK_SET)!=0)goto out;
    z->total=(int)total;
    for(unsigned j=0;j<total;j++){
        unsigned char hdr[46];
        unsigned namelen,extra,comment;
        ZipMember *m;
        if(fread(hdr,1,46,f)!=46 ||
           memcmp(hdr,"PK\001\002",4)!=0)goto out;
        namelen=le16(hdr+28);
        extra=le16(hdr+30);
        comment=le16(hdr+32);
        if(namelen==0 || namelen>=Z_NAMELEN){
            z->bad_name++;
            if(fseek(f,(long)namelen+extra+comment,SEEK_CUR)!=0)goto out;
            continue;
        }
        if(j>=Z_LIMIT){
            z->state=2;/* found but partial: never claim clean */
            if(fseek(f,(long)namelen+extra+comment,SEEK_CUR)!=0)goto out;
            continue;
        }
        m=&z->entries[z->count];
        if(fread(m->name,1,namelen,f)!=namelen)goto out;
        m->name[namelen]=0;
        m->crc=le32(hdr+16);
        m->size=le32(hdr+24);
        m->flags=le16(hdr+8);
        m->method=le16(hdr+10);
        if(fseek(f,(long)extra+comment,SEEK_CUR)!=0)goto out;
        z->count++;
    }
    z->state=z->state==2?2:1;
    ok=1;
out:
    free(tail);fclose(f);
    if(!ok){free(z->entries);memset(z,0,sizeof(*z));z->state=-1;}
    return ok;
}
static void zip_free(ZipArchive *z)
{free(z->entries);memset(z,0,sizeof(*z));}
static const ZipMember *zip_find(const ZipArchive *z,const char *name)
{
    int i;
    if(!z || !z->entries || !name || !*name)return NULL;
    for(i=0;i<z->count;i++)
        if(strcasecmp(z->entries[i].name,name)==0)return &z->entries[i];
    return NULL;
}
static int get_rom_code(const char *path,char *out,size_t cap)
{
    const char *b=path?strrchr(path,'/'):NULL;
    size_t n,i;
    b=b?b+1:path;
    if(!b || (n=strlen(b))<5 || strcasecmp(b+n-4,".zip")!=0)return 0;
    n-=4;
    if(n==0 || n>90 || n>=cap)return 0;
    for(i=0;i<n;i++) {
        unsigned char c=(unsigned char)b[i];
        if(!isalnum(c)&&c!='.'&&c!='-'&&c!='_'&&c!='+')return 0;
        out[i]=(char)tolower(c);
    }
    out[n]=0;
    return 1;
}
#ifndef SF_ARCADE_ROM_ROOT
#define SF_ARCADE_ROM_ROOT "/dev_hdd0/ARCADE"
#endif
static void companion_path(char *out,size_t cap,const char *code)
{
    if(code && *code)snprintf(out,cap,SF_ARCADE_ROM_ROOT "/%s.zip",code);
    else if(cap)out[0]=0;
}
static const char *state_text(int state)
{
    if(state==-1)return "NO_ABRE_ZIP_O_CENTRAL_DIRECTORY_DANADA";
    if(state==2)return "ZIP_TIENE_MAS_DE_768_ENTRADAS_REVISION_PARCIAL";
    return "ZIP_CENTRAL_DIRECTORY_OK";
}
static void probe_file(FILE *out,const char *tag,const char *path)
{
    long n=-1;int found=file_size(path,&n);
    fprintf(out,"%s_PATH=%s\n%s_EXISTE=%s\n%s_BYTES=%ld\n",
            tag,path?path:"(null)",tag,found?"SI":"NO",tag,n);
}
static void split_tab(char *line,char **a,int count)
{
    int i;
    for(i=0;i<count;i++){
        char *p;
        a[i]=line;
        p=strchr(line,'\t');
        if(!p) {
            for(i=i+1;i<count;i++)a[i]="";
            return;
        }
        *p=0;
        line=p+1;
    }
    a[count-1][strcspn(a[count-1],"\r\n")]=0;
}
static void inspect_zip(FILE *out,const char *tag,const char *path,ZipArchive *z)
{
    int i,encryption=0;
    zip_load(path,z);
    fprintf(out,"%s_ZIP=%s\n%s_ESTADO=%s\n%s_BYTES=%ld\n",
            tag,path,tag,state_text(z->state),tag,z->file_bytes);
    fprintf(out,"%s_ENTRADAS=%d/%d\n",tag,z->count,z->total);
    for(i=0;i<z->count;i++)if((z->entries[i].flags&1)!=0)encryption++;
    fprintf(out,"%s_ARCHIVOS_ENCRIPTADOS=%d\n",tag,encryption);
    if(z->state>0){
        fprintf(out,"%s_PRIMEROS_ARCHIVOS_INTERIORES:\n",tag);
        for(i=0;i<z->count && i<16;i++)
            fprintf(out,"  %02d name=%s crc=%08x bytes=%u method=%u\n",
                    i+1,z->entries[i].name,(unsigned)z->entries[i].crc,
                    (unsigned)z->entries[i].size,(unsigned)z->entries[i].method);
    }
}
static void compare_with_manifest(FILE *report,const char *romcode,
                                  const ZipArchive *game,const ZipArchive *parent,
                                  const ZipArchive *bios)
{
    FILE *f;
    char line[480],work[480],*fields[6];
    char expected_parent[96]="",expected_bios[96]="";
    unsigned expected=0,match=0,from_parent=0,from_bios=0;
    unsigned wrong_crc=0,missing=0,report_count=0,game_found=0,manifest_rows=0;
    if(!romcode[0]){fputs("RESULTADO=NO_ES_ZIP_O_CODIGO_INVALIDO\n",report);return;}
    f=fopen(SF_ARCADE_ROM_MANIFEST,"rb");
    if(!f){
        fprintf(report,"MANIFEST_ESTADO=NO_ENCONTRADO\nMANIFEST_PATH=%s\n",
                SF_ARCADE_ROM_MANIFEST);
        fputs("RESULTADO=NO_SE_PUEDE_COMPROBAR_VERSION_ROMSET\n",report);
        return;
    }
    while(fgets(line,sizeof line,f)){
        const ZipMember *z;
        uint32_t crc;
        uint32_t size;
        int valid;
        char *end;
        if(line[0]=='#')continue;
        memcpy(work,line,strlen(line)+1);
        split_tab(work,fields,6);
        valid=strcmp(fields[0],romcode);
        if(valid<0)continue;
        if(valid>0 && game_found)break;
        if(valid>0)continue;
        game_found=1;manifest_rows++;
        if(!expected_parent[0] && fields[1][0]) {
            snprintf(expected_parent,sizeof(expected_parent),"%s",fields[1]);
        }
        if(!expected_bios[0] && fields[2][0]) {
            snprintf(expected_bios,sizeof(expected_bios),"%s",fields[2]);
        }
        if(strcmp(fields[3],"@")==0)continue;
        if(strlen(fields[4])!=8 || !fields[3][0])continue;
        crc=(uint32_t)strtoul(fields[4],&end,16);
        if(*end)continue;
        size=(uint32_t)strtoul(fields[5],&end,10);
        if(end==fields[5] || (*end && *end!='\r' && *end!='\n'))continue;
        expected++;
        z=zip_find(game,fields[3]);
        if(z && z->crc==crc && z->size==size){match++;continue;}
        {
            const ZipMember *p=zip_find(parent,fields[3]);
            const ZipMember *b=zip_find(bios,fields[3]);
            if(p && p->crc==crc && p->size==size){from_parent++;continue;}
            if(b && b->crc==crc && b->size==size){from_bios++;continue;}
        }
        if(z){
            wrong_crc++;
            if(report_count++<DIAG_MAX_MISMATCHES)
                fprintf(report,"INCIDENCIA_CRC archivo=%s esperado_crc=%08x esperado_bytes=%u encontrado_crc=%08x encontrado_bytes=%u\n",
                        fields[3],(unsigned)crc,(unsigned)size,
                        (unsigned)z->crc,(unsigned)z->size);
        }else{
            missing++;
            if(report_count++<DIAG_MAX_MISMATCHES)
                fprintf(report,"INCIDENCIA_FALTANTE archivo=%s esperado_crc=%08x esperado_bytes=%u\n",
                        fields[3],(unsigned)crc,(unsigned)size);
        }
    }
    fclose(f);
    fprintf(report,"MANIFEST_VERSION=FBNeo_PS3_fccba95f2\n");
    fprintf(report,"MANIFEST_GAME_PRESENTE=%s\n",game_found?"SI":"NO");
    fprintf(report,"MANIFEST_PARENT=%s\nMANIFEST_ROMOF_BIOS=%s\n",
            expected_parent[0]?expected_parent:"(ninguno)",
            expected_bios[0]?expected_bios:"(ninguno)");
    fprintf(report,"MANIFEST_EXPECTED_ROMS=%u\nMANIFEST_CRC_OK_JUEGO=%u\n"
                   "MANIFEST_CRC_OK_PARENT=%u\nMANIFEST_CRC_OK_BIOS=%u\n"
                   "MANIFEST_CRC_INCORRECTOS=%u\nMANIFEST_ROMS_FALTANTES=%u\n"
                   "MANIFEST_LINES_SELECCIONADAS=%u\n",
            expected,match,from_parent,from_bios,wrong_crc,missing,manifest_rows);
    if(!game_found)fputs("RESULTADO=ROM_NO_APARECE_EN_DAT_FBNEO_PS3\n",report);
    else if(game->state<0)fputs("RESULTADO=ZIP_ILEGIBLE_O_CORRUPTO\n",report);
    else if(game->state==2 || (parent && parent->state==2) ||
            (bios && bios->state==2))
        fputs("RESULTADO=REVISION_INCOMPLETA_ZIP_MUY_GRANDE\n",report);
    else if(wrong_crc>0)
        fputs("RESULTADO=CRC_DISTINTO_ROMSET_INCOMPATIBLE_PARA_FBNEO_PS3\n",report);
    else if(missing>0)
        fputs("RESULTADO=ROMS_FALTANTES_O_DEPENDENCIA_SIN_RESOLVER\n",report);
    else if(expected>0)
        fputs("RESULTADO=CRC_COINCIDEN_NO_GARANTIZA_EJECUCION\n",report);
    else fputs("RESULTADO=SIN_ROM_LISTADOS_EN_MANIFEST\n",report);
}

/* V15.68: MAME 0.125 uses a different ROM catalog from FBNeo. Diagnose
 * each declared MAME chip by CRC+decompressed size (filename aliases are
 * accepted), and preserve one single report. No ZIP is modified or extracted.
 * Only the frontend can log: after sysProcessExitSpawn2, MAME runs alone. */
#ifndef SF_MAME125_CATALOG
#define SF_MAME125_CATALOG "/dev_hdd0/game/SPKF00001/USRDIR/ARCADE/mame0125_neogeo.tsv"
#endif
#define SF_MAME125_PATH "/dev_hdd0/game/SPKF00001/USRDIR/MAME125/"
static const ZipMember *zip_crc_size(const ZipArchive *z,uint32_t crc,uint32_t size)
{
    int i;
    if(!z || !z->entries || z->state<0)return NULL;
    for(i=0;i<z->count;i++)
        if(z->entries[i].crc==crc && z->entries[i].size==size)
            return z->entries+i;
    return NULL;
}
static void mame_resources(FILE *f,const char *rompath)
{
    FILE *ini;
    char line[256],isodir[216]="";
    int fullscreen=-1,aspect=-1,driver=-1,verbosity=-1,filelog=-1;
    const char *cfg="/dev_hdd0/game/SPKF00001/USRDIR/retroarch.cfg";
    probe_file(f,"MAME_INIT_INI",SF_MAME125_PATH "init.ini");
    probe_file(f,"MAME_CFG_DEFAULT",SF_MAME125_PATH "cfg/default.cfg");
    probe_file(f,"MAME_FONT",SF_MAME125_PATH "fonts/ui000-11.bdc");
    ini=fopen(SF_MAME125_PATH "init.ini","rb");
    if(ini){
        while(fgets(line,sizeof line,ini)){
            char *p=line,*eq;
            while(*p==' ' || *p=='\t')p++;
            if(strncmp(p,"isodir",6)!=0)continue;
            eq=strchr(p,'=');if(!eq)continue;
            p=eq+1;while(*p==' '||*p=='\t')p++;
            p[strcspn(p,"\r\n")]=0;
            snprintf(isodir,sizeof isodir,"%s",p);
            break;
        }
        fclose(ini);
    }
    fprintf(f,"MAME_INIT_ISODIR=%s\n",isodir[0]?isodir:"(no recuperado)");
    fprintf(f,"MAME_ROM_EN_ISODIR=%s\n",
            (rompath && isodir[0] && !strncmp(rompath,isodir,strlen(isodir)) &&
             rompath[strlen(isodir)]=='/')?"SI":"NO_O_NO_COMPROBADO");
    ini=fopen(cfg,"rb");
    if(ini){
        while(fgets(line,sizeof line,ini)){
            char *p=line;while(*p==' '||*p=='\t')p++;
            if(!strncmp(p,"video_fullscreen =",18))
                fullscreen=(strstr(p,"\"true\"")!=NULL);
            if(!strncmp(p,"video_aspect_ratio =",20))
                aspect=(strstr(p,"1.777800")!=NULL);
            if(!strncmp(p,"video_driver =",14))
                driver=(strstr(p,"\"gl\"")!=NULL);
            if(!strncmp(p,"log_verbosity =",15))
                verbosity=(strstr(p,"\"true\"")!=NULL);
            if(!strncmp(p,"log_to_file =",13))
                filelog=(strstr(p,"\"true\"")!=NULL);
        }
        fclose(ini);
    }
    fprintf(f,"RETROARCH_VIDEO_FULLSCREEN=%d\nRETROARCH_ASPECTO_16_9=%d\n"
              "RETROARCH_DRIVER_GL=%d\nRETROARCH_VERBOSE=%d\n"
              "RETROARCH_LOG_TO_FILE=%d\n",
            fullscreen,aspect,driver,verbosity,filelog);
    fputs("AVISO_CONFIG=RETROARCH_CFG_PUEDE_NO_SER_INTERPRETADO_POR_MAME125_STANDALONE\n",f);
}
static void mame125_manifest(FILE *report,const char *romcode,
                             const ZipArchive *game,const ZipArchive *bios)
{
    FILE *f=fopen(SF_MAME125_CATALOG,"rb");
    char line[360],*crc_text,*size_text,*file_text,*tab,*end;
    unsigned expected=0,ok_game=0,ok_bios=0,missing=0,nmissing=0;
    uint64_t bytes_total=0;
    const uint32_t needs_crc[3]={0x97cf998bu,0x354029fcu,0xe09e253cu};
    const uint32_t needs_size[3]={131072u,131072u,65536u};
    const char *needs_name[3]={"sm1.sm1","sfix.sfx","000-lo.lo"};
    unsigned bios_good=0,i;
    for(i=0;i<3;i++){
        const ZipMember *b=zip_crc_size(bios,needs_crc[i],needs_size[i]);
        if(b){bios_good++;fprintf(report,"MAME_BIOS_OK=%s encontrado=%s\n",needs_name[i],b->name);}
        else fprintf(report,"MAME_BIOS_NO_ENCONTRADA=%s crc=%08x bytes=%u\n",
                     needs_name[i],(unsigned)needs_crc[i],(unsigned)needs_size[i]);
    }
    fprintf(report,"MAME_BIOS_ANTIGUA_CLAVE=%u/3\n",bios_good);
    if(!f){
        fprintf(report,"MAME_DAT_ESTADO=NO_ENCONTRADO ruta=%s\n",SF_MAME125_CATALOG);
        fputs("MAME_ROMSET_ESTADO=SIN_CATALOGO_NO_SE_PUEDE_VERIFICAR\n",report);
        return;
    }
    while(fgets(line,sizeof line,f)){
        unsigned long crc,sz;
        tab=strchr(line,'\t');if(!tab)continue;
        *tab++=0;
        if(strcmp(line,romcode))continue;
        crc_text=tab;
        tab=strchr(crc_text,'\t');if(!tab)continue;
        *tab++=0;size_text=tab;
        tab=strchr(size_text,'\t');if(!tab)continue;
        *tab++=0;file_text=tab;
        file_text[strcspn(file_text,"\r\n")]=0;
        if(!strcmp(crc_text,"@"))continue;
        if(strlen(crc_text)!=8)continue;
        crc=strtoul(crc_text,&end,16);
        if(*end)continue;
        sz=strtoul(size_text,&end,10);
        if(*end || !sz || sz>64u*1024u*1024u)continue;
        expected++;bytes_total+=(uint64_t)sz;
        {
            const ZipMember *z=zip_crc_size(game,(uint32_t)crc,(uint32_t)sz);
            const ZipMember *b=zip_crc_size(bios,(uint32_t)crc,(uint32_t)sz);
            if(z)ok_game++;
            else if(b)ok_bios++;
            else{
                missing++;
                if(nmissing++<20)
                    fprintf(report,"MAME_ROM_AUSENTE=%s crc=%08lx bytes=%lu\n",
                            file_text,crc,sz);
            }
        }
    }
    fclose(f);
    fprintf(report,"MAME_DAT_VERSION=ORIGINAL_MAME_0_125\n"
              "MAME_ROM_CRC_ESPERADOS=%u\nMAME_ROM_CRC_JUEGO_OK=%u\n"
              "MAME_ROM_CRC_BIOS_OK=%u\nMAME_ROM_FALTANTES=%u\n"
              "MAME_BYTES_DESCOMPRIMIDOS_SUMADOS=%llu\n",
            expected,ok_game,ok_bios,missing,(unsigned long long)bytes_total);
    if(game->state!=1 || bios->state!=1)
        fputs("MAME_ROMSET_ESTADO=ZIP_NO_COMPLETO_O_ILEGIBLE\n",report);
    else if(!expected)
        fputs("MAME_ROMSET_ESTADO=ROM_NO_INCLUIDA_EN_MAME_0_125\n",report);
    else if(missing || bios_good<3)
        fputs("MAME_ROMSET_ESTADO=DEPENDENCIA_CRC_O_BIOS_FALTANTE\n",report);
    else
        fputs("MAME_ROMSET_ESTADO=METADATOS_CRC_COMPLETOS_NO_GARANTIZA_ARRANQUE\n",report);
    fputs("AVISO_ZIP=CRC_DE_CABECERA_NO_COMPRUEBA_DESCOMPRESION_INTEGRAL\n",report);
}

int arcade_diag_begin(const char *rom_path,int core,const char *core_file,
                      const char *reload_file,const char *config_file)
{
    FILE *f;
    char code[96]="",parent_code[96]="",bios_code[96]="";
    char parent_path[240]="",bios_path[240]="";
    ZipArchive game={0},parent={0},bios={0};
    int is_zip;
    f=fopen(SF_ARCADE_DIAG_PATH,"wb");/* always REPLACE prior report */
    if(!f)return -1;
    fputs("SPOOKYFLOW V15.69 - DIAGNOSTICO UNICO FBNEO LEGACY ARCADE / MAME / FBNEO\n"
          "SOLO ESTE TXT; reemplazado al pulsar X, nunca acumulado.\n"
          "SCOPE=ARCADE_ONLY, NO_MODIFICA_ROMS, NO_DESCARGA_BIOS\n"
          "NOTA=La PS3 sustituye SpookyFlow por el emulador. Este informe\n"
          "     analiza el ZIP ANTES del cambio, NO registra la salida del\n"
          "     motor despues del arranque. ERROR FBNeo Wrong romset puede\n"
          "     deberse a VERSION/CRC, BIOS, padre/clon o ficheros faltantes.\n"
          "------------------------------------------------------------\n",f);
    fprintf(f,"RUTA_ROM=%s\nMOTOR_ELEGIDO=%s\n",
            rom_path?rom_path:"(null)",
            core==1?"FBNeo":core==2?"MAME0125":core==3?"FBNeo_LEGACY":"NO_IDENTIFICADO");
    fprintf(f,"CARPETA_ROM=%s\n",SF_ARCADE_ROM_ROOT);
    probe_file(f,"EJECUTABLE",core_file);
    probe_file(f,"RELOAD",reload_file);
    probe_file(f,"CONFIG_VIDEO",config_file);
    is_zip=get_rom_code(rom_path,code,sizeof code);
    fprintf(f,"CODIGO_ROM=%s\nEXTENSION_ZIP_ADMITIDA=%s\n",
            is_zip?code:"(invalid)",is_zip?"SI":"NO");
    if(is_zip){
        /* First find exact parent/romof for this code; older DAT is sorted.
         * This first pass never opens or changes a game file. */
        FILE *manifest=fopen(SF_ARCADE_ROM_MANIFEST,"rb");
        if(manifest){
            char line[480],work[480],*fields[6];
            while(fgets(line,sizeof line,manifest)){
                if(line[0]=='#')continue;
                memcpy(work,line,strlen(line)+1);
                split_tab(work,fields,6);
                if(strcmp(fields[0],code)==0){
                    snprintf(parent_code,sizeof parent_code,"%s",fields[1]);
                    snprintf(bios_code,sizeof bios_code,"%s",fields[2]);
                    break;
                }
            }
            fclose(manifest);
        }
    }
    inspect_zip(f,"JUEGO",rom_path?rom_path:"", &game);
    if(parent_code[0] && strcmp(parent_code,code)!=0){
        companion_path(parent_path,sizeof parent_path,parent_code);
        inspect_zip(f,"PADRE",parent_path,&parent);
    }else fputs("PADRE_ZIP=(no requerido por metadatos DAT)\n",f);
    if(bios_code[0] && strcmp(bios_code,parent_code)!=0 &&
       strcmp(bios_code,code)!=0){
        companion_path(bios_path,sizeof bios_path,bios_code);
        inspect_zip(f,"BIOS",bios_path,&bios);
    }else fputs("BIOS_ZIP=(no listado por romof o igual al padre)\n",f);
    if(core==1){
        compare_with_manifest(f,code,&game,&parent,&bios);
    }else if(core==2 || core==3){
        fputs("MANIFEST=DAT_ORIGINAL_MAME_0125_COMPATIBILIDAD_CRC\n",f);
        if(core==2)mame_resources(f,rom_path);
        else {
            probe_file(f,"FBNEO_LEGACY", "/dev_hdd0/game/SPKF00001/USRDIR/cores/fb_neo_legacy.SELF");
            fputs("CORE_VARIANTE=FBNEO_LEGACY_BINARIO_AJUSTADO_SOLO_DESCRIPTORES_NEO_GEO\n"
                  "ROM_ZIP_ORIGINAL=NO_MODIFICADO\n"
                  "BIOS_ZIP_ORIGINAL=NO_MODIFICADO\n",f);
        }
        mame125_manifest(f,code,&game,&bios);
        fputs("FASE_POST_100_PORCIENTO=NO_OBSERVABLE_DESDE_SPOOKYFLOW\n"
              "MAME125_SELF_INSTRUMENTADO=NO\n"
              "NOTA=La ejecucion del motor posterior a sysProcessExitSpawn2\n"
              "     debe verificarse en PS3; este TXT analiza pre-lanzamiento.\n",f);
    }else {
        fputs("RESULTADO=SIN_MOTOR_COMPATIBLE_PARA_EL_CODIGO\n",f);
    }
    fputs("FASE=PRE_LANZAMIENTO_FINALIZADA; ZIP_NO_MODIFICADO\n",f);
    fflush(f);fclose(f);
    zip_free(&game);zip_free(&parent);zip_free(&bios);
    return 0;
}
void arcade_diag_event(const char *stage,int rc,const char *detail)
{
    FILE *f=fopen(SF_ARCADE_DIAG_PATH,"ab");
    if(!f)return;
    fprintf(f,"FASE=%s\nCODIGO_LANZAMIENTO=%d\nDETALLE=%.260s\n",
            stage?stage:"SIN_ETAPA",rc,detail?detail:"-");
    fflush(f);fclose(f);
}
