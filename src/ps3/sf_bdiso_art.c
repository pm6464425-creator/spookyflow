/* Movie artwork: background-worker-only network lookup.
 * TMDB is a movie/poster service, not a Blu-ray edition database.
 * 3D filenames keep their own artwork path, and posters tagged 3D
 * should be provided manually if an exact 3D-edition cover is required. */
#include <ctype.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "ps3_platform.h"
#include "sf_bdiso_ntfs.h"
#include "sf_bdiso_art.h"
#include "log.h"
#include "util.h"

#define MOVIE_JSON_MAX (160u * 1024u)
#define MOVIE_JPEG_MAX (2u * 1024u * 1024u)
#define MOVIE_URL_MAX  640
#define MOVIE_PATH_MAX 512
static uint64_t s_attempted[64];
static unsigned s_attempt_index;

static uint64_t hash_key(const char *s)
{
    uint64_t h=1469598103934665603ULL;
    while(*s) { h^=(unsigned char)*s++; h*=1099511628211ULL; }
    return h ? h : 1;
}
static int attempted(const char *path)
{
    uint64_t h=hash_key(path);
    unsigned i;
    for(i=0;i<64;i++) if(s_attempted[i]==h) return 1;
    s_attempted[(s_attempt_index++)%64]=h;
    return 0;
}
static int movie_3d(const char *s)
{
    for(;s[0]&&s[1];s++) {
        if(s[0]=='3' && (s[1]=='d'||s[1]=='D')) return 1;
    }
    return 0;
}
static void iso_stem(const char *path,char *out,size_t cap)
{
    const char *p=strrchr(path,'/');
    size_t n;
    p=p?p+1:path;
    snprintf(out,cap,"%s",p);
    n=strlen(out);
    if(n>6 && sf_ends_with_i(out,".iso.0"))out[n-6]='\0';
    else if(n>4 && sf_ends_with_i(out,".iso"))out[n-4]='\0';
}
static void movie_query(const char *stem,char *dst,size_t cap,int *year)
{
    char clean[256],word[64];
    size_t o=0,i=0,j=0,wi=0;
    int y=0;
    *year=0; clean[0]='\0';
    /* Dots, underscores and punctuation in release names are separators. */
    for(i=0;stem[i]&&o+2<sizeof(clean);i++){
        unsigned char c=(unsigned char)stem[i];
        if(isalnum(c) || c>=128)clean[o++]=(char)c;
        else clean[o++]=' ';
    }
    clean[o]='\0';o=0;i=0;
    while(clean[i]){
        while(clean[i]==' ')i++;
        wi=0;
        while(clean[i]&&clean[i]!=' '){
            if(wi+1<sizeof(word))word[wi++]=clean[i];
            i++;
        }
        word[wi]='\0';
        if(!wi)continue;
        if(wi==4 && (word[0]=='1' || word[0]=='2')){
            y=atoi(word);
            if(y>=1920 && y<=2035){*year=y;continue;}
        }
        if(!sf_stricmp(word,"3D")||!sf_stricmp(word,"BD3D")||
           !sf_stricmp(word,"BLURAY")||!sf_stricmp(word,"BLU")||
           !sf_stricmp(word,"RAY")||!sf_stricmp(word,"BDISO")||
           !sf_stricmp(word,"REMUX")||!sf_stricmp(word,"1080P")||
           !sf_stricmp(word,"2160P")||!sf_stricmp(word,"720P")||
           !sf_stricmp(word,"SBS")||!sf_stricmp(word,"HSBS")||
           !sf_stricmp(word,"MVC")||!sf_stricmp(word,"DTS")||
           !sf_stricmp(word,"TRUEHD"))continue;
        if(o&&o+1<cap)dst[o++]=' ';
        for(j=0;j<wi && o+1<cap;j++)dst[o++]=word[j];
    }
    dst[o]='\0';
}
static int encode_query(const char *src,char *dst,size_t cap)
{
    static const char hex[]="0123456789ABCDEF";
    size_t n=0;
    while(*src){
        unsigned char c=(unsigned char)*src++;
        if(isalnum(c)||c=='-'||c=='_'||c=='.'){
            if(n+2>=cap)return -1;
            dst[n++]=(char)c;
        }else {
            if(n+4>=cap)return -1;
            dst[n++]='%'; dst[n++]=hex[c>>4];dst[n++]=hex[c&15];
        }
    }
    dst[n]=0;return 0;
}
static int remote_get(const char *url,unsigned char *buf,uint32_t cap,uint32_t *len)
{
    char location[MOVIE_URL_MAX];
    char current[MOVIE_URL_MAX];
    int n,code=0;
    snprintf(current,sizeof(current),"%s",url);
    for(n=0;n<3;n++){
        location[0]=0;*len=0;
        code=ps3_http_get(NULL,current,buf,cap,len,location,sizeof(location));
        if(code==200)return 0;
        if((code==301||code==302||code==307||code==308) &&
            !strncmp(location,"https://",8) && strlen(location)<sizeof(current)){
            strcpy(current,location);continue;
        }
        break;
    }
    return code?code:-1;
}
static const char *json_field(const char *obj,const char *end,const char *key)
{
    char field[80];
    const char *p;
    snprintf(field,sizeof(field),"\"%s\"",key);
    p=strstr(obj,field);
    if(!p || p>=end)return NULL;
    p+=strlen(field);
    while(p<end && (*p==' '||*p==':'||*p=='\t'))p++;
    return p<end?p:NULL;
}
/* JSON string decoder: retain Spanish accents from TMDB's \uXXXX notation.
 * Inputs are bounded by the enclosing parsed JSON object and destination. */
