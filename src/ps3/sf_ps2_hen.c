/*
 * PS2 Classics preparation for PS3HEN — EXPERIMENTAL, physical-PS3 untested.
 *
 * Format reference: bucanero/apollo-ps3 source/ps2classic.c and
 * include/ps2_data.h, based on flatz PS2 Classics cryptography.
 * Original code GPLv3. This implementation GPL-3.0-or-later, retains
 * attribution and uses no private keys unique to any console.
 *
 * Unlike Apollo's original routine, source ISO is never modified:
 * LIMG sector is synthesized in the encrypted data stream. On failure
 * only SpookyFlow-owned temporary files are touched.
 *
 * HEN cannot use Cobra's 0x7028 direct PS2 ISO emulation. This prepares
 * ISO.BIN.ENC for an already installed, activated PS2 Classics Placeholder
 * (PS2U10000). The PS2 Classics app must still be launched from XMB.
 */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/stat.h>
#ifdef SF_PS2_HEN_HOST_TEST
#include <pthread.h>
typedef pthread_t sys_ppu_thread_t;
#define sysThreadExit(x) pthread_exit(NULL)
#else
#include <sys/thread.h>
#endif
#include "sf_ps2_hen.h"
#include "ps2_mount_plan.h"
#include "log.h"
#ifdef SF_HAVE_TLS
#include <mbedtls/aes.h>
#include <mbedtls/sha1.h>
#endif

#ifdef SF_PS2_HEN_HOST_TEST
#define SF_CLASSICS_DIR "build-host/ps2hen/placeholder/USRDIR"
#define SF_HEN_DIAG "build-host/ps2hen/ps2_hen_last.txt"
#else
#define SF_CLASSICS_DIR "/dev_hdd0/game/PS2U10000/USRDIR"
#define SF_HEN_DIAG "/dev_hdd0/tmp/spookyflow/ps2_hen_last.txt"
#endif
#define SF_CLASSICS_EBOOT SF_CLASSICS_DIR "/EBOOT.BIN"
#define SF_CLASSICS_IMAGE SF_CLASSICS_DIR "/ISO.BIN.ENC"
#define SF_CLASSICS_TMP SF_CLASSICS_DIR "/ISO.BIN.ENC.sf-part"
#define SF_CLASSICS_USER_BACKUP SF_CLASSICS_DIR "/ISO.BIN.ENC.sf-original"
#define SF_CLASSICS_OLD SF_CLASSICS_DIR "/ISO.BIN.ENC.sf-old"
#define SF_CLASSICS_META SF_CLASSICS_DIR "/ISO.BIN.ENC.sf-source"
#define SF_CLASSICS_META_TMP SF_CLASSICS_DIR "/ISO.BIN.ENC.sf-source-part"
#define SF_CFG_TARGET SF_CLASSICS_DIR "/CONFIG"
#define SF_CFG_TMP SF_CLASSICS_DIR "/CONFIG.sf-part"
#define SF_CFG_OWNER SF_CLASSICS_DIR "/CONFIG.sf-source"
#define SF_CFG_OWNER_TMP SF_CLASSICS_DIR "/CONFIG.sf-source-part"
#define SF_CFG_OLD SF_CLASSICS_DIR "/CONFIG.sf-old"
#define SF_CFG_ORIGINAL SF_CLASSICS_DIR "/CONFIG.sf-original"
#ifdef SF_PS2_HEN_HOST_TEST
#define SF_CFG_PACKAGED "pkgfiles/USRDIR/ps2configs/SLUS_201.74.ENC"
#define SF_CFG_INSTALLED "build-host/ps2hen/PS2CONFIG/CONFIG/ENC/"
#else
#define SF_CFG_PACKAGED "/dev_hdd0/game/SPKF00001/USRDIR/ps2configs/SLUS_201.74.ENC"
#define SF_CFG_INSTALLED "/dev_hdd0/game/PS2CONFIG/USRDIR/CONFIG/ENC/"
#endif

