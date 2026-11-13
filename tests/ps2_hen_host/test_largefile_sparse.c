/* Sparse-file regression: cross the 2 GiB and 4 GiB boundaries without
 * allocating gigabytes of physical storage or encrypting a large ISO. */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include "sf_ps2_largefile.h"
#define CHECK(x) do {if(!(x)){fprintf(stderr,"FAIL %s:%d: %s\n",__FILE__,__LINE__,#x);return 1;}}while(0)
int main(void)
{
    const char *name="build-host/ps2_largefile_64bit_test.sparse";
    const uint64_t offsets[] = {
        (1ULL<<31)-16384, (1ULL<<31), (1ULL<<31)+16384,
        (1ULL<<32)-16384, (1ULL<<32), (1ULL<<32)+16384,
        4866244608ULL-16384
    };
    SfPS2LargeFile f;struct stat st;unsigned char a[16384],b[16384];
    size_t i,j;unsigned n=0;
    for(j=0;j<sizeof(a);j++)a[j]=(unsigned char)(j*19+7);
    CHECK(sf_ps2_large_open(&f,name,1)==0);
    for(i=0;i<sizeof(offsets)/sizeof(offsets[0]);i++){
        CHECK(sf_ps2_large_seek(&f,offsets[i])==0);
        CHECK(sf_ps2_large_tell(&f)==offsets[i]);
        CHECK(sf_ps2_large_write(&f,a,sizeof(a))==0);
        CHECK(sf_ps2_large_tell(&f)==offsets[i]+sizeof(a));
        n+=4;
    }
    CHECK(sf_ps2_large_sync(&f)==0);
    CHECK(sf_ps2_large_close(&f)==0);
    CHECK(stat(name,&st)==0);
    CHECK((uint64_t)st.st_size==4866244608ULL);
    CHECK(sf_ps2_large_open(&f,name,0)==0);
    for(i=0;i<sizeof(offsets)/sizeof(offsets[0]);i++){
        CHECK(sf_ps2_large_read_at(&f,offsets[i],b,sizeof(b))==0);
        CHECK(memcmp(a,b,sizeof(a))==0);
        n+=2;
    }
    CHECK(sf_ps2_large_close(&f)==0);
    CHECK(unlink(name)==0);
    printf("PS2 64-bit sparse file: %u offset/read/write checks PASSED\n",n);
    return 0;
}
