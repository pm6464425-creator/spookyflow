/* Read public movie posters from SensaCine's MOVIE search results.
 * The site labels theatrical posters "Cartel de <film>" and provides
 * direct HTTPS image URLs hosted by es.web.img*.acsta.net.
 * Do not scrape unrelated stills, album images, merchandise or disc art.
 */
#include "movie_sensacine.h"
#include <string.h>
#include <stdio.h>

static int letter(unsigned char c)
{
    return (c>='A'&&c<='Z')||(c>='a'&&c<='z')||
           (c>='0'&&c<='9');
}
static int lower(int c)
{
    return (c>='A'&&c<='Z')?c+('a'-'A'):c;
}
static void normalize(const char *s,size_t n,char *out,size_t cap)
{
    size_t i=0,j=0;
    int pending=0;
    if(!cap)return;
    while(i<n&&s[i]&&j+2<cap){
        unsigned char c=(unsigned char)s[i++];
        if(c==0xc3 && i<n){
            unsigned char x=(unsigned char)s[i++];
            if(x==0xa1||x==0x81||x==0xa0||x==0x80||x==0xa4||x==0x84)c='a';
            else if(x==0xa9||x==0x89||x==0xa8||x==0x88||x==0xab||x==0x8b)c='e';
            else if(x==0xad||x==0x8d||x==0xaf||x==0x8f)c='i';
            else if(x==0xb3||x==0x93||x==0xb6||x==0x96)c='o';
            else if(x==0xba||x==0x9a||x==0xbc||x==0x9c)c='u';
            else if(x==0xb1||x==0x91)c='n';
            else {pending=1;continue;}
        }
        if(c=='&' && i<n){ /* basic HTML entities in titles */
            if(i+4<n && memcmp(s+i,"amp;",4)==0) {i+=4;c=' ';}
            else if(i+5<n && memcmp(s+i,"quot;",5)==0) {i+=5;c=' ';}
            else {pending=1;continue;}
        }
        if(letter(c)){
            if(pending&&j)out[j++]=' ';
            out[j++]=(char)lower(c);
            pending=0;
        } else if(j)pending=1;
    }
    out[j]=0;
}
static int good_host(const char *u)
{
    const char *p,*e;
    if(!u||strncmp(u,"https://es.web.img",18)!=0)return 0;
    p=u+18;
    if(*p<'1'||*p>'9')return 0;
    p++;
    if(strncmp(p,".acsta.net/",11)!=0)return 0;
    e=strrchr(p,'/');
    return e && strstr(e,".jpg") && strlen(e)>4;
}
/* Exact names take priority. Only accept suffix aliases for cases such
 * as "La leyenda del samurái - 47 Ronin" versus ISO "47 Ronin".
 * Do not confuse Avatar with Avatar 2, Thor with Thor: Ragnarok, etc. */
static int title_score(const char *label,const char *req)
{
    size_t a=strlen(label),b=strlen(req);
    if(!a||!b)return 0;
    if(!strcmp(label,req))return 100;
    if(a>b && a>=b && label[a-b-1]==' ' &&
       !strcmp(label+a-b,req))return 70;
    return 0;
}
int movie_sensacine_pick(const char *page,const char *title,char *url,size_t cap)
{
    const char *p,*best=NULL,*best_end=NULL;
    int best_score=0;
    char norm_request[220];
    size_t page_limit;
    if(!url||!cap)return 0;
    url[0]=0;
    if(!page||!title||!title[0])return 0;
    normalize(title,strlen(title),norm_request,sizeof(norm_request));
    page_limit=strlen(page);
    p=page;
    while(p && (p=strstr(p,"alt=\"Cartel de "))!=NULL){
        const char *start=p+14,*end=strchr(start,'"'),*tag=p,*tagend;
        char label[300],norm_label[300];
        int score;
        size_t len;
        if(!end || (size_t)(end-start)>=sizeof(label)) {p+=14;continue;}
        len=(size_t)(end-start);
        memcpy(label,start,len);label[len]=0;
        normalize(label,len,norm_label,sizeof(norm_label));
        score=title_score(norm_label,norm_request);
        if(score>best_score){
            /* All attributes must come from THIS img tag. HTML around it
             * (like recommendations) is explicitly not trusted. */
            size_t back=0;
            while(tag>page && back<1024){
                if(tag[0]=='<' && tag[1]=='i' &&
                   tag[2]=='m' && tag[3]=='g')break;
                tag--;back++;
            }
            tagend=strchr(tag,'>');
            if(back<1024 && tagend && tagend>=end && tagend-tag<1800){
                const char *img=strstr(tag,"data-src=\"");
                if(img && img<tagend) img+=10;
                else {
                    img=strstr(tag,"src=\"");
                    if(img && img<tagend)img+=5;
                    else img=NULL;
                }
                if(img && img<tagend){
                    const char *imgend=strchr(img,'"');
                    if(imgend && imgend<=tagend && imgend-img<1200){
                        char candidate[1200];
                        size_t clen=(size_t)(imgend-img);
                        memcpy(candidate,img,clen);candidate[clen]=0;
                        if(good_host(candidate) && clen<cap){
                            best=img;best_end=imgend;best_score=score;
                            if(score==100)break;
                        }
                    }
                }
            }
        }
        p=end+1;
        if((size_t)(p-page)>=page_limit)break;
    }
    if(!best||!best_end)return 0;
    memcpy(url,best,(size_t)(best_end-best));
    url[best_end-best]=0;
    return 1;
}
int movie_sensacine_fullsize(const char *thumb,char *url,size_t cap)
{
    const char *host,*path,*after;
    size_t prefix,suffix;
    if(!url||cap==0)return 0;
    url[0]=0;
    if(!good_host(thumb))return 0;
    host=strstr(thumb,".acsta.net/");
    if(!host)return 0;
    path=host+10;
    if(strncmp(path,"/c_",3)!=0)return 0;
    after=strchr(path+1,'/');
    if(!after)return 0;
    prefix=(size_t)(path-thumb);
    suffix=strlen(after);
    if(prefix+suffix+1>cap)return 0;
    memcpy(url,thumb,prefix);
    memcpy(url+prefix,after,suffix+1);
    return good_host(url);
}