static int json_hex4(const char *s, const char *end, unsigned *v)
{
    unsigned x=0;
    int k;
    if(end-s<4)return 0;
    for(k=0;k<4;k++){
        int c=(unsigned char)s[k],d;
        if(c>='0'&&c<='9')d=c-'0';
        else if(c>='a'&&c<='f')d=c-'a'+10;
        else if(c>='A'&&c<='F')d=c-'A'+10;
        else return 0;
        x=(x<<4)|(unsigned)d;
    }
    *v=x; return 1;
}
static int json_utf8(char *out,size_t cap,size_t *n,unsigned cp)
{
    unsigned char bytes[4];
    int count=0,k;
    if(cp<=0x7f){bytes[0]=(unsigned char)cp;count=1;}
    else if(cp<=0x7ff){
        bytes[0]=(unsigned char)(0xc0|(cp>>6));
        bytes[1]=(unsigned char)(0x80|(cp&63));count=2;
    }else if(cp<=0xffff && !(cp>=0xd800&&cp<=0xdfff)){
        bytes[0]=(unsigned char)(0xe0|(cp>>12));
        bytes[1]=(unsigned char)(0x80|((cp>>6)&63));
        bytes[2]=(unsigned char)(0x80|(cp&63));count=3;
    }else if(cp<=0x10ffff && cp>=0x10000){
        bytes[0]=(unsigned char)(0xf0|(cp>>18));
        bytes[1]=(unsigned char)(0x80|((cp>>12)&63));
        bytes[2]=(unsigned char)(0x80|((cp>>6)&63));
        bytes[3]=(unsigned char)(0x80|(cp&63));count=4;
    } else return 0;
    if(*n+(size_t)count>=cap)return 0;
    for(k=0;k<count;k++)out[(*n)++]=(char)bytes[k];
    return 1;
}
static int json_text(const char *obj,const char *end,const char *key,char *out,size_t cap)
{
    const char *p=json_field(obj,end,key);
    size_t i=0;
    if(!p||*p!='"'||cap==0)return 0;
    p++;
    while(p<end && *p!='"' && i+1<cap){
        if(*p=='\\' && p+1<end) {
            p++;
            if(*p=='n'||*p=='t'||*p=='r'){out[i++]=' ';p++;continue;}
            if(*p=='u'){
                unsigned cp,low;
                if(!json_hex4(p+1,end,&cp))break;
                p+=5;
                if(cp>=0xd800&&cp<=0xdbff && end-p>=6 &&
                   p[0]=='\\'&&p[1]=='u' &&
                   json_hex4(p+2,end,&low) && low>=0xdc00&&low<=0xdfff){
                    cp=0x10000+((cp-0xd800)<<10)+(low-0xdc00);
                    p+=6;
                }
                if(!json_utf8(out,cap,&i,cp))break;
                continue;
            }
            if(*p=='"'||*p=='\\'||*p=='/') {out[i++]=*p++;continue;}
            break;
        }
        out[i++]=*p++;
    }
    out[i]=0;
    return i>0;
}

