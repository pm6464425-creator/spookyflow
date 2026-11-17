/* V15.42 SNES 16:9 screen mode for pSNES 7.2.
 * pSNES defaults to FIT (pillarboxed 4:3); EMULATION.SCALING="FULL"
 * makes its own C2DUIVideo::updateScaling() fill the screen.
 * The original pSNES saves global config at PSNE00001/USRDIR/config.cfg,
 * then loads configs/<ROM filename without extension>.cfg over it.
 * We modify one libconfig value only and do so atomically before spawn.
 * Never change or move ROMs, other game systems, saved games or controls.
 */
#include "sf_snes_screen.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>
#include <stdint.h>
#include <unistd.h>
#ifdef __PS3__
#include <sys/file.h>
#endif
#ifdef SF_VIDEO_ONLY_DIAG
#include "log.h"
#define VIDEO_CFG_LOG(...) log_printf(__VA_ARGS__)
#else
#define VIDEO_CFG_LOG(...) ((void)0)
#endif

#define SNES_CFG_HOME "/dev_hdd0/game/PSNE00001/USRDIR"
#define SNES_CFG_MAX (256u * 1024u)
#define SNES_CFG_EXTRA 64u

static int ident_start(unsigned char c)
{
    return isalpha(c) || c == '_';
}
static int ident_char(unsigned char c)
{
    return isalnum(c) || c == '_';
}

/* Skip whitespace, quoted strings, hash, line and block comments.
 * A malformed quoted string/comment is deliberately rejected. */
static size_t skip_token(const char *s, size_t n, size_t i)
{
    if (i >= n) return n;
    if (s[i] == '"') {
        ++i;
        while (i < n) {
            if (s[i] == '\\') { i += (i + 1 < n ? 2 : 1); continue; }
            if (s[i++] == '"') return i;
        }
        return n;
    }
    if (s[i] == '\'' ) {
        ++i;
        while (i < n) {
            if (s[i] == '\\') { i += (i + 1 < n ? 2 : 1); continue; }
            if (s[i++] == '\'') return i;
        }
        return n;
    }
    if (s[i] == '#' || (s[i] == '/' && i+1<n && s[i+1]=='/')) {
        while (i<n && s[i]!='\n') ++i;
        return i;
    }
    if (s[i]=='/' && i+1<n && s[i+1]=='*') {
        i += 2;
        while (i+1<n) {
            if (s[i]=='*' && s[i+1]=='/') return i+2;
            ++i;
        }
        return n;
    }
    return i+1;
}
static size_t skip_space_comments(const char *s, size_t n, size_t i)
{
    for (;;) {
        while (i<n && isspace((unsigned char)s[i])) ++i;
        if (i>=n) return n;
        if (s[i]=='#' || (s[i]=='/' && i+1<n &&
            (s[i+1]=='/' || s[i+1]=='*'))) {
            size_t j=skip_token(s,n,i);
            if (j<=i) return n;
            i=j;
            continue;
        }
        return i;
    }
}

/* Find a libconfig named group "EMULATION : {" and its matching brace.
 * Text in comments or string values is never treated as a group. */
