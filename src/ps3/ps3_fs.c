/* ps3_fs.c - directory listing for the scanner, via lv2 sysFs.
 * Verified against PSL1GHT ppu/include/lv2/sysfs.h: sysFsOpendir(path, &fd),
 * sysFsReaddir(fd, sysFSDirent*, u64 *read), sysFsClosedir(fd),
 * sysFsStat(path, sysFSStat*), sysFsMkdir(path, mode); sysFSDirent has
 * d_type, d_namlen, d_name; sysFSStat has s32 st_mode, u64 st_size. */
#include <stdio.h>
#include <string.h>
#include <sys/file.h>
#include "ps3_platform.h"
#include "sf_bdiso_ntfs.h"
#include <sys/stat.h>
#include "log.h"
#include "util.h"

/* UNVERIFIED: lv2 mode bits follow the POSIX layout (S_IFMT 0170000,
 * S_IFDIR 0040000) and d_type 1 = directory, as in the Cell SDK. */
#define SF_S_IFMT  0170000
#define SF_S_IFDIR 0040000
#define SF_DT_DIR  1

int ps3_fs_exists(const char *path)
{
    sysFSStat st;
    return sysFsStat(path, &st) == 0;
}

void ps3_fs_mkdirs(void)
{
    /* mode 0777 like PSL1GHT samples; errors (already exists) are fine */
    sysFsMkdir("/dev_hdd0/tmp", 0777);
    sysFsMkdir(SF_DATA_DIR, 0777);
    sysFsMkdir(SF_TABS_DIR, 0777);
    sysFsMkdir(SF_SOUNDS_DIR, 0777);
    sysFsMkdir(SF_MUSIC_DIR, 0777);
    sysFsMkdir(SF_BG_DIR, 0777);
    sysFsMkdir(SF_CASES_DIR, 0777);
    sysFsMkdir(SF_COVERS_DIR, 0777);
    sysFsMkdir(SF_COVERS_DIR "/ps1", 0777);
    sysFsMkdir(SF_COVERS_DIR "/ps2", 0777);
    sysFsMkdir(SF_COVERS_DIR "/ps3", 0777);
    /* V15.31: create the ROM root once; never touch USB, ROMs or other games. */
    if (sysFsMkdir("/dev_hdd0/SNES", 0777) != 0) {
        sysFSStat st;
        if (sysFsStat("/dev_hdd0/SNES", &st) != 0 ||
            (st.st_mode & SF_S_IFMT) != SF_S_IFDIR)
            log_printf("snes: failed to create ROM directory /dev_hdd0/SNES");
    }
    /* ARCADE is an independent user ROM folder in the HDD root, beside SNES.
     * Never delete, rename or overwrite user-provided ROMs and artwork. */
    if (sysFsMkdir("/dev_hdd0/ARCADE", 0777) != 0) {
        sysFSStat st;
        if (sysFsStat("/dev_hdd0/ARCADE", &st) != 0 ||
            (st.st_mode & SF_S_IFMT) != SF_S_IFDIR)
            log_printf("arcade: could not create /dev_hdd0/ARCADE");
    }
    /* Optional original cover scans: /dev_hdd0/ARCADE/COVERS/<rom name>.png
     * The scanner ignores COVERS as a game folder. */
    if (sysFsMkdir("/dev_hdd0/ARCADE/COVERS", 0777) != 0) {
        sysFSStat st;
        if (sysFsStat("/dev_hdd0/ARCADE/COVERS", &st) != 0 ||
            (st.st_mode & SF_S_IFMT) != SF_S_IFDIR)
            log_printf("arcade: could not create /dev_hdd0/ARCADE/COVERS");
    }
    /* Dedicated processed SNES covers, alongside the existing cache folders. */
    sysFsMkdir(SF_COVERS_DIR "/snes", 0777);
}

/* V15.18 one-time cleanup on each app start. Strict allowlist from our own
 * previous releases. No recursive traversal, no wildcard, no deletion on USB.
 * Missing files or directories simply count as zero. */