#define SEG 0x4000u
#define BLOCKS_PER_META 512u
#define CID "2P0001-PS2U10000_00-0000111122223333"
#define MAX_INPUT (10ULL*1024ULL*1024ULL*1024ULL)
typedef struct {
    char path[SF_PS2_MAX_PATH];
    char gameid[16];     /* Read-only per-game ID, used only for encrypted CONFIG. */
    volatile int state; /* 0 idle, 2 working, 1 ready, negative error */
    volatile int progress;
    volatile int cancel;
} SfHenJob;
static SfHenJob s_job;

static int exists_file(const char *name)
{
    struct stat st;
    return name && stat(name,&st)==0 && S_ISREG(st.st_mode);
}
static int file_size(const char *name,uint64_t *out)
{
    struct stat st;
    if (!name || stat(name,&st)!=0 || !S_ISREG(st.st_mode) || st.st_size<0) return -1;
    if (out)*out=(uint64_t)st.st_size;
    return 0;
}
static int ends(const char *a,const char *b)
{
    size_t n=strlen(a),m=strlen(b);
    return n>=m && strcasecmp(a+n-m,b)==0;
}
/* HEN may stream an ISO from a FAT32 USB drive directly to the encrypted
 * placeholder on the HDD. Unlike the CFW Cobra path, it does not mount from
 * USB, and never modifies the source file. Reject traversal and NTFS proxies. */