static int first_object(const char *json,const char *array_name,
                        const char **start,const char **end)
{
    char pat[56];
    const char *p,*q;
    int depth=0,quoted=0,escaped=0;
    snprintf(pat,sizeof(pat),"\"%s\"",array_name);
    p=strstr(json,pat);
    if(!p)return 0;
    p=strchr(p,'[');if(!p)return 0;
    p=strchr(p,'{');if(!p)return 0;
    *start=p;
    for(q=p;*q;q++){
        char c=*q;
        if(escaped){escaped=0;continue;}
        if(c=='\\'&&quoted){escaped=1;continue;}
        if(c=='"'){quoted=!quoted;continue;}
        if(quoted)continue;
        if(c=='{')depth++;
        if(c=='}' && --depth==0){*end=q+1;return 1;}
    }
    return 0;
}
static int movie_title_id(const char *json,char *title,size_t tcap,
                          char *poster,size_t pcap,int *id)
{
    const char *a,*b,*v;
    if(!first_object(json,"results",&a,&b))return 0;
    if(!json_text(a,b,"title",title,tcap))return 0;
    if(!json_text(a,b,"poster_path",poster,pcap))poster[0]=0;
    v=json_field(a,b,"id");
    if(!v||!isdigit((unsigned char)*v))return 0;
    *id=atoi(v);
    return *id>0;
}
static int poster_from_images(const char *json,char *out,size_t cap)
{
    const char *p=strstr(json,"\"posters\"");
    const char *arr;
    if(!p || !(arr=strchr(p,'['))) return 0;
    for(p=arr+1;*p && p-arr<110000;){
        const char *end;
        char lang[12],filename[100];
        p=strchr(p,'{');
        if(!p)break;
        end=strchr(p,'}');
        if(!end)break;
        /* file_path may precede iso_639_1 in TMDB's JSON; inspect the
           whole object, not only the suffix after the language field. */
        if(json_text(p,end,"iso_639_1",lang,sizeof(lang)) &&
           strcmp(lang,"es")==0 &&
           json_text(p,end,"file_path",filename,sizeof(filename)) &&
           filename[0]=='/'){
            snprintf(out,cap,"%s",filename);
            return 1;
        }
        p=end+1;
    }
    return 0;
}

/* The external USB is the only permanent artwork and provider-key store.
 * Providers are optional: invalid/missing keys simply disable that source.
 * No search-engine or forum pages are scraped; community searches are
 * written as review links for users to choose the exact 3D edition. */
static int movie_key(const char *dir,const char *name,char *out,size_t cap)
{
    char path[MOVIE_PATH_MAX];int n,i;
    if(cap<3 || snprintf(path,sizeof(path),"%s/COVERS/%s",dir,name)>=(int)sizeof(path))return 0;
    n=ps3_read_at(NULL,path,0,out,(uint32_t)cap-1);
    if(n<=0 || (size_t)n>=cap)return 0;
    out[n]=0;
    while(n>0 && (out[n-1]=='\r'||out[n-1]=='\n'||out[n-1]==' '||out[n-1]=='\t'))out[--n]=0;
    for(i=0;i<n;i++){
        unsigned char c=(unsigned char)out[i];
        if(!isalnum(c) && c!='_' && c!='-')return 0;
    }
    return n;
}
static int movie_covers_dir(const char *dir,int port)
{
    char dest[MOVIE_PATH_MAX];
    if(snprintf(dest,sizeof(dest),"%s/COVERS",dir)>=(int)sizeof(dest))return -1;
    if(sf_bdntfs_has_port(port)){
        if(sf_bdntfs_is_dir(dest))return 0;
        return sf_bdntfs_mkdir(dest);
    }
    if(ps3_fs_exists(dest))return 0;
    {
        extern int sf_bdiso_fat_mkdir(const char *);
        return sf_bdiso_fat_mkdir(dest);
    }
}
static int movie_usb_write(const char *path,int port,const void *buf,uint32_t size)
{
    if(!buf || !size)return -1;
    if(sf_bdntfs_has_port(port))return sf_bdntfs_write(path,buf,size,NULL,0);
    return ps3_fs_write_file(path,buf,size);
}
static int movie_http_json(const char *url,unsigned char *buf,uint32_t *len)
{
    if(remote_get(url,buf,MOVIE_JSON_MAX,len)!=0 || *len==0)return 0;
    buf[*len]=0;
    return 1;
}
static int movie_http_jpeg(const char *url,unsigned char *buf,uint32_t *len)
{
    if(strncmp(url,"https://",8)!=0 || remote_get(url,buf,MOVIE_JPEG_MAX,len)!=0)
        return 0;
    return *len>=512 && buf[0]==0xff && buf[1]==0xd8 && buf[2]==0xff;
}
static int movie_from_fanart(const char *json,char *url,size_t cap)
{
    const char *a=strstr(json,"\"movieposter\""),*end;
    char chosen[MOVIE_URL_MAX]="";int best=-1;
    if(!a || !(a=strchr(a,'[')) || !(end=strchr(a,']')))return 0;
    while((a=strchr(a,'{')) && a<end){
        const char *stop=strchr(a,'}');
        char found[MOVIE_URL_MAX],lang[16];int score;
        if(!stop || stop>=end)break;
        if(json_text(a,stop,"url",found,sizeof(found)) &&
           strncmp(found,"https://assets.fanart.tv/",25)==0){
            lang[0]=0;(void)json_text(a,stop,"lang",lang,sizeof(lang));
            score=!sf_stricmp(lang,"es")?3:
                  (!sf_stricmp(lang,"00")||!sf_stricmp(lang,"en"))?2:1;
            if(score>best){snprintf(chosen,sizeof(chosen),"%s",found);best=score;}
        }
        a=stop+1;
    }
    if(best<0 || strlen(chosen)+1>cap)return 0;
    strcpy(url,chosen);
    return 1;
}
static int movie_imdb_id(const char *json,char *out,size_t cap)
{
    size_t i;const char *p=json;
    if(!json_text(p,p+strlen(p),"imdb_id",out,cap) &&
       !json_text(p,p+strlen(p),"imdbID",out,cap))return 0;
    if(strlen(out)<4 || out[0]!='t' || out[1]!='t')return 0;
    for(i=2;out[i];i++)if(!isdigit((unsigned char)out[i]))return 0;
    return 1;
}
/* Community pages usually have real Blu-ray release scans but no documented,
 * stable, anonymous poster API. We provide useful searches instead of
 * silently selecting a random image from a protected forum. */
