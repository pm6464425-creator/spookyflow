/* Read real Cinemeta JSON from file and parse it with our shipping C code. */
#include "movie_catalog.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
int main(int argc,char **argv)
{
    FILE *f;long n;char *json;
    char poster[1024],title[256];int ok;
    if(argc!=4){fprintf(stderr,"usage: movie_catalog_cli file title year\n");return 2;}
    f=fopen(argv[1],"rb");if(!f)return 3;
    if(fseek(f,0,SEEK_END)!=0){fclose(f);return 4;}
    n=ftell(f);if(n<=0||n>700000){fclose(f);return 5;}
    rewind(f);
    json=(char*)malloc((size_t)n+1);if(!json){fclose(f);return 6;}
    if(fread(json,1,(size_t)n,f)!=(size_t)n){fclose(f);free(json);return 7;}
    fclose(f);json[n]=0;
    ok=movie_catalog_pick(json,argv[2],atoi(argv[3]),poster,sizeof(poster),
                          title,sizeof(title));
    printf("C catalog parser: title '%s' -> %s, poster %s\n",argv[2],
           ok?title:"NO MATCH",ok?poster:"NO POSTER");
    free(json);
    return ok?0:1;
}
