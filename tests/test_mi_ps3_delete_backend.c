/* Host-side behavioral tests for guarded MI PS3 user-game removal.
 * Uses an isolated temporary filesystem: NEVER touches actual PS3/user files.
 */
#define _DEFAULT_SOURCE 1
#include <assert.h>
#include <dirent.h>
#include <fcntl.h>
#include <limits.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include "sys/file.h"
#include "layout.h"
#include "sf_delete_game.h"

static char root[PATH_MAX];
static DIR *dirs[16];
static void real_path(const char *virtual_path,char *dst,size_t n)
{
    assert(virtual_path && virtual_path[0]=='/');
    assert(strncmp(virtual_path,"/dev_",5)==0);
    assert(snprintf(dst,n,"%s%s",root,virtual_path)<(int)n);
}
static void make_parents(const char *virtual_path)
{
    char dst[PATH_MAX],*p;
    real_path(virtual_path,dst,sizeof(dst));
    for(p=dst+strlen(root)+1;*p;p++){
        if(*p!='/')continue;
        *p=0;
        assert(mkdir(dst,0777)==0 || access(dst,F_OK)==0);
        *p='/';
    }
}
static void make_file(const char *vpath,const char *contents)
{
    char dst[PATH_MAX];
    FILE *f;
    real_path(vpath,dst,sizeof(dst));
    make_parents(vpath);
    f=fopen(dst,"wb");assert(f);
    assert(fwrite(contents,1,strlen(contents),f)==strlen(contents));
    fclose(f);
}
static void make_dir(const char *vpath)
{
    char dst[PATH_MAX];
    real_path(vpath,dst,sizeof(dst));
    make_parents(vpath);
    assert(mkdir(dst,0777)==0);
}
static int exists(const char *vpath)
{
    char dst[PATH_MAX];
    real_path(vpath,dst,sizeof(dst));
    return access(dst,F_OK)==0;
}
s32 sysFsStat(const char *p,sysFSStat *st)
{
    struct stat q;char dst[PATH_MAX];
    real_path(p,dst,sizeof(dst));
    if(lstat(dst,&q)!=0)return -1;
    st->st_mode=q.st_mode;st->st_size=q.st_size;return 0;
}
s32 sysFsOpen(const char *p,s32 f,s32 *fd,void *mode,s32 arg)
{
    char dst[PATH_MAX];(void)f;(void)mode;(void)arg;
    real_path(p,dst,sizeof(dst));
    *fd=open(dst,O_RDONLY);
    return *fd>=0?0:-1;
}
s32 sysFsRead(s32 fd,void *buf,u64 size,u64 *got)
{
    ssize_t r=read(fd,buf,(size_t)size);
    if(r<0)return -1;
    *got=(u64)r;return 0;
}
s32 sysFsClose(s32 fd){return close(fd)==0?0:-1;}
s32 sysFsOpendir(const char *p,s32 *fd)
{
    char dst[PATH_MAX];int i;
    real_path(p,dst,sizeof(dst));
    for(i=1;i<16;i++)if(!dirs[i]){
        dirs[i]=opendir(dst);
        if(!dirs[i])return -1;
        *fd=i;return 0;
    }
    return -1;
}
s32 sysFsReaddir(s32 fd,sysFSDirent *ent,u64 *got)
{
    struct dirent *d;
    if(fd<=0 || fd>=16 || !dirs[fd])return -1;
    d=readdir(dirs[fd]);
    if(!d){*got=0;return 0;}
    strncpy(ent->d_name,d->d_name,sizeof(ent->d_name)-1);
    ent->d_name[sizeof(ent->d_name)-1]=0;
    *got=1;return 0;
}
s32 sysFsClosedir(s32 fd)
{
    if(fd<=0 || fd>=16 || !dirs[fd])return -1;
    closedir(dirs[fd]);dirs[fd]=NULL;return 0;
}
s32 sysFsUnlink(const char *p)
{
    char dst[PATH_MAX];real_path(p,dst,sizeof(dst));
    return unlink(dst)==0?0:-1;
}
s32 sysFsRmdir(const char *p)
{
    char dst[PATH_MAX];real_path(p,dst,sizeof(dst));
    return rmdir(dst)==0?0:-1;
}
void log_printf(const char *fmt, ...){(void)fmt;}
#define DEL(P,V,D) assert(sf_delete_game((P),(V),(D))==0)
#define FAIL(P,V,D) assert(sf_delete_game((P),(V),(D))!=0)

