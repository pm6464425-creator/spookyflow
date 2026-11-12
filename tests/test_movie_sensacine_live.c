#include "movie_sensacine.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
int main(int argc,char **argv)
{
    FILE *fp;
    char *page,*end;
    char url[1200];
    long n;
    int good;
    if(argc!=2)return 4;
    fp=fopen(argv[1],"rb");
    if(!fp)return 5;
    if(fseek(fp,0,SEEK_END)!=0){fclose(fp);return 6;}
    n=ftell(fp);
    if(n<1000||n>1024*1024){fclose(fp);return 7;}
    if(fseek(fp,0,SEEK_SET)!=0){fclose(fp);return 8;}
    page=(char*)malloc((size_t)n+1);
    if(!page){fclose(fp);return 9;}
    n=(long)fread(page,1,(size_t)n,fp);
    fclose(fp);
    page[n]=0;
    good=movie_sensacine_page_poster(page,"Rio",2011,url,sizeof(url));
    printf("REAL SensaCine Rio 2011: status=%s, url=%s\n",
           good?"VERIFIED":"FAILED",good?url:"");
    end=strstr(page,"Año de producción");
    printf("Rio official page contains production-year label: %s\n",
           end?"YES":"NO");
    free(page);
    return good?0:1;
}