static void movie_community_sources(const char *dir,const char *stem,
                                    const char *title,int year,int port)
{
    char path[MOVIE_PATH_MAX],enc[560],text[2400],query[260];
    int n;
    if(snprintf(path,sizeof(path),"%s/COVERS/%s.sources.txt",dir,stem)>=(int)sizeof(path) ||
       ps3_fs_is_file(NULL,path))return;
    if(year)snprintf(query,sizeof(query),"%s %d 3D Blu-ray",title,year);
    else snprintf(query,sizeof(query),"%s 3D Blu-ray",title);
    if(encode_query(query,enc,sizeof(enc)))return;
    n=snprintf(text,sizeof(text),
        "BÚSQUEDA DE CARÁTULA BLU-RAY 3D - %s\n"
        "Estas páginas son referencias de búsqueda, NO descargas automáticas.\n"
        "Si encuentras la edición correcta, coloca su JPG con el nombre exacto:\n"
        "BDISO/COVERS/%s.jpg\n\n"
        "1) Catálogo de ediciones Blu-ray.com:\n"
        "https://www.google.com/search?q=site%%3Ablu-ray.com+%s\n\n"
        "2) Foro de diseños Blu-ray.com:\n"
        "https://www.google.com/search?q=site%%3Aforum.blu-ray.com+%s\n\n"
        "3) Comunidad CoverCity:\n"
        "https://www.google.com/search?q=site%%3Acovercity.net+%s\n\n"
        "4) Foro Swarzycustom de carátulas 3D:\n"
        "https://www.google.com/search?q=site%%3Aswarzycustom.com+%s\n\n"
        "Verifica país, año y edición 3D. Respeta los permisos de cada sitio.\n",
        stem,stem,enc,enc,enc,enc);
    if(n>0 && (size_t)n<sizeof(text))
        (void)movie_usb_write(path,port,text,(uint32_t)n);
}

/* TMDB, fanart.tv, OMDb and MoviePosterDB are independent poster sources.
 * Only URLs from each service's documented image hosts are accepted. */
typedef struct { char url[MOVIE_URL_MAX]; const char *provider; } MovieCandidate;
static void movie_candidate(MovieCandidate *v,int *count,int max,const char *url,
                            const char *name)
{
    if(*count<max && url && url[0] && strncmp(url,"https://",8)==0 &&
       strlen(url)<MOVIE_URL_MAX){
        snprintf(v[*count].url,sizeof(v[*count].url),"%s",url);
        v[(*count)++].provider=name;
    }
}

