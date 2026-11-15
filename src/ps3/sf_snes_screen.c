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
static int write_atomic(const char *path, const char *bytes, size_t n)
{
    char tmp[1024];
    FILE *f;
    size_t p=strlen(path);
    static const char suffix[]=".sf42.tmp";
    if(p+sizeof(suffix)>sizeof(tmp)) return -6;
    memcpy(tmp,path,p);
    memcpy(tmp+p,suffix,sizeof(suffix));
    f=fopen(tmp,"wb");
    if(!f) return -6;
    {
        int bad=(fwrite(bytes,1,n,f)!=n);
        if(!bad && fflush(f)!=0) bad=1;
        if(fclose(f)!=0) bad=1;
        if(bad) { remove(tmp); return -6; }
    }
    if(rename(tmp,path)!=0) {
        remove(tmp);
        return -6;
    }
    return 0;
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
    f=fopen(path,"rb");
    if(!f) {
        if(!create_if_missing) return 0;
        /* The pSNES config may not exist before its first run.
         * This small valid libconfig file is filled in by pSNES on startup. */
        return write_atomic(path,initial,sizeof(initial)-1);
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
    result=write_atomic(path,out,n);
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
    return sf_snes_config_set_full(cfg,0);
}
