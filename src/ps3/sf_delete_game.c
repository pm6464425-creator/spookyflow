/* MI PS3: selected user game removal. Self-contained, no launch or cache changes.
 * Never delete from /dev_hdd0/game, /dev_hdd0/tmp, NTFS pointer paths or unknown roots.
 */
#include "sf_delete_game.h"
#include "sf_delete_policy.h"
#include "ps3_platform.h"
#include "log.h"
#include <sys/file.h>
#include <stdio.h>
#include <string.h>

#define SF_DEL_IFMT 0170000
#define SF_DEL_IFDIR 0040000
#define SF_DEL_PATH 512
#define SF_DEL_CUE_SIZE 32768
#define SF_DEL_CUE_TRACKS 64
#define SF_DEL_TREE_LIMIT 100000u
static char s_del_cue_buf[SF_DEL_CUE_SIZE+1];
static char s_del_cue_tracks[SF_DEL_CUE_TRACKS][256];
static char s_del_cue_other[SF_DEL_CUE_TRACKS][256];

static int ends_ci(const char *s,const char *tail)
{
    size_t a=strlen(s),b=strlen(tail),i;
    if(a<b)return 0;
    for(i=0;i<b;i++){
        unsigned char x=(unsigned char)s[a-b+i],y=(unsigned char)tail[i];
        if(x>='A' && x<='Z')x=(unsigned char)(x+32);
        if(y>='A' && y<='Z')y=(unsigned char)(y+32);
        if(x!=y)return 0;
    }
    return 1;
}
static int same_ci(const char *a,const char *b)
{
    unsigned char x,y;
    for(;;a++,b++){
        x=(unsigned char)*a;y=(unsigned char)*b;
        if(x>='A' && x<='Z')x=(unsigned char)(x+32);
        if(y>='A' && y<='Z')y=(unsigned char)(y+32);
        if(x!=y)return 0;
        if(!x)return 1;
    }
}
static int basename_ok(const char *s)
{
    size_t n=0;
    if(!s || !*s || !strcmp(s,".") || !strcmp(s,".."))return 0;
    while(s[n]){
        unsigned char c=(unsigned char)s[n];
        if(c<32 || c==127 || c=='/' || c=='\\' || c==':')return 0;
        if(++n>240)return 0;
    }
    return 1;
}
static int regular_file(const char *p)
{
    sysFSStat st;
    return sysFsStat(p,&st)==0 && (st.st_mode&SF_DEL_IFMT)!=SF_DEL_IFDIR;
}
static int safe_join(char *out,size_t cap,const char *dir,const char *name)
{
    int n;
    if(!basename_ok(name))return 0;
    n=snprintf(out,cap,"%s/%s",dir,name);
    return n>0 && (size_t)n<cap;
}
/* Strict read-only parse: only FILE "track.bin" BINARY in the same directory.
 * All referenced tracks must be resolved before removing anything. */
static int cue_tracks(const char *cue,char names[][256],int max)
{
    sysFSStat st;
    s32 fd;
    u64 got=0;
    char *line;
    int n=0;
    if(sysFsStat(cue,&st)!=0 || st.st_size<12 || st.st_size>SF_DEL_CUE_SIZE)
        return -1;
    if(sysFsOpen(cue,SYS_O_RDONLY,&fd,NULL,0)!=0)return -1;
    if(sysFsRead(fd,s_del_cue_buf,(u64)st.st_size,&got)!=0 ||
       got!=(u64)st.st_size){
        sysFsClose(fd);
        return -1;
    }
    sysFsClose(fd);
    s_del_cue_buf[got]=0;
    line=s_del_cue_buf;
    while(*line){
        char *next=strchr(line,'\n'),*p=line,*start,*end;
        char filename[256];
        size_t len;
        int k,seen=0;
        if(next)*next=0;
        while(*p==' ' || *p=='\t')p++;
        if((p[0]=='F'||p[0]=='f') && (p[1]=='I'||p[1]=='i') &&
           (p[2]=='L'||p[2]=='l') && (p[3]=='E'||p[3]=='e') &&
           (p[4]==' ' || p[4]=='\t')){
            p+=4;
            while(*p==' ' || *p=='\t')p++;
            if(*p=='"'){
                start=++p;
                end=strchr(start,'"');
                if(!end)return -1;
            }else{
                start=p;end=p;
                while(*end && *end!=' ' && *end!='\t' && *end!='\r')end++;
            }
            len=(size_t)(end-start);
            if(!len || len>=sizeof(filename))return -1;
            memcpy(filename,start,len);filename[len]=0;
            if(!basename_ok(filename) ||
               !(ends_ci(filename,".bin") || ends_ci(filename,".img") ||
                 ends_ci(filename,".wav") || ends_ci(filename,".mp3")))
                return -1;
            for(k=0;k<n;k++)if(same_ci(names[k],filename)){seen=1;break;}
            if(!seen){
                if(n>=max)return -1;
                strcpy(names[n++],filename);
            }
        }
        if(!next)break;
        line=next+1;
    }
    return n>0?n:-1;
}
static int delete_cue_bundle(const char *cue)
{
    char dir[SF_DEL_PATH],track[SF_DEL_PATH],other[SF_DEL_PATH];
    const char *slash=strrchr(cue,'/');
    s32 fd;
    int n,i,k,ok=1;
    u64 got=0;
    sysFSDirent ent;
    if(!slash || slash==cue || (size_t)(slash-cue)>=sizeof(dir))return -2;
    memcpy(dir,cue,(size_t)(slash-cue));dir[slash-cue]=0;
    n=cue_tracks(cue,s_del_cue_tracks,SF_DEL_CUE_TRACKS);
    if(n<1)return -2;
    /* Preflight every track, refusing missing/foreign files. */
    for(i=0;i<n;i++){
        if(!safe_join(track,sizeof(track),dir,s_del_cue_tracks[i]) ||
           !regular_file(track))return -2;
    }
    /* A second CUE sharing a BIN is not safe to delete automatically. */
    if(sysFsOpendir(dir,&fd)!=0)return -2;
    while(sysFsReaddir(fd,&ent,&got)==0 && got){
        int m;
        if(!ends_ci(ent.d_name,".cue") || same_ci(ent.d_name,slash+1))continue;
        if(!safe_join(other,sizeof(other),dir,ent.d_name)){ok=0;break;}
        m=cue_tracks(other,s_del_cue_other,SF_DEL_CUE_TRACKS);
        if(m<0){ok=0;break;} /* uncertain reference: leave the target intact */
        for(i=0;i<n;i++)for(k=0;k<m;k++)
            if(same_ci(s_del_cue_tracks[i],s_del_cue_other[k]))ok=0;
        if(!ok)break;
    }
    sysFsClosedir(fd);
    if(!ok)return -2;
    /* Keep the CUE until all its tracks are removed; failure remains visible. */
    for(i=0;i<n;i++){
        if(!safe_join(track,sizeof(track),dir,s_del_cue_tracks[i]) ||
           sysFsUnlink(track)!=0)return -1;
    }
    return sysFsUnlink(cue)==0?0:-1;
}
/* Split .iso.0/.iso.1/... must be removed together (bounded 64 chunks).
 * The files are stat-checked first so a missing head never causes deletion. */
