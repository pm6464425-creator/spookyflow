#ifndef SF_GPT_PS3_H
#define SF_GPT_PS3_H
#include <ntfs.h>
/* Read-only GPT probe fallback; existing MBR/FAT32 stays first. */
int sf_gpt_try_mount(int port,const DISC_INTERFACE *device,ntfs_md **mounts);
int sf_gpt_diag_begin(void);
void sf_gpt_diag_mbr_mounted(int port,int partitions);
#endif
