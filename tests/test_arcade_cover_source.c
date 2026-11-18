/* Verified arcade boxart / URL / download tests.
 * No changes permitted to console cover paths or console games.
 */
#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "arcade_boxart.h"
#include "coverart.h"
#include "util.h"

static const char index_text[]=
"# rom<TAB>verified FBNeo<TAB>verified MAME\n"
"gng\tGhosts'n Goblins (World).png\tGhosts'n Goblins.png\n"
"ghouls\tGhouls'n Ghosts (World).png\tGhouls'n Ghosts.png\n"
"wh1\tWorld Heroes (NGM-053).png\tWorld Heroes.png\n";

static int cover_present,requests,write_ok,encoded;
static char dest[512];
static int fake_read(void *ctx,const char *path,uint64_t off,void *out,uint32_t cap)
{
    size_t n=strlen(index_text);
    (void)ctx;
    if(strcmp(path,ARCADE_BOXART_INDEX)==0 && !off && cap>=n){
        memcpy(out,index_text,n);return (int)n;
    }
    return -1;
}
static int fake_is_file(void *ctx,const char *path)
{
    (void)ctx;
    if(cover_present && strstr(path,"/ARCADE/COVERS/gng.png"))return 1;
    return 0;
}
static int fake_mkdir(void *ctx,const char *path){(void)ctx;(void)path;return 0;}
static int fake_write(void *ctx,const char *path,const void *a,uint32_t n,
                      const void *b,uint32_t n2)
{
    (void)ctx;(void)b;(void)n2;
    assert(a && n>=256 && n<=184320u);
    sf_strlcpy(dest,path,sizeof dest);
    write_ok++;return 0;
}
static int fake_jpeg(void *ctx,const uint8_t *src,uint32_t n,
                     uint8_t **out,uint32_t *out_len)
{
    (void)ctx;assert(src && n>=64);
    *out=(uint8_t *)malloc(300);
    assert(*out);
    (*out)[0]=0xff;(*out)[1]=0xd8;(*out)[2]=0xff;
    *out_len=300;encoded++;
    return 0;
}
static int fake_http(void *ctx,const char *url,uint8_t *body,uint32_t cap,
                     uint32_t *n,char *location,size_t location_cap)
{
    (void)ctx;
    assert(strstr(url,"FBNeo%20-%20Arcade%20Games") || strstr(url,"/MAME/"));
    assert(strstr(url,"Named_Boxarts/"));
    requests++;
    assert(cap>160);
    memset(body,0,160);
    memcpy(body,"\x89PNG\r\n\x1a\n",8);
    *n=160;
    if(location_cap)location[0]=0;
    return 200;
}
int main(void)
{
    ArcadeBoxartIndex idx={0};
    CoverIO io={0};
    CoverDl dl={0},dl_manual={0},dl_unknown={0};
    CoverDlItem item={0},manual={0},unknown={0};
    char (*urls)[COVER_URL_MAX];
    char *textbuf;
    int n;
    textbuf=(char *)malloc(strlen(index_text)+1);
    assert(textbuf);
    memcpy(textbuf,index_text,strlen(index_text)+1);
    assert(arcade_boxart_index_parse(&idx,textbuf,strlen(textbuf))==3);
    urls=malloc(3*COVER_URL_MAX);
    assert(urls);
    n=arcade_boxart_urls(&idx,"/dev_hdd0/ARCADE/gng.zip",urls,3);
    assert(n==2);
    assert(strstr(urls[0],"Ghosts%27n%20Goblins%20%28World%29.png"));
    assert(strstr(urls[1],"/MAME/Named_Boxarts/"));
    assert(arcade_boxart_urls(&idx,"wh1.ZIP",urls,3)==2);
    assert(arcade_boxart_urls(&idx,"unknown123.zip",urls,3)==0);
    assert(arcade_boxart_urls(&idx,"../gng.png",urls,3)==0);
    assert(arcade_boxart_urls(&idx,"evil?.zip",urls,3)==0);
    arcade_boxart_index_free(&idx);
    free(urls);
    io.read_at=fake_read;io.is_file=fake_is_file;io.mkdir=fake_mkdir;
    io.write_file=fake_write;io.arcade_compact_jpeg=fake_jpeg;
    item.plat=PLAT_ARCADE;
    strcpy(item.path,"/dev_hdd0/ARCADE/gng.zip");
    dl.item=&item;dl.n=1;dl.io=&io;dl.http=fake_http;dl.auto_mode=1;
    cover_dl_run(&dl);
    assert(dl.finished && dl.n_got==1 && item.state==DL_GOT);
    assert(requests==1 && write_ok==1 && encoded==1);
    assert(!strcmp(dest,"/dev_hdd0/ARCADE/COVERS/gng.jpg"));
    printf("PASS: verified arcade URL -> compressed JPG in manual-cover folder, no PNG stored\n");

    cover_present=1;write_ok=encoded=0;requests=0;
    manual.plat=PLAT_ARCADE;
    strcpy(manual.path,"/dev_hdd0/ARCADE/gng.zip");
    dl_manual.item=&manual;dl_manual.n=1;dl_manual.io=&io;
    dl_manual.http=fake_http;dl_manual.auto_mode=1;
    cover_dl_run(&dl_manual);
    assert(dl_manual.finished && dl_manual.n_have==1 && manual.state==DL_HAVE);
    assert(requests==0 && write_ok==0 && encoded==0);
    printf("PASS: user /ARCADE/COVERS/gng.png is never overwritten or downloaded again\n");

    cover_present=0;write_ok=encoded=0;requests=0;
    unknown.plat=PLAT_ARCADE;
    strcpy(unknown.path,"/dev_hdd0/ARCADE/not_in_catalog.zip");
    dl_unknown.item=&unknown;dl_unknown.n=1;dl_unknown.io=&io;
    dl_unknown.http=fake_http;dl_unknown.auto_mode=1;
    cover_dl_run(&dl_unknown);
    assert(dl_unknown.finished && dl_unknown.n_missing==1 &&
           unknown.state==DL_MISSING && requests==0 && write_ok==0);
    printf("PASS: unknown arcade code cannot pick a different game's artwork\n");
    return 0;
}