int ps3_fs_cleanup_legacy_diagnostics(void)
{
    static const char *const names[]={
        "log.txt",
        "diagnostico.txt", /* previous V15.22 PS2-only report, superseded */
        "diagnostico_ps1.txt", /* old PS1 TXT retired by SNES diagnostic */
        "diagnostico_pantalla_ps1.txt", /* V15.52 legacy PS1 screen diagnostic */
        "diagnostico_portadas.txt",
        "diagnostico_audio.txt",
        "diagnostico_snes.txt",
        "diagnostico_pantalla_snes.txt",
        "diagnostico_usb_ntfs.txt",
        "cobra_startup_probe.txt",
        "cobra_selection_probe.txt",
        "cobra_last_mount.txt",
        "cobra_trace.txt",
        "cobra_previous_interruption.txt",
        "ps2_hen_last.txt",
        "ps2_launcher_probe.txt",
        "ps2_last_mount.txt",
        "ps2_mount_trace.txt"
    };
    size_t i;
    int removed=0;
    for(i=0;i<sizeof(names)/sizeof(names[0]);i++) {
        char path[256];
        sysFSStat st;
        if(snprintf(path,sizeof(path),"%s/%s",SF_DATA_DIR,names[i]) >=
            (int)sizeof(path)) continue;
        if(sysFsStat(path,&st)!=0) continue;
        /* An unexpected directory with a report name is never deleted. */
        if((st.st_mode & SF_S_IFMT)==SF_S_IFDIR) continue;
        if(sysFsUnlink(path)==0)removed++;
    }
#if defined(SF_ARCADE_DIAG_ONLY) || defined(SF_VIDEO_ONLY_DIAG)
    /* User asked to remove all old diagnostics, not user data.
     * Enumerate only this folder and unlink ONLY diagnostico*.txt,
     * skipping the live report, non-regular entries and all subfolders. */
    {
        s32 fd;
        if(sysFsOpendir(SF_DATA_DIR,&fd)==0) {
            for(;;) {
                sysFSDirent ent;
                u64 got=0;
                size_t n;
                char full[256];
                sysFSStat st;
                int rr=sysFsReaddir(fd,&ent,&got);
                if(rr!=0 || got==0)break;
                n=strlen(ent.d_name);
                if(n<=4 || n>=120)continue;
                if(strncmp(ent.d_name,"diagnostico",11)!=0)continue;
                if(strcmp(ent.d_name+n-4,".txt")!=0)continue;
#if defined(SF_ARCADE_DIAG_ONLY)
                /* Preserve the ONE report through app restarts; a new X
                 * overwrites it, without ever growing the folder. */
                if(strcmp(ent.d_name,"diagnostico_mame.txt")==0)continue;
#elif defined(SF_PS1_VIDEO_DIAG)
                if(strcmp(ent.d_name,"diagnostico_pantalla_ps1.txt")==0)continue;
#else
                if(strcmp(ent.d_name,"diagnostico_pantalla_snes.txt")==0)continue;
#endif
                if(snprintf(full,sizeof(full),"%s/%s",SF_DATA_DIR,ent.d_name)>=(int)sizeof(full))continue;
                if(sysFsStat(full,&st)!=0 || (st.st_mode & SF_S_IFMT)==SF_S_IFDIR)continue;
                if(sysFsUnlink(full)==0)removed++;
            }
            sysFsClosedir(fd);
        }
    }
#endif
    return removed;
}

int ps3_fs_list(void *ctx, const char *dir, FsEntry *out, int max)
{
    s32 fd;
    sysFSDirent ent;
    u64 nread = 0;
    int n = 0, skipped = 0;
    char path[SCAN_PATH_MAX];
    (void)ctx;
    if (sysFsOpendir(dir, &fd) != 0) return sf_bdntfs_list(dir,out,max);
    while (n < max) {
        sysFSStat st;
        size_t len;
        if (sysFsReaddir(fd, &ent, &nread) != 0 || nread == 0) break;
        if (ent.d_name[0] == '.' && (ent.d_name[1] == '\0' || (ent.d_name[1] == '.' && ent.d_name[2] == '\0'))) continue;
        len = strlen(ent.d_name);
        if (len >= FS_NAME_MAX) { skipped++; continue; }
        memcpy(out[n].name, ent.d_name, len + 1);
        out[n].is_dir = 0;
        out[n].size = 0;
        if (snprintf(path, sizeof(path), "%s/%s", dir, ent.d_name) < (int)sizeof(path) && sysFsStat(path, &st) == 0) {
            out[n].is_dir = (st.st_mode & SF_S_IFMT) == SF_S_IFDIR;
            out[n].size = out[n].is_dir ? 0 : (uint64_t)st.st_size;
        } else {
            out[n].is_dir = ent.d_type == SF_DT_DIR;
        }
        n++;
    }
    sysFsClosedir(fd);
    if (skipped) log_printf("scan: %s: %d names too long, skipped", dir, skipped);
    return n;
}

