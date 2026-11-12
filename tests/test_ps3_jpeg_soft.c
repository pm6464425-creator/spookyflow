/* Tests the SAME C JPEG fallback compiled into the PS3 PKG. */
#include "ps3_jpeg_soft.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static int has_progressive_sof(const unsigned char *p,size_t n)
{
    size_t i;
    for(i=2;i+1<n;i++)
        if(p[i]==0xff && p[i+1]==0xc2)return 1; /* JPEG SOF2 */
    return 0;
}
int main(int argc,char **argv)
{
    FILE *file;unsigned char *bytes=NULL,*rgba=NULL;
    long size;int w=0,h=0,rc;
    if(argc!=2)return 2;
    file=fopen(argv[1],"rb");if(!file)return 3;
    if(fseek(file,0,SEEK_END)!=0){fclose(file);return 4;}
    size=ftell(file);
    if(size<=0||size>3*1024*1024){fclose(file);return 5;}
    rewind(file);
    bytes=(unsigned char*)malloc((size_t)size);
    if(!bytes){fclose(file);return 6;}
    if(fread(bytes,1,(size_t)size,file)!=(size_t)size){fclose(file);free(bytes);return 7;}
    fclose(file);
    if(!has_progressive_sof(bytes,(size_t)size)){
        fprintf(stderr,"FAIL: test fixture is not progressive JPEG (SOF2)\n");
        free(bytes);return 8;
    }
    rc=ps3_jpeg_soft_decode(bytes,(unsigned)size,2048,&w,&h,&rgba);
    free(bytes);
    if(rc!=0||!rgba||w!=250||h!=372){
        fprintf(stderr,"FAIL: progressive JPEG decode rc=%d image=%dx%d\n",rc,w,h);
        free(rgba);return 9;
    }
    if(rgba[3]!=255 || rgba[(size_t)(w*h-1)*4+3]!=255){
        fprintf(stderr,"FAIL: expected RGBA opacity\n");free(rgba);return 10;
    }
    free(rgba);
    printf("PASS: actual 250x372 progressive JPEG decoded into RGBA for PS3\n");
    return 0;
}