static int sf_hen_safe_iso_path(const char *path)
{
    const char *tail;
    size_t n;
    if(!path || !(n=strlen(path)) || n<21 || n>960)return 0;
    if(!strncmp(path,"/dev_hdd0/",10)) tail=path+10;
    else if(!strncmp(path,"/dev_usb00",10) &&
            path[10]>='0' && path[10]<='7' && path[11]=='/') tail=path+12;
    else return 0;
    if(strncmp(tail,"PS2ISO/",7)!=0 || !tail[7])return 0;
    if(!(ends(path,".iso") || ends(path,".iso.0")))return 0;
    if(strstr(path,"/../")||strstr(path,"/./")||strstr(path,"//")||
       strstr(path,"\\")||ends(path,"/..")||ends(path,"/."))return 0;
    for(;*path;path++)
        if((unsigned char)*path<32 || (unsigned char)*path==127)return 0;
    return 1;
}
static void put32(uint8_t *p,uint32_t v)
{
    p[0]=(uint8_t)(v>>24);p[1]=(uint8_t)(v>>16);
    p[2]=(uint8_t)(v>>8);p[3]=(uint8_t)v;
}
static void put64(uint8_t *p,uint64_t v)
{
    put32(p,(uint32_t)(v>>32));put32(p+4,(uint32_t)v);
}
static uint64_t get64(const uint8_t *p)
{
    return ((uint64_t)p[0]<<56)|((uint64_t)p[1]<<48)|
           ((uint64_t)p[2]<<40)|((uint64_t)p[3]<<32)|
           ((uint64_t)p[4]<<24)|((uint64_t)p[5]<<16)|
           ((uint64_t)p[6]<<8)|(uint64_t)p[7];
}
static void record(const char *stage,const char *path,int code,int progress)
{
    FILE *f=fopen(SF_HEN_DIAG,"wb");
    if (!f) return;
    fprintf(f,"BACKEND=PS2_CLASSICS_HEN\nSTAGE=%s\nRC=%d\nPROGRESS=%d\nPATH=%s\n",
            stage,code,progress,path?path:"(null)");
    (void)fflush(f);(void)fclose(f);
}
#ifdef SF_HAVE_TLS
/* PS2 Classics universal-placeholder constants, from Apollo/flatZ (GPLv3). */
static const uint8_t klic[16]={0xe4,0xe5,0x4f,0xd6,0x7c,0x16,0xc3,0x16,0xf4,0x78,0x29,0xa3,0x04,0x84,0xd8,0x43};
static const uint8_t key_data[16]={0x10,0x17,0x82,0x34,0x63,0xf4,0x68,0xc1,0xaa,0x41,0xd7,0x00,0xb1,0x40,0xf2,0x57};
static const uint8_t key_meta[16]={0x38,0x9d,0xcb,0xa5,0x20,0x3c,0x81,0x59,0xec,0xf9,0x4c,0x93,0x93,0x16,0x4c,0xc9};
static const uint8_t npd_omac2[16]={0x6b,0xa5,0x29,0x76,0xef,0xda,0x16,0xef,0x3c,0x33,0x9f,0xb2,0x97,0x1e,0x25,0x6b};
static const uint8_t npd_omac3[16]={0x9b,0x51,0x5f,0xea,0xcf,0x75,0x06,0x49,0x81,0xaa,0x60,0x4d,0x91,0xa5,0x4e,0x97};
static const uint8_t npd_kek[16]={0x72,0xf9,0x90,0x78,0x8f,0x9c,0xff,0x74,0x57,0x25,0xf0,0x8e,0x4c,0x12,0x83,0x87};
static void mul_x(uint8_t x[16])
{
    int i;uint8_t hi=(x[0]&0x80)?0x87:0;
    for(i=0;i<15;i++)x[i]=(uint8_t)((x[i]<<1)|(x[i+1]>>7));
    x[15]=(uint8_t)((x[15]<<1)^hi);
}
static int aes_cbc_16(const uint8_t key[16],const uint8_t in[16],uint8_t out[16])
{
    mbedtls_aes_context ctx; uint8_t iv[16]={0};int rc;
    mbedtls_aes_init(&ctx);
    rc=mbedtls_aes_setkey_enc(&ctx,key,128);
    if(!rc)rc=mbedtls_aes_crypt_cbc(&ctx,MBEDTLS_AES_ENCRYPT,16,iv,in,out);
    mbedtls_aes_free(&ctx);return rc;
}
static int omac(const uint8_t key[16],const uint8_t *input,size_t length,uint8_t out[16])
{
    mbedtls_aes_context aes;uint8_t L[16]={0},K[16],x[16]={0},b[16];
    size_t n=(length+15)/16, j,i;int full,rc=0;
    if(!n)n=1;
    full=length>0 && length%16==0;
    mbedtls_aes_init(&aes);
    if(mbedtls_aes_setkey_enc(&aes,key,128)!=0){mbedtls_aes_free(&aes);return -1;}
    rc=mbedtls_aes_crypt_ecb(&aes,MBEDTLS_AES_ENCRYPT,L,L);
    if(rc)goto done;
    memcpy(K,L,16);mul_x(K);if(!full)mul_x(K);
    for(i=0;i<n-1;i++){
        for(j=0;j<16;j++)b[j]=x[j]^input[i*16+j];
        rc=mbedtls_aes_crypt_ecb(&aes,MBEDTLS_AES_ENCRYPT,b,x);
        if(rc)goto done;
    }
    memset(b,0,16);
    {size_t offset=(n-1)*16,rem=length-offset;
     if(rem)memcpy(b,input+offset,rem);
     if(!full)b[rem]=0x80;}
    for(j=0;j<16;j++)b[j]^=x[j]^K[j];
    rc=mbedtls_aes_crypt_ecb(&aes,MBEDTLS_AES_ENCRYPT,b,out);
done:
    mbedtls_aes_free(&aes);return rc;
}
static int build_header(uint8_t h[SEG],uint64_t vsize)
{
    uint8_t key[16],buf[0x30+16];size_t i;
    memset(h,0,SEG);
    memcpy(h,"PS2\0",4);put32(h+4,0x00010001u);
    put32(h+8,2);put32(h+12,1);
    memcpy(h+0x10,CID,36);
    memcpy(h+0x40,"spookyflow",10);
    put32(h+0x84,SEG);put64(h+0x88,vsize);
    for(i=0;i<16;i++)key[i]=npd_kek[i]^npd_omac2[i];
    memcpy(buf,h+0x10,0x30);memcpy(buf+0x30,"ISO.BIN.ENC",11);
    if(omac(npd_omac3,buf,0x30+11,h+0x50))return -1;
    if(omac(key,h,0x60,h+0x60))return -1;
    return 0;
}
static int read_bytes(FILE *f,uint64_t offset,void *buf,size_t len)
{
    if(fseeko(f,(off_t)offset,SEEK_SET)!=0)return -1;
    return fread(buf,1,len,f)==len?0:-1;
}
static int write_bytes(FILE *f,const void *buf,size_t len)
{
    return fwrite(buf,1,len,f)==len?0:-1;
}
static int validate_iso(FILE *in,uint64_t size,uint8_t limg[SEG],uint64_t *virtual_size)
{
    uint8_t probe[8],sectors[4];int dvd;
    uint64_t padded;
    if(size<0x20000u || size>MAX_INPUT)return -20;
    if(!read_bytes(in,0x8000,probe,6) && probe[0]==1 && !memcmp(probe+1,"CD001",5)) dvd=1;
    else if(!read_bytes(in,0x9318,probe,6) && probe[0]==1 && !memcmp(probe+1,"CD001",5)) dvd=0;
    else return -21;
    if(size>=SEG && !read_bytes(in,size-SEG,probe,4) && !memcmp(probe,"LIMG",4)){
        *virtual_size=size;
        memset(limg,0,SEG);
        return 1; /* already has LIMG */
    }
    padded=(size+(SEG-1))&~((uint64_t)SEG-1u);
    if(padded>MAX_INPUT-SEG)return -22;
    memset(limg,0,SEG);
    if(read_bytes(in,(dvd?0x8000u:0x9318u)+0x54u,sectors,4))return -23;
    memcpy(limg,"LIMG",4);
    limg[7]=dvd?1:2;
    memcpy(limg+8,sectors,4);
    limg[14]=dvd?0x08:0x09;
    limg[15]=dvd?0x00:0x30;
    *virtual_size=padded+SEG;
    return 0;
}
static uint64_t expected_size(uint64_t data_size)
{
    uint64_t groups=(data_size+(uint64_t)SEG*BLOCKS_PER_META-1)/((uint64_t)SEG*BLOCKS_PER_META);
    return SEG+data_size+groups*SEG;
}
/* A file can be replaced without changing its name and size (modded PS2
 * games). Reject a stale cached conversion using SHA1 of both ends. */