static int find_emulation_group(const char *s, size_t n,
                                size_t *open, size_t *close)
{
    size_t i=0, j, k;
    while(i<n) {
        i=skip_space_comments(s,n,i);
        if(i>=n) break;
        if(s[i]=='"' || s[i]=='\'') { i=skip_token(s,n,i); continue; }
        if(!ident_start((unsigned char)s[i])) { ++i; continue; }
        j=i+1;
        while(j<n && ident_char((unsigned char)s[j])) ++j;
        if(j-i==9 && !memcmp(s+i,"EMULATION",9)) {
            k=skip_space_comments(s,n,j);
            if(k<n && s[k]==':') {
                k=skip_space_comments(s,n,k+1);
                if(k<n && s[k]=='{') {
                    size_t p=k+1;
                    unsigned nesting=1;
                    while(p<n) {
                        p=skip_space_comments(s,n,p);
                        if(p>=n) break;
                        if(s[p]=='"' || s[p]=='\'') { p=skip_token(s,n,p); continue; }
                        if(s[p]=='{') ++nesting;
                        if(s[p]=='}' && --nesting==0) {
                            *open=k; *close=p;
                            return 0;
                        }
                        ++p;
                    }
                    return -1;
                }
            }
        }
        i=j;
    }
    return -1;
}
/* Locate a direct (not nested) assignment SCALING = "xxx". */
static int find_scaling_value(const char *s, size_t n, size_t left, size_t right,
                              size_t *value_begin, size_t *value_end)
{
    size_t i=left+1,j,k; unsigned depth=0;
    while(i<right && i<n) {
        i=skip_space_comments(s,n,i);
        if(i>=right || i>=n) break;
        if(s[i]=='"' || s[i]=='\'') { i=skip_token(s,n,i); continue; }
        if(s[i]=='{') { ++depth; ++i; continue; }
        if(s[i]=='}') { if(depth) --depth; ++i; continue; }
        if(!ident_start((unsigned char)s[i])) { ++i; continue; }
        j=i+1;
        while(j<right && ident_char((unsigned char)s[j])) ++j;
        if(!depth && j-i==7 && !memcmp(s+i,"SCALING",7)) {
            k=skip_space_comments(s,n,j);
            if(k>=right || s[k]!='=') return -1;
            k=skip_space_comments(s,n,k+1);
            if(k>=right || s[k]!='"') return -1;
            *value_begin=k+1;
            k=skip_token(s,n,k);
            if(k<=*value_begin+1 || k>right || s[k-1]!='"') return -1;
            *value_end=k-1;
            return 1;
        }
        i=j;
    }
    return 0;
}
/* V15.49: actual hardware log shows -6 and global SCALING still FIT.
 * Detect failed existing-file rename on PS3 HEN. Use a durable backup in
 * SpookyFlow's own directory and a verified in-place fallback, without
 * modifying any ROM, other game's config, save file or console backend.
 */