static int delete_split_iso(const char *path)
{
    char root[SF_DEL_PATH],part[SF_DEL_PATH];
    size_t n=strlen(path);
    int i,parts=0;
    if(n<6 || n-1>=sizeof(root))return -2;
    memcpy(root,path,n-1);root[n-1]=0; /* '.iso.0' -> '.iso.' */
    for(i=0;i<64;i++){
        sysFSStat st;
        if(snprintf(part,sizeof(part),"%s%d",root,i)>=(int)sizeof(part))
            return -2;
        if(sysFsStat(part,&st)!=0)break;
        if((st.st_mode&SF_DEL_IFMT)==SF_DEL_IFDIR)return -2;
        parts++;
    }
    if(parts==0 || parts==64)return -2;
    for(i=0;i<parts;i++){
        if(snprintf(part,sizeof(part),"%s%d",root,i)>=(int)sizeof(part) ||
           sysFsUnlink(part)!=0)return -1;
    }
    return 0;
}
/* Only selected PS3 game folders, never roots. Check every child name and
 * cap recursion and entry count so a corrupt tree cannot loop forever. */
static int delete_tree(const char *dir,int depth,unsigned *count)
{
    s32 fd;
    u64 got=0;
    sysFSDirent ent;
    int rc=0;
    if(depth>16 || *count>=SF_DEL_TREE_LIMIT)return -2;
    if(sysFsOpendir(dir,&fd)!=0)return -1;
    while(sysFsReaddir(fd,&ent,&got)==0 && got){
        char child[SF_DEL_PATH];
        sysFSStat st;
        if(!strcmp(ent.d_name,".") || !strcmp(ent.d_name,".."))continue;
        if(!safe_join(child,sizeof(child),dir,ent.d_name) ||
           sysFsStat(child,&st)!=0 || ++*count>SF_DEL_TREE_LIMIT){
            rc=-2;break;
        }
        if((st.st_mode&SF_DEL_IFMT)==SF_DEL_IFDIR)
            rc=delete_tree(child,depth+1,count);
        else rc=sysFsUnlink(child)==0?0:-1;
        if(rc!=0)break;
    }
    sysFsClosedir(fd);
    if(rc!=0)return rc;
    return sysFsRmdir(dir)==0?0:-1;
}
int sf_delete_game(int platform,const char *path,int is_dir)
{
    sysFSStat st;
    unsigned n=0;
    int rc=sf_delete_path_allowed(platform,path,is_dir);
    if(rc!=0)return rc;
    if(sysFsStat(path,&st)!=0)return -1;
    if(((st.st_mode&SF_DEL_IFMT)==SF_DEL_IFDIR)!=!!is_dir)return -2;
    if(is_dir){
        rc=delete_tree(path,0,&n);
    }else if(platform==PLAT_PS1 && ends_ci(path,".cue")){
        rc=delete_cue_bundle(path);
    }else if(ends_ci(path,".iso.0")){
        rc=delete_split_iso(path);
    }else{
        rc=sysFsUnlink(path)==0?0:-1;
    }
    log_printf("delete: platform=%d dir=%d rc=%d entries=%u path=%s",
               platform,is_dir,rc,n,path);
    return rc;
}
