/* V15.51: actual XRegistry parse invariants, no real PS3 registry mutations. */
#include "sf_ps1_registry.h"
#include <stdio.h>
#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#define N 0x20000u
#define K0 "/setting/game/emuUpConvert"
#define K1 "/setting/game/emuSmoothing"
static unsigned char b[N];
static unsigned offset0,offset1;
static void b16(size_t p,unsigned x) {b[p]=(unsigned char)(x>>8);b[p+1]=(unsigned char)x;}
static void b32(size_t p,uint32_t x){
  b[p]=(unsigned char)(x>>24);b[p+1]=(unsigned char)(x>>16);
  b[p+2]=(unsigned char)(x>>8);b[p+3]=(unsigned char)x;
}
static unsigned addkey(unsigned pos,const char *s)
{
    unsigned length=(unsigned)strlen(s);
    b16(pos,0x25a0);b16(pos+2,length);b[pos+4]=0;
    memcpy(b+pos+5,s,length);b[pos+5+length]=0;
    return pos+6+length;
}
static unsigned addvalue(unsigned pos,unsigned keyoff,unsigned n)
{
    b16(pos,0x25a0);b16(pos+2,keyoff);b16(pos+4,0);
    b16(pos+6,4);b[pos+8]=1;b32(pos+9,n);b[pos+13]=0;
    return pos+14;
}
static void fresh(int addscale)
{
    unsigned pos;
    memset(b,0,sizeof(b));
    {
        const unsigned char header[16]={0xbc,0xad,0xad,0xbc,0,0,0,0x90,0,0,0,2,0xbc,0xad,0xad,0xbc};
        memcpy(b,header,16);
    }
    pos=16;
    offset0=pos-16;
    if(addscale)pos=addkey(pos,K0);
    offset1=pos-16;
    pos=addkey(pos,K1);
    memcpy(b+pos,"\xaa\xbb\xcc\xdd\xee",5);
    pos=0x10000;
    if(addscale)pos=addvalue(pos,offset0,2);
    pos=addvalue(pos,offset1,1);
    memcpy(b+pos,"\xaa\xbb\xcc\xdd\xee",5);
}
static void save(const char *path) {
    FILE *f=fopen(path,"wb");assert(f);
    assert(fwrite(b,1,N,f)==N);assert(fclose(f)==0);
}
int main(int argc,char **argv)
{
    int up=-999,sm=-999,rc;
    unsigned p;
    assert(argc==2);
    rc=sf_ps1_registry_read_modes("/this_fake_registry_does_not_exist_987",&up,&sm);
    assert(rc==-1 && up==-1 && sm==-1);
    assert(strcmp(sf_ps1_upscaler_name(0),"OFF")==0);
    assert(strcmp(sf_ps1_upscaler_name(1),"NORMAL_4_3")==0);
    assert(strcmp(sf_ps1_upscaler_name(2),"FULL_SCREEN_16_9")==0);
    for(unsigned mode=0;mode<3;mode++) {
        fresh(1);
        p=0x10000;
        b32(p+9,mode);save(argv[1]);
        assert(sf_ps1_registry_read_modes(argv[1],&up,&sm)==0);
        assert(up==(int)mode && sm==1);
    }
    fresh(1);
    b32(0x10000+9,99);save(argv[1]);
    assert(sf_ps1_registry_read_modes(argv[1],&up,&sm)==-4);
    fresh(0);save(argv[1]);
    assert(sf_ps1_registry_read_modes(argv[1],&up,&sm)==-3);
    fresh(1);b[0]=0;save(argv[1]);
    assert(sf_ps1_registry_read_modes(argv[1],&up,&sm)==-2);
    fresh(1);b16(0x10+2,0xffff);save(argv[1]);
    assert(sf_ps1_registry_read_modes(argv[1],&up,&sm)==-2);
    remove(argv[1]);
    puts("PASS V15.51 XRegistry read-only: OFF/NORMAL/FULL, smoothing, missing, corrupt, invalid values, fail-closed");
    return 0;
}