#ifndef SF_SN_CFG_BACKUP_DIR
#define SF_SN_CFG_BACKUP_DIR "/dev_hdd0/tmp/spookyflow"
#endif
static void cfg_backup_path(const char *path,char *dst,size_t cap)
{
    const unsigned char *p=(const unsigned char*)path;
    uint64_t hash=UINT64_C(14695981039346656037);
    for(;*p;p++){hash^=*p;hash*=UINT64_C(1099511628211);}
    snprintf(dst,cap,SF_SN_CFG_BACKUP_DIR "/snes_cfg_recovery_%016llx.dat",
             (unsigned long long)hash);
}
static int cfg_same_bytes(const char *path,const char *bytes,size_t n)
{
    FILE *f=fopen(path,"rb");
    long length;
    size_t k=0;
    char block[2048];
    if(!f)return 0;
    if(fseek(f,0,SEEK_END)!=0 || (length=ftell(f))<0 ||
       (unsigned long)length!=(unsigned long)n || fseek(f,0,SEEK_SET)!=0) {
        (void)fclose(f);return 0;
    }
    while(k<n){
        size_t x=n-k<sizeof(block)?n-k:sizeof(block);
        if(fread(block,1,x,f)!=x || memcmp(block,bytes+k,x)!=0) {
            (void)fclose(f);return 0;
        }
        k+=x;
    }
    return fclose(f)==0;
}
static int cfg_direct(const char *path,const char *bytes,size_t n)
{
    FILE *f;
    size_t written;
    int bad;
    errno=0;
#ifdef SF_SN_TEST_DENY_DIRECT
    f=NULL; errno=EACCES;
#else
    f=fopen(path,"wb");
#endif
#ifdef __PS3__
    if(!f && (errno==EACCES || errno==EPERM)){
        /* Never chmod a directory or another title, only this pSNES CFG. */
        int c=sysFsChmod(path,0666);
        VIDEO_CFG_LOG("video_diag: CFG_WRITE stage=CHMOD_EXISTING_CONFIG rc=%d path=%.180s",c,path);
        if(c==0)f=fopen(path,"wb");
    }
#endif
    if(!f){
        VIDEO_CFG_LOG("video_diag: CFG_WRITE stage=DIRECT_OPEN_FAILED errno=%d path=%.180s",errno,path);
        return -6;
    }
    written=fwrite(bytes,1,n,f);
    bad=(written!=n || fflush(f)!=0);
    if(fclose(f)!=0)bad=1;
    if(bad || !cfg_same_bytes(path,bytes,n)){
        VIDEO_CFG_LOG("video_diag: CFG_WRITE stage=DIRECT_VERIFY_FAILED actual_bytes=%u target_bytes=%u path=%.180s",
                      (unsigned)written,(unsigned)n,path);
        return -6;
    }
    VIDEO_CFG_LOG("video_diag: CFG_WRITE stage=DIRECT_VERIFIED bytes=%u path=%.180s",(unsigned)n,path);
    return 0;
}
static int cfg_snapshot(const char *bak,const char *bytes,size_t n)
{
    FILE *f=fopen(bak,"rb");
    int bad;
    if(f){(void)fclose(f);return -6;} /* never overwrite a crash backup */
    f=fopen(bak,"wb");
    if(!f)return -6;
    bad=(fwrite(bytes,1,n,f)!=n || fflush(f)!=0);
    if(fclose(f)!=0)bad=1;
    if(bad || !cfg_same_bytes(bak,bytes,n)) {
        (void)remove(bak);
        return -6;
    }
    return 0;
}
static void cfg_recover_previous(const char *path)
{
    char bak[1024],state[128];
    FILE *f;
    long n=-1;
    char *buf;
    int pr;
    cfg_backup_path(path,bak,sizeof(bak));
    f=fopen(bak,"rb");
    if(!f)return;
    pr=sf_snes_config_video_probe(path,state,sizeof(state));
    if(pr==0 || pr==1 || (pr==2 && strstr(state,"state=MISSING")==NULL)){
        (void)fclose(f);
        (void)remove(bak); /* target is parseable; old backup is stale */
        VIDEO_CFG_LOG("video_diag: CFG_RECOVERY stage=TARGET_VALID_BACKUP_REMOVED path=%.180s",path);
        return;
    }
    if(fseek(f,0,SEEK_END)!=0 || (n=ftell(f))<1 || n>SNES_CFG_MAX ||
       fseek(f,0,SEEK_SET)!=0){(void)fclose(f);return;}
    buf=(char*)malloc((size_t)n);
    if(!buf){(void)fclose(f);return;}
    {
        int read_ok=fread(buf,1,(size_t)n,f)==(size_t)n;
        int close_ok=fclose(f)==0;
        if(read_ok && close_ok) {
            if(cfg_direct(path,buf,(size_t)n)==0){
                (void)remove(bak);
                VIDEO_CFG_LOG("video_diag: CFG_RECOVERY stage=ORIGINAL_RESTORED path=%.180s",path);
            }else VIDEO_CFG_LOG("video_diag: CFG_RECOVERY stage=FAILED_BACKUP_KEPT path=%.180s",path);
        }else VIDEO_CFG_LOG("video_diag: CFG_RECOVERY stage=BACKUP_READ_FAILED path=%.180s",path);
    }
    free(buf);
}
static int cfg_rename_replace(const char *src,const char *target)
{
#ifdef SF_SN_TEST_RENAME_EEXIST
    if(access(target,F_OK)==0){ errno=EEXIST;return -1; }
#endif
    return rename(src,target);
}
static int write_atomic(const char *path,const char *bytes,size_t n,
                        const char *old,size_t old_n)
{
    char tmp[1024],bak[1024];
    static const char suffix[]=".sf49.tmp";
    FILE *f=NULL;
    size_t p=strlen(path);
    int bad=1,rc;
    if(p+sizeof(suffix)>sizeof(tmp))return -6;
    memcpy(tmp,path,p);
    memcpy(tmp+p,suffix,sizeof(suffix));
    cfg_backup_path(path,bak,sizeof(bak));
    errno=0;
#ifndef SF_SN_TEST_DENY_TEMP
    f=fopen(tmp,"wb");
#else
    errno=EACCES;
#endif
    if(f){
        bad=(fwrite(bytes,1,n,f)!=n || fflush(f)!=0);
        if(fclose(f)!=0)bad=1;
        if(!bad) {
            if(cfg_rename_replace(tmp,path)==0){
                if(cfg_same_bytes(path,bytes,n)){
                    VIDEO_CFG_LOG("video_diag: CFG_WRITE stage=ATOMIC_FULL_CONFIRMED path=%.180s",path);
                    return 0;
                }
                VIDEO_CFG_LOG("video_diag: CFG_WRITE stage=ATOMIC_READBACK_FAILED path=%.180s",path);
                return -6;
            }
            VIDEO_CFG_LOG("video_diag: CFG_WRITE stage=RENAME_TARGET_FAILED errno=%d path=%.180s",errno,path);
        }else VIDEO_CFG_LOG("video_diag: CFG_WRITE stage=TEMP_WRITE_FAILED errno=%d path=%.180s",errno,path);
    }else VIDEO_CFG_LOG("video_diag: CFG_WRITE stage=TEMP_CREATE_FAILED errno=%d path=%.180s",errno,path);
    (void)remove(tmp); /* only our private temporary file */
    /* If PS3 HEN disallows replacement of an existing path, back up its
     * precise contents and write the same config file in-place. */
    if(old && old_n){
        if(cfg_snapshot(bak,old,old_n)!=0){
            VIDEO_CFG_LOG("video_diag: CFG_WRITE stage=BACKUP_FAILED_NO_MUTATION path=%.180s",path);
            return -6;
        }
        VIDEO_CFG_LOG("video_diag: CFG_WRITE stage=BACKUP_VERIFIED bytes=%u path=%.180s",(unsigned)old_n,path);
    }
    rc=cfg_direct(path,bytes,n);
    if(rc==0){
        if(old && old_n)(void)remove(bak);
        VIDEO_CFG_LOG("video_diag: CFG_WRITE stage=FULL_PERSISTED_RECHECKED path=%.180s",path);
        return 0;
    }
    if(old && old_n){
        if(cfg_direct(path,old,old_n)==0){
            (void)remove(bak);
            VIDEO_CFG_LOG("video_diag: CFG_WRITE stage=ROLLED_BACK_ORIGINAL path=%.180s",path);
        }else VIDEO_CFG_LOG("video_diag: CFG_WRITE stage=ROLLBACK_FAILED_BACKUP_KEPT path=%.180s",path);
    }else (void)remove(path); /* newly created config only */
    return -6;
}
int sf_snes_config_set_full(const char *path, int create_if_missing)
{
    FILE *f;
    char *buf=NULL,*out=NULL;
    size_t n,open,close,vb,ve,cut;
    long fsz;
    int result,found;
    static const char initial[] =
        "PSNES : {\n  EMULATION : {\n    SCALING = \"FULL\";\n  };\n};\n";
    static const char insertion[] = "\n    SCALING = \"FULL\";";
    if(!path || !path[0] || strlen(path)>960) return -1;
    cfg_recover_previous(path);
    f=fopen(path,"rb");
    if(!f) {
        if(!create_if_missing) return 0;
        /* The pSNES config may not exist before its first run.
         * This small valid libconfig file is filled in by pSNES on startup. */
        return write_atomic(path,initial,sizeof(initial)-1,NULL,0);
    }
    if(fseek(f,0,SEEK_END)!=0 || (fsz=ftell(f))<1 || fsz>SNES_CFG_MAX ||
       fseek(f,0,SEEK_SET)!=0) {
        fclose(f); return -2;
    }
    n=(size_t)fsz;
    buf=(char*)malloc(n+SNES_CFG_EXTRA+1);
    if(!buf) { fclose(f); return -3; }
    {
        int bad=(fread(buf,1,n,f)!=n);
        if(fclose(f)!=0) bad=1;
        if(bad || memchr(buf,0,n)) { free(buf); return -2; }
    }
    buf[n]='\0';
    result=find_emulation_group(buf,n,&open,&close);
    if(result!=0) { free(buf); return -4; }
    found=find_scaling_value(buf,n,open,close,&vb,&ve);
    if(found<0) { free(buf); return -4; }
    if(found && ve-vb==4 && !memcmp(buf+vb,"FULL",4)) {
        free(buf);
        return 0;  /* avoid writing to disk if already correct */
    }
    if(found) {
        cut=ve-vb;
        out=(char*)malloc(n+5);
        if(!out) { free(buf); return -3; }
        memcpy(out,buf,vb);
        memcpy(out+vb,"FULL",4);
        memcpy(out+vb+4,buf+ve,n-ve);
        n=n+4-cut;
    } else {
        /* Preserve every existing option, only add scaling within EMULATION */
        cut=sizeof(insertion)-1;
        out=(char*)malloc(n+cut+1);
        if(!out) { free(buf); return -3; }
        memcpy(out,buf,close);
        memcpy(out+close,insertion,cut);
        memcpy(out+close+cut,buf+close,n-close);
        n+=cut;
    }
    result=write_atomic(path,out,n,buf,(size_t)fsz);
    free(out);free(buf);
    return result;
}
int sf_snes_force_widescreen(const char *rom_path)
{
    char cfg[1024];
    const char *name,*dot;
    size_t base_len;
    int rc;
    rc=sf_snes_config_set_full(SNES_CFG_HOME "/config.cfg",1);
    if(rc!=0) return rc;
    {
        char state[192];
        int probe=sf_snes_config_video_probe(SNES_CFG_HOME "/config.cfg",state,sizeof(state));
        VIDEO_CFG_LOG("video_diag: CFG_GLOBAL_POSTWRITE probe_rc=%d %s",probe,state);
        if(probe!=0) return -7; /* Never report FULL without a readback. */
    }
    if(!rom_path) return -1;
    name=strrchr(rom_path,'/');
    name=name?name+1:rom_path;
    dot=strrchr(name,'.');
    base_len=dot?(size_t)(dot-name):strlen(name);
    if(base_len==0 || base_len>500 || strchr(name,'\\') ||
       strchr(name,'\r') || strchr(name,'\n')) return -1;
    if(snprintf(cfg,sizeof(cfg),SNES_CFG_HOME "/configs/%.*s.cfg",
                (int)base_len,name) >= (int)sizeof(cfg)) return -1;
    /* Optional per-game override loads AFTER the global settings. If it
     * exists, it must also be FULL; otherwise global FULL is inherited. */
    rc=sf_snes_config_set_full(cfg,0);
    if(rc!=0)return rc;
    {
        char state[192];
        int probe=sf_snes_config_video_probe(cfg,state,sizeof(state));
        VIDEO_CFG_LOG("video_diag: CFG_PER_ROM_POSTWRITE probe_rc=%d %s",probe,state);
        if(probe!=0 && probe!=2)return -7;
    }
    return 0;
}

