/* SpookyFlow V15.51. Safe, bounded, read-only XRegistry PS1 upscaler probe.
 * Format: 13xforever/xregistry-parser (keys 0x10..0xffef, values
 * 0x10000..0x1ffff), PS3 Dev Wiki /setting/game/emuUpConvert.
 * The registry also contains personal accounts, IDs, etc; NEVER dump it.
 * On HEN, do not write /dev_flash2/etc/xRegistry.sys for an image setting.
 */
#include "sf_ps1_registry.h"
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define REG_KEYS_START 0x10u
#define REG_KEYS_END 0xFFF0u
#define REG_VALUES_START 0x10000u
#define REG_BYTES 0x20000u
static unsigned be16(const unsigned char *p) {
    return ((unsigned)p[0]<<8) | p[1];
}
static uint32_t be32(const unsigned char *p) {
    return ((uint32_t)p[0]<<24) | ((uint32_t)p[1]<<16) |
           ((uint32_t)p[2]<<8) | p[3];
}
static int terminator(const unsigned char *p) {
    return p[0]==0xAA && p[1]==0xBB && p[2]==0xCC &&
           p[3]==0xDD && p[4]==0xEE;
}
const char *sf_ps1_upscaler_name(int mode) {
    switch(mode) {
        case 0: return "OFF";
        case 1: return "NORMAL_4_3";
        case 2: return "FULL_SCREEN_16_9";
        default:return "UNKNOWN";
    }
}
int sf_ps1_registry_read_modes(const char *path, int *upscaler, int *smoothing)
{
    static const unsigned char signature[16]={
      0xBC,0xAD,0xAD,0xBC,0,0,0,0x90,0,0,0,2,0xBC,0xAD,0xAD,0xBC
    };
    static const char keyscale[]="/setting/game/emuUpConvert";
    static const char keysmooth[]="/setting/game/emuSmoothing";
    unsigned short scaleoff=0xffffu,smoothoff=0xffffu;
    unsigned char *data;
    FILE *file;
    size_t i,read;
    int found=0,rc=-3;
    if(upscaler)*upscaler=-1;
    if(smoothing)*smoothing=-1;
    if(!upscaler||!smoothing||!path)return -2;
    file=fopen(path,"rb");
    if(!file)return -1;
    /* A fixed 128 KiB window: never read/copy/output unrelated registry. */
    data=(unsigned char*)malloc(REG_BYTES);
    if(!data){(void)fclose(file);return -1;}
    read=fread(data,1,REG_BYTES,file);
    if(ferror(file)){(void)fclose(file);free(data);return -1;}
    (void)fclose(file);
    if(read<REG_BYTES||memcmp(data,signature,sizeof(signature))!=0){
        free(data);return -2;
    }
    i=REG_KEYS_START;
    while(i+5<=REG_KEYS_END) {
        unsigned length,type;
        size_t entry=i;
        if(terminator(data+i))break;
        length=be16(data+i+2);
        type=data[i+4];
        i+=5;
        if(length==0 || length>4096 || i+length>=REG_KEYS_END) {
            rc=-2;goto done;
        }
        if(type<=3 && length==sizeof(keyscale)-1 &&
           memcmp(data+i,keyscale,length)==0)
            scaleoff=(unsigned short)(entry-REG_KEYS_START);
        if(type<=3 && length==sizeof(keysmooth)-1 &&
           memcmp(data+i,keysmooth,length)==0)
            smoothoff=(unsigned short)(entry-REG_KEYS_START);
        if(data[i+length]!=0){rc=-2;goto done;}
        i+=length+1;
    }
    if(scaleoff==0xffffu){rc=-3;goto done;}
    i=REG_VALUES_START;
    while(i+9<=REG_BYTES) {
        unsigned keyoff,size,type;
        if(terminator(data+i))break;
        keyoff=be16(data+i+2);
        size=be16(data+i+6);
        type=data[i+8];
        i+=9;
        if(size>REG_BYTES-i-1){rc=-2;goto done;}
        if((keyoff==scaleoff || keyoff==smoothoff) &&
           type==1 && size==4 && data[i+size]==0) {
            uint32_t value=be32(data+i);
            if(keyoff==scaleoff){
                if(value>2){rc=-4;goto done;}
                *upscaler=(int)value;
                found=1;
            } else if(value<=1) {
                *smoothing=(int)value;
            }
        }
        i+=size+1;
    }
    rc=found?0:-3;
done:
    free(data);
    return rc;
}