static int source_fingerprint(const char *source,uint64_t size,char hex[41])
{
    unsigned char raw[2*SEG],digest[20];
    static const char alphabet[]="0123456789abcdef";
    FILE *f;int i,rc=0;
    if(size<SEG)return -1;
    f=fopen(source,"rb");if(!f)return -1;
    if(read_bytes(f,0,raw,SEG)!=0 ||
       read_bytes(f,size-SEG,raw+SEG,SEG)!=0 ||
       mbedtls_sha1(raw,sizeof(raw),digest)!=0)rc=-1;
    if(fclose(f)!=0)rc=-1;
    if(rc!=0)return -1;
    for(i=0;i<20;i++){hex[2*i]=alphabet[digest[i]>>4];
                        hex[2*i+1]=alphabet[digest[i]&15];}
    hex[40]=0;return 0;
}
static int ready_manifest(const char *source,uint64_t isosize)
{
    uint64_t installed=0,expected=0;
    FILE *f;char line[1200],wanted[1200],fingerprint[41]; uint8_t h[0x90];
    if(!exists_file(SF_CLASSICS_IMAGE) || !exists_file(SF_CLASSICS_META))return 0;
    if(file_size(SF_CLASSICS_IMAGE,&installed)!=0 || installed<3*SEG)return 0;
    f=fopen(SF_CLASSICS_META,"rb");if(!f)return 0;
    if(!fgets(line,sizeof(line),f)){fclose(f);return 0;}
    fclose(f);
    if(source_fingerprint(source,isosize,fingerprint)!=0)return 0;
    if(snprintf(wanted,sizeof(wanted),"SF-PS2-CLASSICS2 %llu %s %s\n",
       (unsigned long long)isosize,fingerprint,source)>=(int)sizeof(wanted))return 0;
    if(strcmp(line,wanted))return 0;
    f=fopen(SF_CLASSICS_IMAGE,"rb");if(!f)return 0;
    if(fread(h,1,sizeof(h),f)!=sizeof(h)){fclose(f);return 0;}
    fclose(f);
    expected=expected_size(get64(h+0x88));
    return !memcmp(h,"PS2\0",4) && installed==expected;
}
/* Only an already-encrypted CONFIG for the exact disc ID is installed.
 * Never reuse raw GX/SOFT configuration bytes as encrypted PS2 Classics.
 * PS2CONFIG ENC is authoritative; the bundled Rumble Racing patch is fallback.
 */
