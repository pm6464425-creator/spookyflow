/* Libretro FBNeo/MAME verified thumbnail names, keyed by original ROM ZIP.
 * The index is built from actual published files and may be installed
 * entirely offline. This module does not touch any console path.
 */
#include "arcade_boxart.h"
#include "util.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

static int by_code(const void *aa,const void *bb)
{
    const ArcadeBoxartRow *a=(const ArcadeBoxartRow *)aa;
    const ArcadeBoxartRow *b=(const ArcadeBoxartRow *)bb;
    return strcmp(a->code,b->code);
}
int arcade_boxart_rom_code(const char *path,char *out,size_t cap)
{
    const char *p,*base;
    size_t i,n;
    if(!path || !out || cap<3)return 0;
    base=path;
    for(p=path;*p;p++)if(*p=='/' || *p=='\\')base=p+1;
    n=strlen(base);
    if(n>=4 && sf_ends_with_i(base,".zip"))n-=4;
    else if(n>=3 && sf_ends_with_i(base,".7z"))n-=3;
    else return 0;
    if(!n || n>=cap || n>90)return 0;
    for(i=0;i<n;i++){
        unsigned char c=(unsigned char)base[i];
        if(!((c>='a'&&c<='z')||(c>='A'&&c<='Z')||
             (c>='0'&&c<='9')||c=='_'||c=='+'||c=='-'||c=='.'))return 0;
        out[i]=(char)((c>='A'&&c<='Z')?c-'A'+'a':c);
    }
    out[n]=0;
    return 1;
}
void arcade_boxart_index_free(ArcadeBoxartIndex *ix)
{
    if(!ix)return;
    free(ix->bytes); free(ix->rows);
    ix->bytes=NULL; ix->rows=NULL; ix->count=0;
}
int arcade_boxart_index_parse(ArcadeBoxartIndex *out,char *data,size_t n)
{
    char *cur,*end;
    if(!out || !data || n<16 || n>ARCADE_BOXART_MAX_BYTES)return 0;
    memset(out,0,sizeof(*out));
    out->bytes=data;
    out->rows=(ArcadeBoxartRow *)malloc(sizeof(ArcadeBoxartRow)*ARCADE_BOXART_MAX_ROWS);
    if(!out->rows){arcade_boxart_index_free(out);return 0;}
    end=data+n;*end=0;
    cur=data;
    while(cur<end && out->count<ARCADE_BOXART_MAX_ROWS){
        char *tab1,*tab2,*line=cur,*eol=(char *)memchr(cur,'\n',(size_t)(end-cur));
        if(!eol)eol=end;
        cur=eol<end?eol+1:end;
        *eol=0;
        if(!line[0] || line[0]=='#')continue;
        tab1=strchr(line,'\t');
        if(!tab1 || tab1==line)continue;
        *tab1++=0;
        tab2=strchr(tab1,'\t');
        if(!tab2)continue;
        *tab2++=0;
        if(strchr(tab2,'\r'))*strchr(tab2,'\r')=0;
        if(!tab1[0] && !tab2[0])continue;
        out->rows[out->count].code=line;
        out->rows[out->count].fbneo=tab1;
        out->rows[out->count].mame=tab2;
        out->count++;
    }
    if(out->count==0){arcade_boxart_index_free(out);return 0;}
    qsort(out->rows,(size_t)out->count,sizeof(out->rows[0]),by_code);
    return out->count;
}
int arcade_boxart_index_load(ArcadeBoxartIndex *out,const CoverIO *io)
{
    char *data; int n;
    if(!out || !io || !io->read_at)return 0;
    data=(char *)malloc(ARCADE_BOXART_MAX_BYTES+1u);
    if(!data)return 0;
    n=io->read_at(io->ctx,ARCADE_BOXART_INDEX,0,data,ARCADE_BOXART_MAX_BYTES);
    if(n<16 || n>=(int)ARCADE_BOXART_MAX_BYTES){free(data);return 0;}
    return arcade_boxart_index_parse(out,data,(size_t)n);
}
static int percent_url(char *dst,size_t cap,const char *base,const char *filename)
{
    static const char h[]="0123456789ABCDEF";
    size_t pos=strlen(base),i;
    if(pos>=cap)return 0;
    memcpy(dst,base,pos+1);
    for(i=0;filename[i];i++){
        unsigned char c=(unsigned char)filename[i];
        int safe=(c>='a'&&c<='z')||(c>='A'&&c<='Z')||
                 (c>='0'&&c<='9')||c=='_'||c=='-'||c=='.'||c=='~';
        if(safe){
            if(pos+2>cap)return 0;
            dst[pos++]=(char)c;
        }else{
            if(pos+4>cap)return 0;
            dst[pos++]='%';dst[pos++]=h[c>>4];dst[pos++]=h[c&15];
        }
    }
    if(pos>=cap)return 0;
    dst[pos]=0;
    return 1;
}
int arcade_boxart_urls(const ArcadeBoxartIndex *index,const char *rom_path,
                       char out[][COVER_URL_MAX],int max)
{
    static const char *const bases[]={
        "https://thumbnails.libretro.com/FBNeo%20-%20Arcade%20Games/Named_Boxarts/",
        "https://thumbnails.libretro.com/MAME/Named_Boxarts/"
    };
    ArcadeBoxartRow key,*hit;
    const char *names[2];
    char code[96];
    int n=0,j;
    if(!index || !index->rows || index->count<=0 || !out || max<1 ||
       !arcade_boxart_rom_code(rom_path,code,sizeof(code)))return 0;
    key.code=code;key.fbneo=key.mame=NULL;
    hit=(ArcadeBoxartRow *)bsearch(&key,index->rows,(size_t)index->count,
                                   sizeof(index->rows[0]),by_code);
    if(!hit)return 0;
    names[0]=hit->fbneo;names[1]=hit->mame;
    for(j=0;j<2 && n<max;j++)
        if(names[j] && names[j][0] &&
           percent_url(out[n],COVER_URL_MAX,bases[j],names[j]))n++;
    return n;
}
