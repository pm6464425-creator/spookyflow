/* MI PS3 user-game removal authorization. No write operations in this file. */
#include "sf_delete_policy.h"
#include "layout.h"
#include <string.h>
#include <ctype.h>

static int sf_del_ci_eq(char a, char b)
{
    return tolower((unsigned char)a)==tolower((unsigned char)b);
}
static int sf_del_ends(const char *name, const char *ext)
{
    size_t n=strlen(name), e=strlen(ext), i;
    if(n<=e) return 0;
    for(i=0;i<e;i++) if(!sf_del_ci_eq(name[n-e+i],ext[i]))return 0;
    return 1;
}
static int sf_del_comp_eq(const char *p,size_t n,const char *lit)
{
    size_t i, ln=strlen(lit);
    if(n!=ln)return 0;
    for(i=0;i<n;i++)if(!sf_del_ci_eq(p[i],lit[i]))return 0;
    return 1;
}
static int sf_del_remainder_ok(const char *rel, int is_dir)
{
    const char *p=rel,*segment=rel;
    int depth=0;
    if(!rel || !*rel || strlen(rel)>360)return 0;
    for(;;p++){
        unsigned char ch=(unsigned char)*p;
        if(!ch || ch=='/'){
            size_t len=(size_t)(p-segment);
            if(!len || (len==1 && segment[0]=='.') ||
               (len==2 && segment[0]=='.' && segment[1]=='.') ||
               sf_del_comp_eq(segment,len,"COVERS"))
                return 0;
            depth++;
            if(!ch)break;
            segment=p+1;
        }else if(ch<32 || ch==127 || ch=='\\' || ch==':'){
            return 0;
        }
    }
    /* A PS3 GAMES/GAMEZ entry is always exactly one user folder. */
    return is_dir ? depth==1 : depth<=2;
}
static int sf_del_root(const char *path, const char **remainder)
{
    static const char internal[]="/dev_hdd0/";
    if(strncmp(path,internal,sizeof(internal)-1)==0){
        *remainder=path+sizeof(internal)-1;
        return 1;
    }
    /* FAT32 USB only: never mutate raw NTFS/exFAT or webMAN pointer paths. */
    if(strncmp(path,"/dev_usb",8)==0 &&
       path[8]>='0' && path[8]<='9' &&
       path[9]>='0' && path[9]<='9' &&
       path[10]>='0' && path[10]<='7' &&
       path[11]=='/'){
        *remainder=path+12;
        return 1;
    }
    return 0;
}
int sf_delete_path_allowed(int platform,const char *path,int is_dir)
{
    const char *r, *name, *slash;
    char root[16];
    size_t root_len;
    if(!path || !*path || strlen(path)>500 || !sf_del_root(path,&r))
        return SF_DEL_DENIED;
    slash=strchr(r,'/');
    if(!slash || slash==r || (size_t)(slash-r)>=sizeof(root))
        return SF_DEL_DENIED;
    root_len=(size_t)(slash-r);
    memcpy(root,r,root_len);
    root[root_len]=0;
    name=slash+1;
    if(!sf_del_remainder_ok(name,is_dir))return SF_DEL_DENIED;

    if(is_dir){
        return platform==PLAT_PS3 &&
            (!strcmp(root,"GAMES") || !strcmp(root,"GAMEZ")) ?
            SF_DEL_OK:SF_DEL_DENIED;
    }
    /* Reject unsupported containers and all paths outside console roots. */
    switch(platform){
    case PLAT_PS1:
        if(strcmp(root,"PSXISO") && strcmp(root,"PSXGAMES"))return SF_DEL_DENIED;
        return (sf_del_ends(name,".cue") || sf_del_ends(name,".bin") ||
                sf_del_ends(name,".img") || sf_del_ends(name,".mdf") ||
                sf_del_ends(name,".iso") || sf_del_ends(name,".iso.0")) ?
                SF_DEL_OK:SF_DEL_UNSUPPORTED;
    case PLAT_PS2:
        if(strcmp(root,"PS2ISO"))return SF_DEL_DENIED;
        return (sf_del_ends(name,".iso") || sf_del_ends(name,".iso.0")) ?
                SF_DEL_OK:SF_DEL_UNSUPPORTED;
    case PLAT_PS3:
        if(strcmp(root,"PS3ISO"))return SF_DEL_DENIED;
        return (sf_del_ends(name,".iso") || sf_del_ends(name,".iso.0")) ?
                SF_DEL_OK:SF_DEL_UNSUPPORTED;
    case PLAT_BDISO:
        if(strcmp(root,"BDISO"))return SF_DEL_DENIED;
        return (sf_del_ends(name,".iso") || sf_del_ends(name,".iso.0")) ?
                SF_DEL_OK:SF_DEL_UNSUPPORTED;
    case PLAT_SNES:
        if(strcmp(root,"SNES"))return SF_DEL_DENIED;
        return (sf_del_ends(name,".sfc") || sf_del_ends(name,".smc") ||
                sf_del_ends(name,".zip")) ? SF_DEL_OK:SF_DEL_UNSUPPORTED;
    case PLAT_ARCADE:
        if(strcmp(root,"ARCADE"))return SF_DEL_DENIED;
        return (sf_del_ends(name,".zip") || sf_del_ends(name,".7z")) ?
                SF_DEL_OK:SF_DEL_UNSUPPORTED;
    default:
        return SF_DEL_DENIED;
    }
}
