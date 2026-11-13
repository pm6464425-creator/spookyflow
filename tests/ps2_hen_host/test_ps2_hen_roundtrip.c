/* Actual PS2 ISO -> PS2 Classics encrypted output round-trip.
 * Uses fake PS3 threads + an independent OpenSSL AES/SHA1 provider, not
 * the cross compiler. No real game data; this is a generated ISO9660 fixture.
 */
#include "sf_ps2_hen.h"
#include "sf_ps2_cdrom.h"
#include <openssl/evp.h>
#include <openssl/sha.h>
#include <openssl/cmac.h>
#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
/* Host logger captures the SAME read-only encrypted image inspection
 * performed on the PS3, independently verifying its reported status. */
static int diag_good_images,diag_good_limg,diag_good_config;
void log_printf(const char *fmt,...)
{
    char buf[1024];va_list a;
    va_start(a,fmt);vsnprintf(buf,sizeof(buf),fmt,a);va_end(a);
    if(strstr(buf,"PS2_DIAG ENCRYPTED_SAMPLES_VALID=1"))diag_good_images++;
    if(strstr(buf,"PS2_DIAG LIMG_PRESENT=1") &&
       strstr(buf,"VALID=1"))diag_good_limg++;
    if(strstr(buf,"PS2_DIAG CONFIG_PRESENT=1 CONFIG_ENCRYPTED_VALID=1"))
       diag_good_config++;
}
#define SEG 16384u
static int checks;
#define CHECK(x) do {if(!(x)){fprintf(stderr,"PS2 HEN FAIL line %d: %s\n",__LINE__,#x);exit(1);}checks++;}while(0)
#define ROOT "build-host/ps2hen"
#define DIR ROOT "/placeholder/USRDIR"
#define ISO ROOT "/PS2ISO/Rumble Racing.iso"
#define IMAGE DIR "/ISO.BIN.ENC"
#define USER_BACKUP DIR "/ISO.BIN.ENC.sf-original"
static unsigned char base_data[0x40000];
static uint64_t size_file(const char *p)
{struct stat st;return stat(p,&st)==0?(uint64_t)st.st_size:0;}
static uint64_t be64(const unsigned char *p)
{uint64_t out=0;int i;for(i=0;i<8;i++)out=(out<<8)|p[i];return out;}
static uint32_t be32(const unsigned char *p)
{uint32_t out=0;int i;for(i=0;i<4;i++)out=(out<<8)|p[i];return out;}
static int aes(const unsigned char key[16],int decrypt,const unsigned char *src,size_t n,unsigned char *dst)
{
    EVP_CIPHER_CTX *c=EVP_CIPHER_CTX_new();unsigned char iv[16]={0};int k=0,l=0,ok=0;
    if(!c)return -1;
    if(decrypt){
        if(EVP_DecryptInit_ex(c,EVP_aes_128_cbc(),NULL,key,iv)!=1)goto done;
        EVP_CIPHER_CTX_set_padding(c,0);
        if(EVP_DecryptUpdate(c,dst,&k,src,(int)n)!=1||k!=(int)n)goto done;
        if(EVP_DecryptFinal_ex(c,dst+k,&l)!=1||l!=0)goto done;
    }else{
        if(EVP_EncryptInit_ex(c,EVP_aes_128_cbc(),NULL,key,iv)!=1)goto done;
        EVP_CIPHER_CTX_set_padding(c,0);
        if(EVP_EncryptUpdate(c,dst,&k,src,(int)n)!=1||k!=(int)n)goto done;
        if(EVP_EncryptFinal_ex(c,dst+k,&l)!=1||l!=0)goto done;
    }
    ok=1;
done:EVP_CIPHER_CTX_free(c);return ok?0:-1;
}
static int wait_done(void)
{
    int i,p=0,state;
    for(i=0;i<1500;i++){
        state=sf_ps2_hen_poll(&p);
        if(state!=0)return state;
        usleep(10000);
    }
    return -999;
}
static void write_file(const char *path,const void *buf,size_t n)
{FILE *f=fopen(path,"wb");CHECK(f!=NULL);CHECK(fwrite(buf,1,n,f)==n);CHECK(fclose(f)==0);}
static int independent_cmac(const unsigned char key[16],
                            const unsigned char *data,size_t n,unsigned char out[16])
{
    CMAC_CTX *ctx=CMAC_CTX_new();
    size_t out_len=0;
    int ok=0;
    if(!ctx)return -1;
    ok=CMAC_Init(ctx,key,16,EVP_aes_128_cbc(),NULL)==1 &&
       CMAC_Update(ctx,data,n)==1 &&
       CMAC_Final(ctx,out,&out_len)==1 && out_len==16;
    CMAC_CTX_free(ctx);
    return ok?0:-1;
}
static void make_fixture(void)
{
    FILE *f;size_t i;
    CHECK(system("mkdir -p build-host/ps2hen/placeholder/USRDIR build-host/ps2hen/PS2ISO")==0);
    for(i=0;i<sizeof(base_data);i++)base_data[i]=(unsigned char)((i*31u+7u)&255u);
    base_data[0x8000]=1;memcpy(base_data+0x8001,"CD001",5);
    base_data[0x8054]=0x34;base_data[0x8055]=0x12;
    base_data[0x8056]=0x00;base_data[0x8057]=0x00;
    f=fopen(ISO,"wb");CHECK(f!=NULL);
    CHECK(fwrite(base_data,1,sizeof(base_data),f)==sizeof(base_data));
    CHECK(fclose(f)==0);
    /* Real package evidence: launcher may exist even if EBOOT is not
     * visible as a regular file. PARAM.SFO + USRDIR is a valid install. */
    {unsigned char sfo[64]={0};
     /* Proper little-endian PARAM.SFO with TITLE_ID string record. */
     sfo[0]=0;sfo[1]='P';sfo[2]='S';sfo[3]='F';sfo[4]=1;sfo[5]=1;
     sfo[8]=36; sfo[12]=48; sfo[16]=1;
     sfo[22]=4; sfo[23]=2; sfo[24]=10; sfo[28]=12;
     memcpy(sfo+36,"TITLE_ID",9);
     memcpy(sfo+48,"PS2U10000",10);
     write_file(ROOT "/placeholder/PARAM.SFO",sfo,sizeof(sfo));
    }
    write_file(IMAGE,"EXISTING USER CLASSICS GAME",27);
    write_file(DIR "/CONFIG","PREVIOUS USER CONFIG",20);
}
int main(void)
{
    static const unsigned char klic[16]={0xe4,0xe5,0x4f,0xd6,0x7c,0x16,0xc3,0x16,0xf4,0x78,0x29,0xa3,0x04,0x84,0xd8,0x43};
    static const unsigned char kd[16]={0x10,0x17,0x82,0x34,0x63,0xf4,0x68,0xc1,0xaa,0x41,0xd7,0x00,0xb1,0x40,0xf2,0x57};
    static const unsigned char km[16]={0x38,0x9d,0xcb,0xa5,0x20,0x3c,0x81,0x59,0xec,0xf9,0x4c,0x93,0x93,0x16,0x4c,0xc9};
    unsigned char data_key[16],meta_key[16],header[SEG],meta_cipher[SEG],meta[SEG];
    unsigned char ciphertext[SEG],plain[SEG],digest[20],original_sha[20],next_sha[20];
    uint64_t virtual_size,expected,block,blocks;int rc;FILE *f;
    CHECK(system("rm -rf build-host/ps2hen")==0);
    make_fixture();
    /* Isolated regression for the user's real screenshot: previously
     * V15.14 rejected PS2U10000 solely for an unseen USRDIR/EBOOT.BIN. */
    CHECK(size_file(DIR "/EBOOT.BIN")==0);
    CHECK(size_file(ROOT "/placeholder/PARAM.SFO")>0);
    /* An uninstalled launcher must still fail safely without side effects. */
    CHECK(rename(ROOT "/placeholder/PARAM.SFO",ROOT "/placeholder/PARAM.SFO.hide")==0);
    CHECK(sf_ps2_hen_start(ISO,"SLUS_201.74")==-64);
    CHECK(rename(ROOT "/placeholder/PARAM.SFO.hide",ROOT "/placeholder/PARAM.SFO")==0);
    /* A different title ID cannot authorize modifications to this launcher. */
    {FILE *sfo=fopen(ROOT "/placeholder/PARAM.SFO","r+b");
     CHECK(sfo!=NULL);CHECK(fseeko(sfo,48,SEEK_SET)==0);
     CHECK(fwrite("WRONG10000",1,10,sfo)==10);CHECK(fclose(sfo)==0);}
    CHECK(sf_ps2_hen_start(ISO,"SLUS_201.74")==-64);
    {FILE *sfo=fopen(ROOT "/placeholder/PARAM.SFO","r+b");
     CHECK(sfo!=NULL);CHECK(fseeko(sfo,48,SEEK_SET)==0);
     CHECK(fwrite("PS2U10000",1,10,sfo)==10);CHECK(fclose(sfo)==0);}
    /* Diagnostic is readable and records the matched title ID. */
    CHECK(sf_ps2_hen_start(ROOT "/PS2ISO/not_present.iso",NULL)<0);
    SHA1(base_data,sizeof(base_data),original_sha);
    CHECK(sf_ps2_hen_start("build-host/ps2hen/PS2ISO/no.iso",NULL)<0);
    rc=sf_ps2_hen_start(ISO,"SLUS_201.74");
    CHECK(rc==0);
    CHECK(wait_done()==1);
    CHECK(size_file(ISO)==sizeof(base_data));
    f=fopen(ISO,"rb");CHECK(f!=NULL);
    {unsigned char buf[sizeof(base_data)];CHECK(fread(buf,1,sizeof(buf),f)==sizeof(buf));
     SHA1(buf,sizeof(buf),next_sha);CHECK(!memcmp(original_sha,next_sha,20));}
    fclose(f);
    /* Worker inspected the installed ciphertext and LIMG before returning. */
    CHECK(diag_good_images>=1);
    CHECK(diag_good_limg>=1);
    CHECK(diag_good_config>=1);
    CHECK(size_file(USER_BACKUP)==27);
    CHECK(size_file(DIR "/CONFIG.sf-original")==20);
    CHECK(size_file(DIR "/CONFIG")==49152);
    {FILE *cfg=fopen(DIR "/CONFIG","rb");unsigned char head[0x90];
     CHECK(cfg!=NULL);
     CHECK(fread(head,1,sizeof(head),cfg)==sizeof(head));
     CHECK(head[0]=='P'&&head[1]=='S'&&head[2]=='2'&&head[3]==0);
     CHECK(!memcmp(head+0x10,"2P0001-PS2U10000_00-0000111122223333",36));
     CHECK(fclose(cfg)==0);}
    f=fopen(USER_BACKUP,"rb");CHECK(f!=NULL);
    {char buf[28]={0};CHECK(fread(buf,1,27,f)==27);CHECK(strcmp(buf,"EXISTING USER CLASSICS GAME")==0);}
    fclose(f);
    f=fopen(IMAGE,"rb");CHECK(f!=NULL);
    CHECK(fread(header,1,SEG,f)==SEG);
    CHECK(!memcmp(header,"PS2\0",4));
    CHECK(!memcmp(header+0x10,"2P0001-PS2U10000_00-0000111122223333",36));
    /* Independently compute both NPDRM AES-CMAC header tags. */
    {static const unsigned char k2[16]={0x6b,0xa5,0x29,0x76,0xef,0xda,0x16,0xef,0x3c,0x33,0x9f,0xb2,0x97,0x1e,0x25,0x6b};
     static const unsigned char k3[16]={0x9b,0x51,0x5f,0xea,0xcf,0x75,0x06,0x49,0x81,0xaa,0x60,0x4d,0x91,0xa5,0x4e,0x97};
     static const unsigned char kek[16]={0x72,0xf9,0x90,0x78,0x8f,0x9c,0xff,0x74,0x57,0x25,0xf0,0x8e,0x4c,0x12,0x83,0x87};
     unsigned char tmp[0x30+11],tag[16],key[16];
     size_t j;
     memcpy(tmp,header+0x10,0x30);
     memcpy(tmp+0x30,"ISO.BIN.ENC",11);
     CHECK(independent_cmac(k3,tmp,sizeof(tmp),tag)==0);
     CHECK(!memcmp(tag,header+0x50,16));
     for(j=0;j<16;j++)key[j]=kek[j]^k2[j];
     CHECK(independent_cmac(key,header,0x60,tag)==0);
     CHECK(!memcmp(tag,header+0x60,16));
    }
    CHECK(be32(header+0x84)==SEG);
    virtual_size=be64(header+0x88);
    CHECK(virtual_size==((sizeof(base_data)/2048u*2352u+SEG-1u)/SEG)*SEG+SEG);
    blocks=virtual_size/SEG;
    expected=SEG+SEG+virtual_size;
    CHECK(size_file(IMAGE)==expected);
    CHECK(aes(kd,0,klic,16,data_key)==0);
    CHECK(aes(km,0,klic,16,meta_key)==0);
    CHECK(fread(meta_cipher,1,SEG,f)==SEG);
    CHECK(aes(meta_key,1,meta_cipher,SEG,meta)==0);
    {static unsigned char raw_cd[2352u*(sizeof(base_data)/2048u)];
     uint64_t rawbytes=sizeof(raw_cd);
     size_t z;
     for(z=0;z<sizeof(base_data)/2048u;z++)
         sf_cdrom_mode2_form1(raw_cd+z*2352u,base_data+z*2048u,(uint32_t)z);
     for(block=0;block<blocks;block++){
         size_t n;
         CHECK(fread(ciphertext,1,SEG,f)==SEG);
         SHA1(ciphertext,SEG,digest);
         CHECK(!memcmp(meta+block*32,digest,20));
         CHECK(be32(meta+block*32+20)==block);
         CHECK(aes(data_key,1,ciphertext,SEG,plain)==0);
         if(block*SEG<rawbytes) {
             n=(size_t)((rawbytes-block*SEG)<SEG?(rawbytes-block*SEG):SEG);
             CHECK(!memcmp(plain,raw_cd+block*SEG,n));
             for(z=n;z<SEG;z++)CHECK(plain[z]==0);
         }else if(block==blocks-1) {
             CHECK(!memcmp(plain,"LIMG",4));
             CHECK(plain[7]==2);/* Rumble US CD-mode 2352 */
             CHECK(be32(plain+12)==2352);
         }else{
             for(z=0;z<SEG;z++)CHECK(plain[z]==0);
         }
     }
    }
    CHECK(fclose(f)==0);
    CHECK(sf_ps2_hen_start(ISO,"SLUS_201.74")==1); /* cache skip */
    CHECK(size_file(DIR "/ISO.BIN.ENC.sf-part")==0); /* no leftover partial */
    CHECK(size_file(DIR "/ISO.BIN.ENC.sf-source")>0);
    /* Same name and size, modified contents: must invalidate the cache. */
    f=fopen(ISO,"rb+");CHECK(f!=NULL);
    CHECK(fputc(0xF2,f)!=EOF);CHECK(fclose(f)==0);
    CHECK(sf_ps2_hen_start(ISO,"SLUS_201.74")==0);
    CHECK(wait_done()==1);
    CHECK(size_file(USER_BACKUP)==27);
    CHECK(size_file(DIR "/ISO.BIN.ENC.sf-old")==0);
    CHECK(size_file(DIR "/ISO.BIN.ENC.sf-source-old")==0);
    /* The V15.19 PS3 log hit RC=-40 at 100% because the old
     * ISO.BIN.ENC.sf-source was still the destination of a rename().
     * Make the next conversion replace an existing manifest safely,
     * then reject a stale recovery copy without touching the current game. */
    {
        unsigned char probe[0x90];
        uint64_t previous_image_bytes=size_file(IMAGE);
        uint64_t previous_manifest_bytes=size_file(DIR "/ISO.BIN.ENC.sf-source");
        f=fopen(IMAGE,"rb");CHECK(f!=NULL);
        CHECK(fread(probe,1,sizeof(probe),f)==sizeof(probe));
        CHECK(fclose(f)==0);
        write_file(DIR "/ISO.BIN.ENC.sf-source-old","INTERRUPTED",11);
        f=fopen(ISO,"r+b");CHECK(f!=NULL);
        CHECK(fputc(0xF3,f)!=EOF);CHECK(fclose(f)==0);
        CHECK(sf_ps2_hen_start(ISO,"SLUS_201.74")==0);
        CHECK(wait_done()==-43);
        CHECK(size_file(IMAGE)==previous_image_bytes);
        CHECK(size_file(DIR "/ISO.BIN.ENC.sf-source")==previous_manifest_bytes);
        {unsigned char check[0x90];f=fopen(IMAGE,"rb");CHECK(f!=NULL);
         CHECK(fread(check,1,sizeof(check),f)==sizeof(check));
         CHECK(fclose(f)==0);CHECK(!memcmp(check,probe,sizeof(probe)));}
        CHECK(size_file(DIR "/ISO.BIN.ENC.sf-source-old")==11);
        CHECK(size_file(DIR "/ISO.BIN.ENC.sf-part")==0);
        CHECK(remove(DIR "/ISO.BIN.ENC.sf-source-old")==0);
        CHECK(sf_ps2_hen_start(ISO,"SLUS_201.74")==0);
        CHECK(wait_done()==1);
        CHECK(size_file(DIR "/ISO.BIN.ENC.sf-source-old")==0);
        CHECK(size_file(DIR "/ISO.BIN.ENC.sf-old")==0);
        CHECK(size_file(USER_BACKUP)==27);
    }
    /* CD sector layout: build a CD fixture without the DVD signature. */
    {static unsigned char cd[2352u*112u];unsigned char last_cipher[SEG];
     unsigned char last_data[SEG];const char *cdpath=ROOT "/PS2ISO/Rumble CD.iso";
     size_t j;for(j=0;j<sizeof(cd);j++)cd[j]=(unsigned char)(j*7u+11u);
     cd[0x9318]=1;memcpy(cd+0x9319,"CD001",5);
     cd[0x936c]=0x32;cd[0x936d]=0x01;
     write_file(cdpath,cd,sizeof(cd));
     CHECK(sf_ps2_hen_start(cdpath,NULL)==0);
     CHECK(wait_done()==1);
     CHECK(diag_good_images>=2);
     CHECK(diag_good_limg>=2);
     f=fopen(IMAGE,"rb");CHECK(f!=NULL);
     CHECK(fread(header,1,SEG,f)==SEG);
     blocks=be64(header+0x88)/SEG;
     CHECK(fseeko(f,(off_t)((2u+blocks-1u)*SEG),SEEK_SET)==0);
     CHECK(fread(last_cipher,1,SEG,f)==SEG);
     CHECK(fclose(f)==0);
     CHECK(aes(data_key,1,last_cipher,SEG,last_data)==0);
     CHECK(!memcmp(last_data,"LIMG",4));
     CHECK(last_data[7]==2);
     CHECK(size_file(USER_BACKUP)==27);
    }
    /* PAL region, cooked CD (2048/sector): reconstruct real 2352 data
     * track without modifying source; LIMG must say CD, not DVD. Check the
     * entire decrypted data track, not just first/last blocks. */
    {static unsigned char cooked[0x40000];
     static unsigned char raw[2352u*(sizeof(cooked)/2048u)];
     unsigned char meta_encrypted[SEG],tmp_data[SEG];
     const char *pal=ROOT "/PS2ISO/Rumble Racing PAL.iso";
     uint64_t rawbytes=sizeof(raw),p=0,k,total_blocks,virtual_bytes;
     size_t j,actual;
     for(j=0;j<sizeof(cooked);j++)cooked[j]=(unsigned char)(j*19u+13u);
     cooked[0x8000]=1;memcpy(cooked+0x8001,"CD001",5);
     cooked[0x8054]=0; cooked[0x8055]=0; cooked[0x8056]=0; cooked[0x8057]=128;
     write_file(pal,cooked,sizeof(cooked));
     CHECK(sf_ps2_hen_start(pal,"SLES_501.20")==0);
     CHECK(wait_done()==1);
     CHECK(size_file(pal)==sizeof(cooked));
     CHECK(size_file(DIR "/CONFIG")==49152);
     CHECK(diag_good_config>=2);
     f=fopen(DIR "/CONFIG.sf-source","rb");CHECK(f!=NULL);
     {char line[200];CHECK(fgets(line,sizeof(line),f)!=NULL);
      CHECK(strstr(line,"SLES_501.20")!=NULL);}
     CHECK(fclose(f)==0);
     f=fopen(IMAGE,"rb");CHECK(f!=NULL);
     CHECK(fread(header,1,SEG,f)==SEG);
     virtual_bytes=be64(header+0x88);
     total_blocks=virtual_bytes/SEG;
     CHECK(virtual_bytes==((rawbytes+SEG-1u)/SEG)*SEG+SEG);
     CHECK(fread(meta_encrypted,1,SEG,f)==SEG);
     for(k=0;k<total_blocks;k++){
         CHECK(fread(ciphertext,1,SEG,f)==SEG);
         CHECK(aes(data_key,1,ciphertext,SEG,tmp_data)==0);
         if(k*SEG<rawbytes){
             actual=(size_t)(rawbytes-k*SEG<SEG?rawbytes-k*SEG:SEG);
             memcpy(raw+k*SEG,tmp_data,actual);
             p+=actual;
         }else if(k==total_blocks-1){
             CHECK(!memcmp(tmp_data,"LIMG",4));
             CHECK(tmp_data[7]==2);
             CHECK(be32(tmp_data+8)==sizeof(cooked)/2048u);
             CHECK(be32(tmp_data+12)==2352);
         }
     }
     CHECK(p==rawbytes);
     for(j=0;j<sizeof(cooked)/2048u;j++){
         const unsigned char *sector=raw+j*2352u;
         CHECK(sector[0]==0 && sector[1]==255 && sector[11]==0);
         CHECK(sector[15]==2 && sector[18]==8 && sector[22]==8);
         CHECK(!memcmp(sector+24,cooked+j*2048u,2048));
     }
     CHECK(fclose(f)==0);
     CHECK(diag_good_limg>=3);
     CHECK(sf_ps2_hen_start(pal,"SLES_501.20")==1);
     CHECK(size_file(DIR "/CONFIG")==49152);
     expected=size_file(IMAGE);
    }
    /* A corrupt ISO must not replace the valid previously prepared image. */
    {static unsigned char bad[0x20000];write_file(ROOT "/PS2ISO/bad.iso",bad,sizeof(bad));}
    CHECK(sf_ps2_hen_start(ROOT "/PS2ISO/bad.iso",NULL)==0);
    CHECK(wait_done()<0);
    CHECK(size_file(IMAGE)==expected);
    CHECK(size_file(DIR "/ISO.BIN.ENC.sf-part")==0);
    printf("PS2 HEN ACTUAL AES/SHA1 round-trip: %d checks passed\n",checks);
    return 0;
}
