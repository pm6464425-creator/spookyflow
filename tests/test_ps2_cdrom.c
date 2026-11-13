/* Independent CD-ROM Mode2/Form1 reconstruction reference vectors.
 * Expected EDC/ECC from ECM/UNECM GPL algorithm, not production function.
 */
#include "sf_ps2_cdrom.h"
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
static int checks=0;
#define CHECK(x) do{if(!(x)){fprintf(stderr,"FAIL %d: %s\n",__LINE__,#x);exit(1);}checks++;}while(0)
static void check_hex(const unsigned char *p,const unsigned char *expected,size_t n)
{CHECK(memcmp(p,expected,n)==0);}
int main(void)
{
    uint8_t userdata[2048],sector[2352],next[2352];
    uint8_t edc_ref[]={0x90,0xf3,0x38,0x5e};
    uint8_t p_ref[]={0xae,0x9d,0x35,0x63,0x08,0x6e,0x74,0x46};
    uint8_t q_ref[]={0x7c,0x09,0x2f,0xe7,0x02,0x8a,0xb9,0xc8};
    uint8_t tail_ref[]={0x90,0xa6,0xc0,0x4c,0x4f,0xa7,0x10,0xa5};
    size_t i;
    for(i=0;i<2048;i++)userdata[i]=(uint8_t)(i*37u+11u);
    sf_cdrom_init();
    sf_cdrom_mode2_form1(sector,userdata,0);
    CHECK(sector[0]==0 && sector[11]==0 && sector[15]==2);
    for(i=1;i<11;i++)CHECK(sector[i]==0xff);
    CHECK(sector[12]==0 && sector[13]==2 && sector[14]==0);
    CHECK(sector[18]==8 && sector[22]==8 && sector[16]==0);
    CHECK(!memcmp(sector+24,userdata,2048));
    check_hex(sector+2072,edc_ref,4);
    check_hex(sector+0x81c,p_ref,8);
    check_hex(sector+0x8c8,q_ref,8);
    check_hex(sector+0x928,tail_ref,8);
    sf_cdrom_mode2_form1(next,userdata,0);
    CHECK(!memcmp(next,sector,2352));
    sf_cdrom_mode2_form1(next,userdata,75);
    CHECK(next[12]==0 && next[13]==3 && next[14]==0);
    sf_cdrom_mode2_form1(next,userdata,4350);
    CHECK(next[12]==1 && next[13]==0 && next[14]==0);
    printf("RAW CD Mode2/Form1: %d deterministic checks passed\n",checks);
    return 0;
}
