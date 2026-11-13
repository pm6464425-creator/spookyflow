/* Actual PS2 ISO -> PS2 Classics encrypted output round-trip.
 * Uses fake PS3 threads + an independent OpenSSL AES/SHA1 provider, not
 * the cross compiler. No real game data; this is a generated ISO9660 fixture.
 */
#include "sf_ps2_hen.h"
#include <openssl/evp.h>
#include <openssl/sha.h>
#include <openssl/cmac.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
/* Host-only logger supplied by the full PS3 application at runtime. */
void log_printf(const char *fmt,...) {(void)fmt;}
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
    write_file(DIR "/EBOOT.BIN","dummy",5);
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
    CHECK(size_file(USER_BACKUP)==27);
    CHECK(size_file(DIR "/CONFIG.sf-original")==20);
    CHECK(size_file(DIR "/CONFIG")==49152);
    {FILE *cfg=fopen(DIR "/CONFIG","rb");unsigned char head[0x90];
     CHECK(cfg!=NULL);
     CHECK(fread(head,1,sizeof(head),cfg)==sizeof(head));
     CHECK(!memcmp(head,"PS2\\0",4));
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
    CHECK(virtual_size==sizeof(base_data)+SEG);
    blocks=virtual_size/SEG;
    expected=SEG+SEG+virtual_size;
    CHECK(size_file(IMAGE)==expected);
    CHECK(aes(kd,0,klic,16,data_key)==0);
    CHECK(aes(km,0,klic,16,meta_key)==0);
    CHECK(fread(meta_cipher,1,SEG,f)==SEG);
    CHECK(aes(meta_key,1,meta_cipher,SEG,meta)==0);
    for(block=0;block<blocks;block++){
        CHECK(fread(ciphertext,1,SEG,f)==SEG);
        SHA1(ciphertext,SEG,digest);
        CHECK(!memcmp(meta+block*32,digest,20));
        CHECK(be32(meta+block*32+20)==block);
        CHECK(aes(data_key,1,ciphertext,SEG,plain)==0);
        if(block*sizeof(plain)<sizeof(base_data)){
            CHECK(!memcmp(plain,base_data+block*SEG,SEG));
        }else{
            CHECK(!memcmp(plain,"LIMG",4));
            CHECK(plain[7]==1);/* DVD fixture */
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
    /* CD sector layout: build a CD fixture without the DVD signature. */
    {static unsigned char cd[0x40000];unsigned char last_cipher[SEG];
     unsigned char last_data[SEG];const char *cdpath=ROOT "/PS2ISO/Rumble CD.iso";
     size_t j;for(j=0;j<sizeof(cd);j++)cd[j]=(unsigned char)(j*7u+11u);
     cd[0x9318]=1;memcpy(cd+0x9319,"CD001",5);
     cd[0x936c]=0x32;cd[0x936d]=0x01;
     write_file(cdpath,cd,sizeof(cd));
     CHECK(sf_ps2_hen_start(cdpath,NULL)==0);
     CHECK(wait_done()==1);
     f=fopen(IMAGE,"rb");CHECK(f!=NULL);
     CHECK(fseeko(f,(off_t)((2u+blocks-1u)*SEG),SEEK_SET)==0);
     CHECK(fread(last_cipher,1,SEG,f)==SEG);
     CHECK(fclose(f)==0);
     CHECK(aes(data_key,1,last_cipher,SEG,last_data)==0);
     CHECK(!memcmp(last_data,"LIMG",4));
     CHECK(last_data[7]==2);
     CHECK(size_file(USER_BACKUP)==27);
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