/* V15.48: read-only video-config probe using the same lexer as the
 * existing production FULL writer, without changing any file.
 * 0=FULL, 1=another scaling, 2=missing value/file, negative=corrupt.
 */
int sf_snes_config_video_probe(const char *path, char *out, size_t cap)
{
    FILE *f;
    char *buf;
    long l=-1;
    size_t n,open,close,vb,ve;
    int found,rc;
    if(!out || cap<48 || !path) return -10;
    out[0]=0;
    f=fopen(path,"rb");
    if(!f) {
        snprintf(out,cap,"state=MISSING (per-ROM file inherits global)");
        return 2;
    }
    if(fseek(f,0,SEEK_END)!=0 || (l=ftell(f))<1 || l>(long)SNES_CFG_MAX ||
       fseek(f,0,SEEK_SET)!=0) {
        (void)fclose(f);
        snprintf(out,cap,"state=BAD_SIZE_OR_SEEK size=%ld",l);
        return -2;
    }
    n=(size_t)l;
    buf=(char*)malloc(n+1);
    if(!buf) {
        (void)fclose(f);
        snprintf(out,cap,"state=OUT_OF_MEMORY");
        return -3;
    }
    rc=(fread(buf,1,n,f)==n && !ferror(f) && !memchr(buf,0,n))?0:-4;
    if(fclose(f)!=0)rc=-4;
    buf[n]=0;
    if(rc!=0) {
        snprintf(out,cap,"state=READ_FAILED bytes=%u",(unsigned)n);
        free(buf);
        return rc;
    }
    if(find_emulation_group(buf,n,&open,&close)!=0) {
        snprintf(out,cap,"state=EMULATION_GROUP_NOT_FOUND bytes=%u",(unsigned)n);
        free(buf);
        return -5;
    }
    found=find_scaling_value(buf,n,open,close,&vb,&ve);
    if(found<0) {
        snprintf(out,cap,"state=BAD_SCALING_SYNTAX bytes=%u",(unsigned)n);
        free(buf);
        return -5;
    }
    if(found==0) {
        snprintf(out,cap,"state=SCALING_NOT_SET bytes=%u",(unsigned)n);
        free(buf);
        return 2;
    }
    if(ve-vb>48) {
        snprintf(out,cap,"state=INVALID_VALUE_LENGTH bytes=%u",(unsigned)(ve-vb));
        free(buf);
        return -5;
    }
    rc=(ve-vb==4 && memcmp(buf+vb,"FULL",4)==0)?0:1;
    snprintf(out,cap,"state=PARSED scaling=%.*s expected_FULL=%s config_bytes=%u",
             (int)(ve-vb),buf+vb,rc==0?"YES":"NO",(unsigned)n);
    free(buf);
    return rc;
}
