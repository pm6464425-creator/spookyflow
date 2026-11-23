/* V15.79: guarded external BDISO NTFS ISO deletion.
 * No webMAN index removal, no directory recursion, no cover or sidecar writes.
 */
#include "sf_delete_ntfs.h"
#include "sf_delete_policy.h"
#include "layout.h"
#include <string.h>

int sf_delete_bdiso_ntfs_safe(const char *path,
     int (*is_regular)(const char *),
     int (*resolve_native)(const char *,char *,size_t),
     int (*unlink_native)(const char *))
{
    char native[512];
    const char *leaf;
    size_t n;
    int rc,exists_after;
    if(!path || !is_regular || !resolve_native || !unlink_native)return SF_DEL_UNSUPPORTED;
    n=strlen(path);
    if(n<23 || n>=sizeof(native) || strncmp(path,"/dev_usb00",10)!=0 ||
       path[10]<'0' || path[10]>'7' ||
       strncmp(path+11,"/BDISO/",7)!=0 ||
       sf_delete_path_allowed(PLAT_BDISO,path,0)!=SF_DEL_OK)
        return SF_DEL_UNSUPPORTED;
    leaf=path+18;
    n=strlen(leaf);
    if(n<5 || strchr(leaf,'/') || strchr(leaf,'\\') ||
       strpbrk(leaf,":") || strcmp(leaf,".")==0 ||
       strcmp(leaf,"..")==0 ||
       leaf[n-4]!='.' ||
       (leaf[n-3]!='i' && leaf[n-3]!='I') ||
       (leaf[n-2]!='s' && leaf[n-2]!='S') ||
       (leaf[n-1]!='o' && leaf[n-1]!='O'))
        return SF_DEL_UNSUPPORTED;
    if(!is_regular(path) || !resolve_native(path,native,sizeof(native)))
        return SF_DEL_UNSUPPORTED;
    if(!native[0] || strstr(native,"/../") || strstr(native,"/./") ||
       strchr(native,'\\'))return SF_DEL_UNSUPPORTED;
    rc=unlink_native(native);
    exists_after=is_regular(path);
    if(rc==0 && !exists_after)return SF_DEL_OK;
    if(!exists_after)return SF_DEL_PARTIAL; /* uncertain deletion result */
    return rc==0?SF_DEL_PARTIAL:SF_DEL_DENIED;
}