int sf_bdiso_ensure_art(const char *iso_path)
{
    char dir[MOVIE_PATH_MAX],stem[256],cover[MOVIE_PATH_MAX],titlepath[MOVIE_PATH_MAX];
    char tmdb[80]="",fanart[100]="",omdb[100]="",mpdb[140]="";
    char query[240],encoded[560],title[256]="",poster[120]="",poster_es[120]="";
    char imdb[40]="",url[MOVIE_URL_MAX],tmp[MOVIE_URL_MAX],keypath[MOVIE_PATH_MAX];
    const char *slash;
    unsigned char *json=NULL,*jpeg=NULL;
    MovieCandidate candidates[6];
    uint32_t n=0;
    int year=0,id=0,is3d,port,status=-1,count=0,i,k;
    if(!iso_path || strncmp(iso_path,"/dev_usb00",10)!=0 ||
       iso_path[10]<'0'||iso_path[10]>'7'||iso_path[11]!='/')return -1;
    if(attempted(iso_path))return -2;
    port=iso_path[10]-'0';
    slash=strrchr(iso_path,'/');
    if(!slash || (size_t)(slash-iso_path)>=sizeof(dir)-20)return -3;
    memcpy(dir,iso_path,(size_t)(slash-iso_path));dir[slash-iso_path]=0;
    if(!strstr(dir,"/BDISO"))return -3;
    iso_stem(iso_path,stem,sizeof(stem));
    if(!stem[0] ||
       snprintf(cover,sizeof(cover),"%s/COVERS/%s.jpg",dir,stem)>=(int)sizeof(cover) ||
       snprintf(titlepath,sizeof(titlepath),"%s/COVERS/%s.title",dir,stem)>=(int)sizeof(titlepath))
       return -4;
    /* Manual art always wins. The worker invokes us only when art is missing. */
    if(ps3_fs_is_file(NULL,cover))return 1;
    movie_query(stem,query,sizeof(query),&year);
    if(!query[0] || encode_query(query,encoded,sizeof(encoded))!=0)return -5;
    is3d=movie_3d(stem);
    k=movie_key(dir,"TMDB_KEY.TXT",tmdb,sizeof(tmdb));
    if(k!=32)tmdb[0]=0;
    else for(i=0;i<32;i++)if(!isxdigit((unsigned char)tmdb[i])){tmdb[0]=0;break;}
    if(!movie_key(dir,"FANART_KEY.TXT",fanart,sizeof(fanart)))fanart[0]=0;
    if(!movie_key(dir,"OMDB_KEY.TXT",omdb,sizeof(omdb)))omdb[0]=0;
    if(!movie_key(dir,"MOVIEPOSTERDB_KEY.TXT",mpdb,sizeof(mpdb)) ||
       strncmp(mpdb,"mpdb_pk_",8)!=0)mpdb[0]=0;

    /* The 3D exact-edition review list is useful even when API credentials
       are absent. It never changes the ISO or any manually supplied cover. */
    if(is3d && movie_covers_dir(dir,port)==0)
        movie_community_sources(dir,stem,query,year,port);
    if(!tmdb[0] && !omdb[0]) {
        log_printf("bdiso: no TMDB/OMDb metadata key; use COVERS keys or manual artwork");
        return -6;
    }
    json=(unsigned char*)malloc(MOVIE_JSON_MAX+1);
    jpeg=(unsigned char*)malloc(MOVIE_JPEG_MAX);
    if(!json || !jpeg){status=-8;goto done;}

    /* TMDB identity + Spanish poster. An edition named 3D is NOT assumed
       to have a specially branded poster in this standard film database. */
    if(tmdb[0]){
        int ok;
        if(year)ok=snprintf(url,sizeof(url),
          "https://api.themoviedb.org/3/search/movie?api_key=%s&language=es-MX&include_adult=false&query=%s&year=%d",
          tmdb,encoded,year);
        else ok=snprintf(url,sizeof(url),
          "https://api.themoviedb.org/3/search/movie?api_key=%s&language=es-MX&include_adult=false&query=%s",
          tmdb,encoded);
        if(ok>0 && ok<(int)sizeof(url) && movie_http_json(url,json,&n)){
            (void)movie_title_id((const char*)json,title,sizeof(title),
                                 poster,sizeof(poster),&id);
        }
        if(id>0){
            if(snprintf(url,sizeof(url),
               "https://api.themoviedb.org/3/movie/%d/images?api_key=%s&language=es-MX&include_image_language=es,null",
               id,tmdb)<(int)sizeof(url) && movie_http_json(url,json,&n)){
                (void)poster_from_images((const char*)json,poster_es,sizeof(poster_es));
            }
            if(snprintf(url,sizeof(url),
               "https://api.themoviedb.org/3/movie/%d/external_ids?api_key=%s",id,tmdb)
               <(int)sizeof(url) && movie_http_json(url,json,&n)){
                (void)movie_imdb_id((const char*)json,imdb,sizeof(imdb));
            }
        }
    }
    /* OMDb also resolves titles independently when TMDB is unavailable. */
    if(omdb[0]){
        int ok;
        if(imdb[0])ok=snprintf(url,sizeof(url),
          "https://www.omdbapi.com/?apikey=%s&i=%s&plot=short",omdb,imdb);
        else if(year)ok=snprintf(url,sizeof(url),
          "https://www.omdbapi.com/?apikey=%s&t=%s&y=%d&type=movie",omdb,encoded,year);
        else ok=snprintf(url,sizeof(url),
          "https://www.omdbapi.com/?apikey=%s&t=%s&type=movie",omdb,encoded);
        if(ok>0 && ok<(int)sizeof(url) && movie_http_json(url,json,&n)){
            if(!imdb[0])(void)movie_imdb_id((const char*)json,imdb,sizeof(imdb));
            if(!title[0])(void)json_text((const char*)json,(const char*)json+n,
                                        "Title",title,sizeof(title));
            if(json_text((const char*)json,(const char*)json+n,"Poster",tmp,sizeof(tmp)) &&
               (strncmp(tmp,"https://m.media-amazon.com/",26)==0 ||
                strncmp(tmp,"https://ia.media-imdb.com/",26)==0))
                movie_candidate(candidates,&count,6,tmp,"OMDb");
        }
    }
    /* TMDB Spanish first, then 3D-aware manual art, then fallbacks.
       A normal poster is a fallback only, never claimed as an exact 3D edition. */
    if(poster_es[0]=='/' && strlen(poster_es)<100){
        snprintf(url,sizeof(url),"https://image.tmdb.org/t/p/w500%s",poster_es);
        movie_candidate(candidates,&count,6,url,"TMDB-ES");
    }
    /* fanart.tv's posters are linked by TMDB movie ID. API keys are optional. */
    if(fanart[0] && id>0 &&
       snprintf(url,sizeof(url),
         "https://webservice.fanart.tv/v3.2/movies/%d?api_key=%s",id,fanart)
       <(int)sizeof(url) && movie_http_json(url,json,&n) &&
       movie_from_fanart((const char*)json,tmp,sizeof(tmp)))
        movie_candidate(candidates,&count,6,tmp,"Fanart.tv");

    /* MoviePosterDB provides a documented stable image URL by IMDb id.
       Its FREE tier may be only 200px; higher-quality images need the
       owner's own plan. We never invent or embed any vendor API key. */
    if(mpdb[0] && imdb[0]){
        int v=snprintf(url,sizeof(url),
          "https://posters.movieposterdb.com/m/%s?k=%s&s=s&cc=ES",
          imdb,mpdb);
        if(v>0 && v<(int)sizeof(url))
            movie_candidate(candidates,&count,6,url,"MoviePosterDB");
    }
    if(poster[0]=='/' && strlen(poster)<100){
        snprintf(url,sizeof(url),"https://image.tmdb.org/t/p/w500%s",poster);
        movie_candidate(candidates,&count,6,url,"TMDB-default");
    }
    if(count==0){status=-9;goto save_title;}
    for(i=0;i<count;i++){
        uint32_t bytes=0;
        if(!movie_http_jpeg(candidates[i].url,jpeg,&bytes))continue;
        if(movie_covers_dir(dir,port)!=0)continue;
        if(ps3_fs_is_file(NULL,cover)){status=1;break;}
        if(movie_usb_write(cover,port,jpeg,bytes)==0){
            log_printf("bdiso: poster source=%s, 3D=%d (edition not authenticated)",
                       candidates[i].provider,is3d);
            status=0;break;
        }
    }
save_title:
    if(title[0] && !ps3_fs_is_file(NULL,titlepath) &&
       movie_covers_dir(dir,port)==0)
        (void)movie_usb_write(titlepath,port,title,(uint32_t)strlen(title));
done:
    free(jpeg);free(json);
    return status;
}
