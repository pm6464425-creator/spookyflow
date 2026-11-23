/* Host-only NTFS transaction regressions. Never touches a user disk. */
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "sf_delete_policy.h"
#include "sf_delete_ntfs.h"
static int file_exists, unlink_rc, misleading_rc, native_calls;
static char last_native[512];
static int is_file(const char *path) { (void)path; return file_exists; }
static int native_path(const char *in,char *out,size_t cap)
{
    native_calls++;
    if(!in || cap<100)return 0;
    snprintf(out,cap,"ntfs0:%s",in);
    return 1;
}
static int erase(const char *path)
{
    strncpy(last_native,path,sizeof(last_native)-1);
    last_native[sizeof(last_native)-1]=0;
    if(unlink_rc==0 || misleading_rc)file_exists=0;
    return unlink_rc;
}
static int run(const char *p)
{
    native_calls=0;last_native[0]=0;
    return sf_delete_bdiso_ntfs_safe(p,is_file,native_path,erase);
}
#define REJECT(P) do{file_exists=1;assert(run(P)==SF_DEL_UNSUPPORTED);assert(native_calls==0);}while(0)
int main(void)
{
    REJECT(NULL);
    REJECT("");
    REJECT("/dev_hdd0/BDISO/Movie.iso");
    REJECT("/dev_usb008/BDISO/Movie.iso");
    REJECT("/dev_usb000/BDISO/Movie.mkv");
    REJECT("/dev_usb000/BDISO/Movie.iso.0");
    REJECT("/dev_usb000/BDISO/COVERS/Movie.iso");
    REJECT("/dev_usb000/BDISO/../Movie.iso");
    REJECT("/dev_usb000/BDISO/./Movie.iso");
    REJECT("/dev_usb000/BDISO//Movie.iso");
    REJECT("/dev_usb000/BDISO/Movie.iso/other.iso");
    REJECT("/dev_usb000/PS3ISO/Movie.iso");
    REJECT("/dev_hdd0/tmp/wmtmp/Movie.iso.ntfs[BDISO]");
    REJECT("/dev_usb000/BDISO/");
    /* Genuine native media, one direct ISO. */
    file_exists=1;unlink_rc=0;misleading_rc=0;
    assert(run("/dev_usb000/BDISO/Movie.iso")==SF_DEL_OK);
    assert(native_calls==1);
    assert(strcmp(last_native,"ntfs0:/dev_usb000/BDISO/Movie.iso")==0);
    assert(!file_exists);
    file_exists=1;
    assert(run("/dev_usb007/BDISO/a.ISO")==SF_DEL_OK);
    /* Missing media never calls unlink. */
    file_exists=0;
    assert(run("/dev_usb000/BDISO/Movie.iso")==SF_DEL_UNSUPPORTED);
    assert(!native_calls);
    /* A driver returns error but the file disappeared: partial/uncertain. */
    file_exists=1;unlink_rc=-1;misleading_rc=1;
    assert(run("/dev_usb000/BDISO/Movie.iso")==SF_DEL_PARTIAL);
    /* Rejection cannot be reported as a successful deletion. */
    file_exists=1;unlink_rc=-1;misleading_rc=0;
    assert(run("/dev_usb000/BDISO/Movie.iso")==SF_DEL_DENIED);
    assert(file_exists);
    printf("PASS 19 native NTFS BDISO guarded deletion host scenarios\n");
    return 0;
}
