/* Strict internal-HDD PS2 ISO list for Cobra. Untrusted paths never reach LV2.
 * Derived behavior from IRISMAN's multi-part ISO handling (GPL-3.0-or-later).
 */
#include "ps2_mount_plan.h"
#include <stdio.h>
#include <string.h>
#include <strings.h>

static int ends(const char *s,const char *suffix)
{
    size_t n=strlen(s),m=strlen(suffix);
    return n>=m && strcasecmp(s+n-m,suffix)==0;
}
int sf_ps2_valid_iso_path(const char *path)
{
    const char *p;
    size_t n;
    if (!path) return 0;
    n=strlen(path);
    if (n<22 || n>960 || strncmp(path,"/dev_hdd0/PS2ISO/",17)) return 0;
    if (!path[17] || !strcmp(path+17,".") || !strcmp(path+17,"..")) return 0;
    if (!(ends(path,".iso") || ends(path,".iso.0"))) return 0;
    if (strstr(path,"//") || strstr(path,"/../") || strstr(path,"/./") ||
        strstr(path,"\\") || ends(path,"/..") || ends(path,"/.")) return 0;
    for (p=path;*p;p++) if ((unsigned char)*p<32 || (unsigned char)*p==127) return 0;
    return 1;
}
int sf_ps2_collect_parts(const char *path, SfPs2FileExists exists, void *ctx,
                         char paths[SF_PS2_MAX_PARTS][SF_PS2_MAX_PATH])
{
    size_t len;
    int i,n=0;
    if (!sf_ps2_valid_iso_path(path) || !exists || !paths) return -1;
    len=strlen(path);
    if (!ends(path,".iso.0")) {
        if (!exists(ctx,path)) return -2;
        if (snprintf(paths[0],SF_PS2_MAX_PATH,"%s",path)>=SF_PS2_MAX_PATH) return -1;
        return 1;
    }
    for (i=0;i<SF_PS2_MAX_PARTS;i++) {
        int k=snprintf(paths[i],SF_PS2_MAX_PATH,"%.*s%d",(int)(len-1),path,i);
        if (k<=0 || k>=SF_PS2_MAX_PATH) return -1;
        if (!exists(ctx,paths[i])) break;
        n++;
    }
    if (!n) return -2;
    /* If all 64 exist, explicit refusal: possibly an unlisted 65th part. */
    if (n==SF_PS2_MAX_PARTS) return -3;
    return n;
}