int main(void)
{
    char templ[]="/tmp/mi-ps3-delete-XXXXXX";
    char *p=mkdtemp(templ);assert(p);strcpy(root,p);
    /* 1. SNES exact SFC deletes one ROM and retains a sibling. */
    make_file("/dev_hdd0/SNES/Cooly Skunk (ESP).sfc","ROM");
    make_file("/dev_hdd0/SNES/Classic Kong.smc","ROM");
    DEL(PLAT_SNES,"/dev_hdd0/SNES/Cooly Skunk (ESP).sfc",0);
    assert(!exists("/dev_hdd0/SNES/Cooly Skunk (ESP).sfc"));
    assert(exists("/dev_hdd0/SNES/Classic Kong.smc"));
    /* 2. Direct system EBOOT protection. */
    make_file("/dev_hdd0/game/SPKF00001/USRDIR/EBOOT.BIN","SYSTEM");
    FAIL(PLAT_PS3,"/dev_hdd0/game/SPKF00001/USRDIR/EBOOT.BIN",0);
    assert(exists("/dev_hdd0/game/SPKF00001/USRDIR/EBOOT.BIN"));
    /* 3. Arcade single ROM removal, keep clone. */
    make_file("/dev_hdd0/ARCADE/kof2002.zip","ZIP");
    make_file("/dev_hdd0/ARCADE/kof2002b.zip","ZIP");
    DEL(PLAT_ARCADE,"/dev_hdd0/ARCADE/kof2002.zip",0);
    assert(!exists("/dev_hdd0/ARCADE/kof2002.zip"));
    assert(exists("/dev_hdd0/ARCADE/kof2002b.zip"));
    /* 4. PS2 ISO. */
    make_file("/dev_hdd0/PS2ISO/Downhill Domination.iso","DVD");
    DEL(PLAT_PS2,"/dev_hdd0/PS2ISO/Downhill Domination.iso",0);
    assert(!exists("/dev_hdd0/PS2ISO/Downhill Domination.iso"));
    /* 5. Blu-ray ISO in internal disk. */
    make_file("/dev_hdd0/BDISO/La Odisea.iso","BD");
    DEL(PLAT_BDISO,"/dev_hdd0/BDISO/La Odisea.iso",0);
    assert(!exists("/dev_hdd0/BDISO/La Odisea.iso"));
    /* 6. PS3 folder recursive removal, never touch sibling installation. */
    make_file("/dev_hdd0/GAMES/BLES00001/PS3_GAME/USRDIR/EBOOT.BIN","ONE");
    make_file("/dev_hdd0/GAMES/BLES00002/PS3_GAME/USRDIR/EBOOT.BIN","TWO");
    DEL(PLAT_PS3,"/dev_hdd0/GAMES/BLES00001",1);
    assert(!exists("/dev_hdd0/GAMES/BLES00001"));
    assert(exists("/dev_hdd0/GAMES/BLES00002/PS3_GAME/USRDIR/EBOOT.BIN"));
    /* 7. Prevent game root deletion. */
    FAIL(PLAT_PS3,"/dev_hdd0/GAMES",1);
    assert(exists("/dev_hdd0/GAMES/BLES00002"));
    /* 8. Split PS3 ISO removes all its fragments and no unrelated ISO. */
    make_file("/dev_hdd0/PS3ISO/GT6.iso.0","SPLIT0");
    make_file("/dev_hdd0/PS3ISO/GT6.iso.1","SPLIT1");
    make_file("/dev_hdd0/PS3ISO/GT5.iso","SIBLING");
    DEL(PLAT_PS3,"/dev_hdd0/PS3ISO/GT6.iso.0",0);
    assert(!exists("/dev_hdd0/PS3ISO/GT6.iso.0"));
    assert(!exists("/dev_hdd0/PS3ISO/GT6.iso.1"));
    assert(exists("/dev_hdd0/PS3ISO/GT5.iso"));
    /* 9. CUE single BIN removes both, leaving unrelated games untouched. */
    make_file("/dev_hdd0/PSXISO/Tekken 3.bin","TRACK");
    make_file("/dev_hdd0/PSXISO/Tekken 3.cue",
              "FILE \"Tekken 3.bin\" BINARY\n TRACK 01 MODE2/2352\n");
    make_file("/dev_hdd0/PSXISO/Crash Bandicoot.bin","OTHER");
    DEL(PLAT_PS1,"/dev_hdd0/PSXISO/Tekken 3.cue",0);
    assert(!exists("/dev_hdd0/PSXISO/Tekken 3.cue"));
    assert(!exists("/dev_hdd0/PSXISO/Tekken 3.bin"));
    assert(exists("/dev_hdd0/PSXISO/Crash Bandicoot.bin"));
    /* 10. CUE with two tracks and two FILE lines. */
    make_file("/dev_hdd0/PSXISO/Metal Slug (Track 1).bin","DATA");
    make_file("/dev_hdd0/PSXISO/Metal Slug (Track 2).bin","AUDIO");
    make_file("/dev_hdd0/PSXISO/Metal Slug.cue",
              "FILE \"Metal Slug (Track 1).bin\" BINARY\n"
              " TRACK 01 MODE2/2352\n"
              "FILE \"Metal Slug (Track 2).bin\" BINARY\n"
              " TRACK 02 AUDIO\n");
    DEL(PLAT_PS1,"/dev_hdd0/PSXISO/Metal Slug.cue",0);
    assert(!exists("/dev_hdd0/PSXISO/Metal Slug.cue"));
    assert(!exists("/dev_hdd0/PSXISO/Metal Slug (Track 1).bin"));
    assert(!exists("/dev_hdd0/PSXISO/Metal Slug (Track 2).bin"));
    /* 11. Detect shared BIN in another CUE and refuse any deletion. */
    make_file("/dev_hdd0/PSXISO/Common.bin","SHARED");
    make_file("/dev_hdd0/PSXISO/Game A.cue","FILE \"Common.bin\" BINARY\n");
    make_file("/dev_hdd0/PSXISO/Game B.cue","FILE \"Common.bin\" BINARY\n");
    FAIL(PLAT_PS1,"/dev_hdd0/PSXISO/Game A.cue",0);
    assert(exists("/dev_hdd0/PSXISO/Game A.cue"));
    assert(exists("/dev_hdd0/PSXISO/Game B.cue"));
    assert(exists("/dev_hdd0/PSXISO/Common.bin"));
    /* 12. Reject missing CUE track; leave CUE and files untouched. */
    make_file("/dev_hdd0/PSXISO/Broken.cue","FILE \"Missing.bin\" BINARY\n");
    FAIL(PLAT_PS1,"/dev_hdd0/PSXISO/Broken.cue",0);
    assert(exists("/dev_hdd0/PSXISO/Broken.cue"));
    /* 13. Reject CUE directory traversal before unlink. */
    make_file("/dev_hdd0/PSXISO/Evil.cue","FILE \"../Protected.bin\" BINARY\n");
    FAIL(PLAT_PS1,"/dev_hdd0/PSXISO/Evil.cue",0);
    assert(exists("/dev_hdd0/PSXISO/Evil.cue"));
    /* 14. Reject unknown extensions, even within a normal game directory. */
    make_file("/dev_hdd0/BDISO/Movie.mkv","MOVIE");
    FAIL(PLAT_BDISO,"/dev_hdd0/BDISO/Movie.mkv",0);
    assert(exists("/dev_hdd0/BDISO/Movie.mkv"));
    /* 15. Reject NTFS pointer; it is not the movie ISO. */
    make_file("/dev_hdd0/tmp/wmtmp/Film.iso.ntfs[BDISO]","POINTER");
    FAIL(PLAT_BDISO,"/dev_hdd0/tmp/wmtmp/Film.iso.ntfs[BDISO]",0);
    assert(exists("/dev_hdd0/tmp/wmtmp/Film.iso.ntfs[BDISO]"));
    /* 16. USB FAT32 path can delete real ISO; other roots still denied. */
    make_file("/dev_usb000/BDISO/Film.iso","FAT32");
    DEL(PLAT_BDISO,"/dev_usb000/BDISO/Film.iso",0);
    assert(!exists("/dev_usb000/BDISO/Film.iso"));
    /* 17. File not found is an error (never claim success). */
    FAIL(PLAT_SNES,"/dev_hdd0/SNES/Missing.sfc",0);
    /* 18. Folder deletion is never attempted for generic SNES or BDISO dirs. */
    make_dir("/dev_hdd0/SNES/Accidental");
    FAIL(PLAT_SNES,"/dev_hdd0/SNES/Accidental",1);
    assert(exists("/dev_hdd0/SNES/Accidental"));
    printf("PASS 18 MI PS3 end-to-end deletion and safety scenarios\n");
    return 0;
}