/* Arcade-only paged HDD listing: keep the original ps3_fs_list() untouched.
 * Offsets count valid names, not bytes and not batches; reopening the small
 * directory handle allows 7000-10000 ROMs without a giant array in RAM. */
int ps3_fs_list_arcade_page(void *ctx,const char *dir,FsEntry *out,int max,int offset)
{
    static const char root[]="/dev_hdd0/ARCADE";
    const size_t prefix=sizeof(root)-1;
    s32 fd;
    sysFSDirent ent;
    u64 nread=0;
    int seen=0,n=0,skipped=0;
    char path[SCAN_PATH_MAX];
    (void)ctx;
    if(!dir || strncmp(dir,root,prefix)!=0 ||
       (dir[prefix]!='\0' && dir[prefix]!='/') ||
       !out || max<=0 || offset<0) return -1;
    if(sysFsOpendir(dir,&fd)!=0)return -1;
    while(n<max){
        sysFSStat st;
        size_t len;
        if(sysFsReaddir(fd,&ent,&nread)!=0 || nread==0)break;
        if(ent.d_name[0]=='.' &&
           (ent.d_name[1]=='\0' ||
            (ent.d_name[1]=='.' && ent.d_name[2]=='\0')))continue;
        len=strlen(ent.d_name);
        if(len>=FS_NAME_MAX){skipped++;continue;}
        if(seen++<offset)continue;
        memcpy(out[n].name,ent.d_name,len+1);
        out[n].is_dir=0;
        out[n].size=0;
        if(snprintf(path,sizeof(path),"%s/%s",dir,ent.d_name)<(int)sizeof(path) &&
           sysFsStat(path,&st)==0){
            out[n].is_dir=(st.st_mode & SF_S_IFMT)==SF_S_IFDIR;
            out[n].size=out[n].is_dir ? 0 : (uint64_t)st.st_size;
        }else out[n].is_dir=ent.d_type==SF_DT_DIR;
        n++;
    }
    sysFsClosedir(fd);
    if(skipped)log_printf("arcade: skipped=%d long filenames dir=%s",skipped,dir);
    return n;
}

/* ---- small file operations (PS2 config picker) ---- */
int ps3_fs_is_file(void *ctx, const char *path)
{
    sysFSStat st;
    (void)ctx;
    return (sysFsStat(path, &st) == 0 && (st.st_mode & SF_S_IFMT) != SF_S_IFDIR) ||
           sf_bdntfs_is_file(path);
}

int ps3_fs_write_file(const char *path, const void *data, uint32_t len)
{
    s32 fd;
    u64 n = 0;
    int ok;
    if (sysFsOpen(path, SYS_O_WRONLY | SYS_O_CREAT | SYS_O_TRUNC, &fd, NULL, 0) != 0) return -1;
    ok = len == 0 || (sysFsWrite(fd, data, len, &n) == 0 && n == len);
    sysFsClose(fd);
    if (!ok) { sysFsUnlink(path); return -1; }
    sysFsChmod(path, 0666);      /* other tools (ManaGunZ, FTP) can replace it */
    return 0;
}

int ps3_fs_remove_file(const char *path) { return sysFsUnlink(path) == 0 ? 0 : -1; }
int sf_bdiso_fat_mkdir(const char *path) { return sysFsMkdir(path,0777)==0 ? 0 : -1; }
