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
int sf_bdiso_ensure_art(const char *iso_path)
{
    char dir[MOVIE_PATH_MAX],stem[256],cover[MOVIE_PATH_MAX],titlepath[MOVIE_PATH_MAX];
    char keypath[MOVIE_PATH_MAX],key[80],query[240],encoded[560],poster[120],title[256];
    char url[MOVIE_URL_MAX];
    const char *slash;
    unsigned char *json=NULL,*jpeg=NULL;
    uint32_t n=0;
    int year=0,movie_id=0,is3d,status=-1,i,key_len;
    if(!iso_path || strncmp(iso_path,"/dev_usb00",10)!=0 ||
       iso_path[10]<'0'||iso_path[10]>'7')return -1;
    if(attempted(iso_path))return -2;
    slash=strrchr(iso_path,'/');
    if(!slash || slash-iso_path>=(int)sizeof(dir)-20)return -3;
    memcpy(dir,iso_path,(size_t)(slash-iso_path));dir[slash-iso_path]='\0';
    if(!strstr(dir,"/BDISO"))return -3;
    iso_stem(iso_path,stem,sizeof(stem));
    if(!stem[0])return -3;
    is3d=movie_3d(stem);
    if(snprintf(keypath,sizeof(keypath),"%s/COVERS/TMDB_KEY.TXT",dir)>=sizeof(keypath))return -4;
    key_len=ps3_read_at(NULL,keypath,0,key,sizeof(key)-1);
    if(key_len<=0)return -5; /* No API key, manual covers remain supported. */
    key[key_len]=0;
    while(key_len>0 && (key[key_len-1]=='\n'||key[key_len-1]=='\r'||key[key_len-1]==' '))
        key[--key_len]=0;
    if(key_len!=32)return -6;
    for(i=0;i<key_len;i++)if(!isxdigit((unsigned char)key[i]))return -6;
    if(snprintf(cover,sizeof(cover),"%s/COVERS/%s.jpg",dir,stem)>=sizeof(cover) ||
       snprintf(titlepath,sizeof(titlepath),"%s/COVERS/%s.title",dir,stem)>=sizeof(titlepath))return -4;
    movie_query(stem,query,sizeof(query),&year);
    if(!query[0] || encode_query(query,encoded,sizeof(encoded))!=0)return -7;

    json=(unsigned char*)malloc(MOVIE_JSON_MAX+1);
    if(!json)return -8;
    if(year) {
        if(snprintf(url,sizeof(url),
           "https://api.themoviedb.org/3/search/movie?api_key=%s&language=es-MX&include_adult=false&query=%s&year=%d",
           key,encoded,year)>=(int)sizeof(url))goto done;
    }else{
        if(snprintf(url,sizeof(url),
           "https://api.themoviedb.org/3/search/movie?api_key=%s&language=es-MX&include_adult=false&query=%s",
           key,encoded)>=(int)sizeof(url))goto done;
    }
    if(remote_get(url,json,MOVIE_JSON_MAX,&n)!=0 || !n)goto done;
    json[n]=0;
    if(!movie_title_id((char*)json,title,sizeof(title),poster,sizeof(poster),&movie_id))goto done;
    /* Spanish metadata is independently useful even when poster servers
       fail, and never modifies the original English ISO filename. */
    if(snprintf(titlepath,sizeof(titlepath),"%s/COVERS/%s.title",dir,stem)
       <(int)sizeof(titlepath)){
        char folder[MOVIE_PATH_MAX];
        snprintf(folder,sizeof(folder),"%s/COVERS",dir);
        if(sf_bdntfs_has_port(iso_path[10]-'0')) {
            (void)sf_bdntfs_mkdir(folder);
            (void)sf_bdntfs_write(titlepath,title,(uint32_t)strlen(title),"\n",1);
        }else{
            extern int sf_bdiso_fat_mkdir(const char *);
            (void)sf_bdiso_fat_mkdir(folder);
            (void)ps3_fs_write_file(titlepath,title,(uint32_t)strlen(title));
        }
    }

    if(snprintf(url,sizeof(url),
       "https://api.themoviedb.org/3/movie/%d/images?api_key=%s&language=es-MX&include_image_language=es,null",
       movie_id,key)<sizeof(url)){
        uint32_t nn=0;
        if(remote_get(url,json,MOVIE_JSON_MAX,&nn)==0 && nn){
            json[nn]=0;
            (void)poster_from_images((char*)json,poster,sizeof(poster));
        }
    }
    if(poster[0]!='/' || strlen(poster)>90)goto done;
    if(snprintf(url,sizeof(url),"https://image.tmdb.org/t/p/w500%s",poster)>=sizeof(url))goto done;
    jpeg=(unsigned char*)malloc(MOVIE_JPEG_MAX);
    if(!jpeg)goto done;
    if(remote_get(url,jpeg,MOVIE_JPEG_MAX,&n)!=0 || n<512 ||
       jpeg[0]!=0xff || jpeg[1]!=0xd8)goto done;
    /* Create COVERS on the source drive; write both picture and title there.
       No PS3 internal permanent image storage, including for 3D films. */
    {
        char folder[MOVIE_PATH_MAX];
        int ok;
        snprintf(folder,sizeof(folder),"%s/COVERS",dir);
        if(sf_bdntfs_has_port(iso_path[10]-'0'))(void)sf_bdntfs_mkdir(folder);
        else { /* FAT32 device folder creation */
            extern int sf_bdiso_fat_mkdir(const char *);
            (void)sf_bdiso_fat_mkdir(folder);
        }
        if(sf_bdntfs_has_port(iso_path[10]-'0')) {
            ok=sf_bdntfs_write(cover,jpeg,n,NULL,0);
            if(ok==0)(void)sf_bdntfs_write(titlepath,title,(uint32_t)strlen(title),"\n",1);
        }else{
            ok=ps3_fs_write_file(cover,jpeg,n);
            if(ok==0)(void)ps3_fs_write_file(titlepath,title,(uint32_t)strlen(title));
        }
        if(ok==0) {
            log_printf("bdiso: poster saved on external USB, 3D=%d, movie id=%d",is3d,movie_id);
            status=0;
        }
    }
done:
    free(jpeg);free(json);
    return status;
}
