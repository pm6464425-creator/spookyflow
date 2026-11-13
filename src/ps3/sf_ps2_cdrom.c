/* SPDX-License-Identifier: GPL-3.0-or-later
 * PS2 CD MODE2/FORM1 sector synthesis from cooked ISO userdata.
 * Adapted from Neill Corlett's unecm.c: EDC/ECC (GNU GPL).
 * Important: generated CD data tracks do NOT restore missing audio/form2.
 */
#include "sf_ps2_cdrom.h"
#include <string.h>
static uint8_t forward[256],backward[256];
static uint32_t edctab[256];
static int initialized;
void sf_cdrom_init(void)
{
    uint32_t i,j,edc;
    if(initialized)return;
    for(i=0;i<256;i++){
        j=(i<<1) ^ ((i&0x80)?0x11Du:0);
        forward[i]=(uint8_t)j;
        backward[i^j]=(uint8_t)i;
        edc=i;
        for(j=0;j<8;j++)edc=(edc>>1) ^ ((edc&1)?0xD8018001u:0);
        edctab[i]=edc;
    }
    initialized=1;
}
static void ecc_block(const uint8_t *src,unsigned maj,unsigned min,
                      unsigned mult,unsigned inc,uint8_t *dest)
{
    unsigned size=maj*min,m,k;
    for(m=0;m<maj;m++){
        unsigned index=(m>>1)*mult+(m&1);
        uint8_t a=0,b=0;
        for(k=0;k<min;k++){
            uint8_t v=src[index];
            index+=inc;if(index>=size)index-=size;
            a^=v;b^=v;a=forward[a];
        }
        a=backward[forward[a]^b];
        dest[m]=a;dest[m+maj]=a^b;
    }
}
static uint8_t bcd(unsigned x){return (uint8_t)(((x/10u)<<4)|(x%10u));}
void sf_cdrom_mode2_form1(uint8_t out[2352],const uint8_t data[2048],
                          uint32_t lba)
{
    unsigned abs=lba+150u;uint32_t edc=0;unsigned i;
    sf_cdrom_init();
    memset(out,0,2352);
    memset(out+1,0xFF,10);
    out[12]=bcd(abs/(60u*75u));
    out[13]=bcd((abs/75u)%60u);
    out[14]=bcd(abs%75u);
    out[15]=2; /* Mode 2 */
    out[18]=out[22]=8; /* DATA and Form 1 flags, duplicated subheader */
    memcpy(out+24,data,2048);
    for(i=16;i<2072;i++)edc=(edc>>8)^edctab[(edc^out[i])&255u];
    out[2072]=(uint8_t)edc;
    out[2073]=(uint8_t)(edc>>8);
    out[2074]=(uint8_t)(edc>>16);
    out[2075]=(uint8_t)(edc>>24);
    /* As in unecm.c, Mode 2 Form 1 ECC calculation zeroes the address. */
    {uint8_t addr[4];memcpy(addr,out+12,4);memset(out+12,0,4);
     ecc_block(out+12,86,24,2,86,out+0x81c);
     ecc_block(out+12,52,43,86,88,out+0x8c8);
     memcpy(out+12,addr,4);
    }
}