static int ps2_valid_gameid(const char *id)
{
    int i;
    if(!id || strlen(id)!=11 || id[4]!='_' || id[8]!='.')return 0;
    for(i=0;i<4;i++)if(id[i]<'A'||id[i]>'Z')return 0;
    for(i=5;i<11;i++)if(i!=8 && (id[i]<'0'||id[i]>'9'))return 0;
    return 1;
}
static int ps2_config_source(const char *id,char out[SF_PS2_MAX_PATH])
{
    if(!ps2_valid_gameid(id))return 0;
    if(snprintf(out,SF_PS2_MAX_PATH,"%s%s.ENC",SF_CFG_INSTALLED,id)>=SF_PS2_MAX_PATH)return 0;
    if(exists_file(out))return 1;
    if(!strcmp(id,"SLUS_201.74") && exists_file(SF_CFG_PACKAGED)){
        strcpy(out,SF_CFG_PACKAGED);
        return 1;
    }
    out[0]=0;
    return 0;
}
static int verify_cfg(const char *path)
{
    unsigned char head[0x90];uint64_t size=0,vsize;FILE *f;
    if(file_size(path,&size)!=0 || size<3*SEG || size>2*1024*1024)return 0;
    f=fopen(path,"rb");if(!f)return 0;
    if(fread(head,1,sizeof(head),f)!=sizeof(head)){fclose(f);return 0;}
    fclose(f);
    vsize=get64(head+0x88);
    return !memcmp(head,"PS2\0",4) && !memcmp(head+0x10,CID,36) &&
           vsize>0 && vsize<=0x20000 && size==expected_size(vsize);
}
static int ps2_stage_config(const char *id)
{
    char src[SF_PS2_MAX_PATH]={0};
    char marker[SF_PS2_MAX_PATH+64];
    const char *backup;
    FILE *in=NULL,*out=NULL,*f;
    unsigned char buf[8192];
    size_t n;
    int managed=exists_file(SF_CFG_OWNER),has_source=ps2_config_source(id,src),had_old;
    if(exists_file(SF_CFG_OLD))return -81;
    if(!has_source) {
        /* No matching PS2 Classics patch: do not carry an old managed
         * CONFIG into an unrelated game. Preserve third-party originals. */
        if(managed && exists_file(SF_CFG_TARGET)){
            if(rename(SF_CFG_TARGET,SF_CFG_OLD)!=0)return -82;
            if(remove(SF_CFG_OLD)!=0) return -83;
        }
        if(managed)(void)remove(SF_CFG_OWNER);
        return 0;
    }
    if(!verify_cfg(src))return -84; /* Never install an invalid .ENC */
    if(snprintf(marker,sizeof(marker),"SF-PS2-CONFIG %s %s\n",id,src)>=(int)sizeof(marker))
        return -85;
    f=fopen(SF_CFG_OWNER,"rb");
    if(f) {
        char line[SF_PS2_MAX_PATH+64];
        int same=fgets(line,sizeof(line),f) && strcmp(line,marker)==0;
        fclose(f);
        if(same && verify_cfg(SF_CFG_TARGET))return 0;
    }
    /* Stage and close a complete encrypted CONFIG before any replacement. */
    in=fopen(src,"rb");if(!in)return -86;
    out=fopen(SF_CFG_TMP,"wb");if(!out){fclose(in);return -87;}
    while((n=fread(buf,1,sizeof(buf),in))>0){
        if(fwrite(buf,1,n,out)!=n)break;
    }
    {int ok=feof(in)&&!ferror(out) && fflush(out)==0;
     if(fclose(in)!=0)ok=0;
     if(fclose(out)!=0)ok=0;
     in=NULL;out=NULL;
     if(!ok || !verify_cfg(SF_CFG_TMP)){(void)remove(SF_CFG_TMP);return -88;}
    }
    f=fopen(SF_CFG_OWNER_TMP,"wb");
    if(!f){(void)remove(SF_CFG_TMP);return -89;}
    {int ok=fputs(marker,f)!=EOF && fflush(f)==0;
     if(fclose(f)!=0)ok=0;
     if(!ok){(void)remove(SF_CFG_OWNER_TMP);(void)remove(SF_CFG_TMP);return -89;}}
    had_old=exists_file(SF_CFG_TARGET);
    backup=managed?SF_CFG_OLD:SF_CFG_ORIGINAL;
    if(had_old && exists_file(backup)){(void)remove(SF_CFG_TMP);(void)remove(SF_CFG_OWNER_TMP);return -90;}
    if(had_old && rename(SF_CFG_TARGET,backup)!=0){(void)remove(SF_CFG_TMP);(void)remove(SF_CFG_OWNER_TMP);return -91;}
    if(rename(SF_CFG_TMP,SF_CFG_TARGET)!=0){
        if(had_old)(void)rename(backup,SF_CFG_TARGET);
        (void)remove(SF_CFG_OWNER_TMP);
        return -92;
    }
    if(rename(SF_CFG_OWNER_TMP,SF_CFG_OWNER)!=0){
        (void)remove(SF_CFG_TARGET);
        if(had_old)(void)rename(backup,SF_CFG_TARGET);
        return -93;
    }
    if(had_old && managed)(void)remove(SF_CFG_OLD);
    log_printf("ps2 hen: encrypted CONFIG %s installed for %s",src,id);
    return 0;
}
static int write_manifest(const char *source,uint64_t isosize)
{
    FILE *f=fopen(SF_CLASSICS_META_TMP,"wb");int rc=0;
    if(!f)return -1;
    {char fingerprint[41];
     if(source_fingerprint(source,isosize,fingerprint)!=0 ||
        fprintf(f,"SF-PS2-CLASSICS2 %llu %s %s\n",
                (unsigned long long)isosize,fingerprint,source)<0)rc=-1;}
    if(fflush(f)!=0)rc=-1;
    if(fclose(f)!=0)rc=-1;
    return rc;
}
static int finish_atomic(const char *source,uint64_t isosize)
{
    const char *backup;
    int had_old=exists_file(SF_CLASSICS_IMAGE);
    uint64_t size=0;
    FILE *f;uint8_t header[0x90];
    if(file_size(SF_CLASSICS_TMP,&size)!=0 || size<3*SEG)return -35;
    f=fopen(SF_CLASSICS_TMP,"rb");if(!f)return -35;
    if(fread(header,1,sizeof(header),f)!=sizeof(header)){fclose(f);return -35;}
    fclose(f);
    if(memcmp(header,"PS2\0",4) || size!=expected_size(get64(header+0x88)))return -35;
    if(write_manifest(source,isosize)!=0)return -36;
    backup=exists_file(SF_CLASSICS_META)?SF_CLASSICS_OLD:SF_CLASSICS_USER_BACKUP;
    if(had_old){
        if(exists_file(backup))return -37; /* protect existing backup */
        if(rename(SF_CLASSICS_IMAGE,backup)!=0)return -38;
    }
    if(rename(SF_CLASSICS_TMP,SF_CLASSICS_IMAGE)!=0){
        if(had_old)(void)rename(backup,SF_CLASSICS_IMAGE);
        return -39;
    }
    /* When replacing an image managed by SpookyFlow, keep a rollback until
     * both the encrypted image and its associated metadata are committed. */
    if(rename(SF_CLASSICS_META_TMP,SF_CLASSICS_META)!=0){
        (void)remove(SF_CLASSICS_IMAGE);
        if(had_old)(void)rename(backup,SF_CLASSICS_IMAGE);
        return -40;
    }
    if(had_old && strcmp(backup,SF_CLASSICS_OLD)==0)
        (void)remove(SF_CLASSICS_OLD);
    return 0; /* leave external user's original encrypted file backed up */
}
static int perform_conversion(const char *path)
{
    FILE *in=NULL,*out=NULL;
    mbedtls_aes_context aes_data,aes_meta;
    uint8_t *blk=NULL,*meta=NULL,*header=NULL,*limg=NULL;
    uint8_t iv[16],derived_data[16],derived_meta[16];
    uint64_t size=0,vsize=0,blocks,i,j,pos=0;
    int rc=-30,had_limg=0;
    mbedtls_aes_init(&aes_data);mbedtls_aes_init(&aes_meta);
    if(file_size(path,&size)!=0){rc=-20;goto cleanup;}
    if(ready_manifest(path,size)){rc=ps2_stage_config(s_job.gameid);goto cleanup;}
    in=fopen(path,"rb");if(!in){rc=-20;goto cleanup;}
    blk=malloc(SEG);meta=malloc(SEG);header=malloc(SEG);limg=malloc(SEG);
    if(!blk || !meta || !header || !limg){rc=-31;goto cleanup;}
    had_limg=validate_iso(in,size,limg,&vsize);
    if(had_limg<0){rc=had_limg;goto cleanup;}
    if(vsize%SEG){rc=-22;goto cleanup;}
    if(build_header(header,vsize)){rc=-32;goto cleanup;}
    memset(iv,0,sizeof(iv));
    if(aes_cbc_16(key_data,klic,derived_data) ||
       aes_cbc_16(key_meta,klic,derived_meta)){rc=-32;goto cleanup;}
    if(mbedtls_aes_setkey_enc(&aes_data,derived_data,128)!=0 ||
       mbedtls_aes_setkey_enc(&aes_meta,derived_meta,128)!=0){rc=-32;goto cleanup;}
    out=fopen(SF_CLASSICS_TMP,"wb+");
    if(!out){rc=-33;goto cleanup;}
    if(write_bytes(out,header,SEG)){rc=-34;goto cleanup;}
    if(fseeko(in,0,SEEK_SET)!=0){rc=-20;goto cleanup;}
    blocks=vsize/SEG;
    for(i=0;i<blocks;){
        off_t meta_offset;uint64_t group_end;
        if(s_job.cancel){rc=-70;goto cleanup;}
        memset(meta,0,SEG);
        meta_offset=ftello(out);
        if(meta_offset<0){rc=-34;goto cleanup;}
        if(write_bytes(out,meta,SEG)){rc=-34;goto cleanup;}
        group_end=i+BLOCKS_PER_META;if(group_end>blocks)group_end=blocks;
        for(j=0;i<group_end;j++,i++,pos+=SEG){
            size_t readn=0;
            if(s_job.cancel){rc=-70;goto cleanup;}
            memset(blk,0,SEG);
            if(pos<size){
                uint64_t remaining=size-pos;
                readn=(size_t)(remaining<SEG?remaining:SEG);
                if(fread(blk,1,readn,in)!=readn){rc=-20;goto cleanup;}
            }else if(!had_limg && pos==vsize-SEG){
                memcpy(blk,limg,SEG);
            }
            memset(iv,0,16);
            if(mbedtls_aes_crypt_cbc(&aes_data,MBEDTLS_AES_ENCRYPT,SEG,iv,blk,blk)!=0){rc=-32;goto cleanup;}
            if(mbedtls_sha1(blk,SEG,meta+j*0x20)!=0){rc=-32;goto cleanup;}
            put32(meta+j*0x20+0x14,(uint32_t)i);
            if(write_bytes(out,blk,SEG)){rc=-34;goto cleanup;}
            s_job.progress=(int)((i+1)*100/blocks);
        }
        memset(iv,0,16);
        if(mbedtls_aes_crypt_cbc(&aes_meta,MBEDTLS_AES_ENCRYPT,SEG,iv,meta,meta)!=0){rc=-32;goto cleanup;}
        {off_t end=ftello(out);if(end<0 || fseeko(out,meta_offset,SEEK_SET)!=0 ||
         write_bytes(out,meta,SEG) || fseeko(out,end,SEEK_SET)!=0){rc=-34;goto cleanup;}}
        if(fflush(out)!=0){rc=-34;goto cleanup;}
        record("CIFRANDO",path,0,s_job.progress);
    }
    if(fflush(out)!=0){rc=-34;goto cleanup;}
    if(fclose(out)!=0){out=NULL;rc=-34;goto cleanup;}
    out=NULL;
    /* Source integrity: reject media changed during the conversion. */
    {uint64_t size_after=0;
      if(file_size(path,&size_after)!=0 || size_after!=size){rc=-45;goto cleanup;}}
    rc=finish_atomic(path,size);
    if(rc==0)rc=ps2_stage_config(s_job.gameid);
cleanup:
    if(out)fclose(out);
    if(in)fclose(in);
    if(blk)free(blk);if(meta)free(meta);if(header)free(header);if(limg)free(limg);
    mbedtls_aes_free(&aes_data);mbedtls_aes_free(&aes_meta);
    if(rc!=0){(void)remove(SF_CLASSICS_TMP);(void)remove(SF_CLASSICS_META_TMP);}
    return rc;
}
static void ps2_thread(void *arg)
{
    int rc; (void)arg;
    record("PREPARANDO",s_job.path,0,0);
    rc=perform_conversion(s_job.path);
    record(rc==0?"LISTO":"ERROR",s_job.path,rc,s_job.progress);
    __sync_synchronize();
    s_job.state=rc?rc:1;
    sysThreadExit(0);
}
#ifdef SF_PS2_HEN_HOST_TEST
static void *ps2_host_entry(void *arg)
{
    ps2_thread(arg);
    return NULL;
}
#endif
#endif /* SF_HAVE_TLS */
int sf_ps2_hen_start(const char *path,const char *gameid)
{
    uint64_t s=0;
    int n;
#ifdef SF_HAVE_TLS
    sys_ppu_thread_t tid;
#endif
    if(s_job.state==2)return -60;
#ifdef SF_PS2_HEN_HOST_TEST
    if(!path || strncmp(path,"build-host/ps2hen/PS2ISO/",
                        sizeof("build-host/ps2hen/PS2ISO/")-1)!=0)return -61;
#else
    if(!sf_hen_safe_iso_path(path))return -61;
#endif
    /* Split files are deliberately rejected until streaming multi-part
     * decryption is verified; do not silently encrypt only disc segment 0. */
    if(ends(path,".iso.0"))return -62;
    if(file_size(path,&s)!=0 || s<0x20000 || s>MAX_INPUT)return -63;
    if(!exists_file(SF_CLASSICS_EBOOT))return -64;
#ifndef SF_HAVE_TLS
    (void)n;return -65;
#else
    if(ready_manifest(path,s)) {
        int cfg=ps2_stage_config(gameid);
        return cfg<0?cfg:1;
    }
    if(strlen(path)>=sizeof(s_job.path))return -61;
    if(exists_file(SF_CLASSICS_OLD))return -66;
    strcpy(s_job.path,path);
    s_job.gameid[0]=0;
    if(ps2_valid_gameid(gameid))strcpy(s_job.gameid,gameid);
    s_job.progress=0;s_job.cancel=0;s_job.state=2;
    __sync_synchronize();
#ifdef SF_PS2_HEN_HOST_TEST
    n=pthread_create(&tid,NULL,ps2_host_entry,NULL);
    if(n==0)pthread_detach(tid);
#else
    n=sysThreadCreate(&tid,ps2_thread,NULL,1600,128u*1024u,0,(char*)"sf_ps2_hen");
#endif
    if(n!=0){s_job.state=-67;return -67;}
    return 0;
#endif
}
int sf_ps2_hen_poll(int *percent)
{
    __sync_synchronize();
    if(percent)*percent=s_job.progress;
    return s_job.state==2?0:s_job.state;
}
void sf_ps2_hen_cancel(void)
{
    s_job.cancel=1;
}
