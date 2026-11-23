#ifndef TEST_SYS_FILE_H
#define TEST_SYS_FILE_H
#include <stdint.h>
typedef uint64_t u64;
typedef int32_t s32;
typedef struct {char d_name[256];} sysFSDirent;
typedef struct {s32 st_mode;u64 st_size;} sysFSStat;
#define SYS_O_RDONLY 0
s32 sysFsStat(const char *path,sysFSStat *st);
s32 sysFsOpen(const char *path,s32 flags,s32 *fd,void *mode,s32 arg);
s32 sysFsRead(s32 fd,void *buf,u64 bytes,u64 *got);
s32 sysFsClose(s32 fd);
s32 sysFsOpendir(const char *path,s32 *fd);
s32 sysFsReaddir(s32 fd,sysFSDirent *ent,u64 *got);
s32 sysFsClosedir(s32 fd);
s32 sysFsUnlink(const char *path);
s32 sysFsRmdir(const char *path);
#endif
