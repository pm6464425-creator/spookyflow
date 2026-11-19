#ifndef SPOOKY_GPT_H
#define SPOOKY_GPT_H
#include <stdint.h>
/* Read-only GPT NTFS discovery. Callback must return one complete sector. */
typedef int (*SpookyReadSector)(void *user,uint64_t lba,void *out_sector);
typedef struct {
 void *user;
 SpookyReadSector read_sector;
 uint64_t device_sectors;
 uint32_t logical_sector_bytes;
} SpookyDisk;
typedef struct {
 uint64_t partition_start_lba;
 uint64_t partition_last_lba;
 uint64_t volume_sectors;
 uint32_t sector_bytes;
 int backup_gpt_used;
} SpookyNtfsPartition;
enum {
 SPOOKY_GPT_FOUND=0, SPOOKY_GPT_NOT_GPT=1, SPOOKY_GPT_NO_NTFS=2,
 SPOOKY_GPT_INVALID=-1, SPOOKY_GPT_IO_ERROR=-2, SPOOKY_GPT_BAD_ARGUMENT=-3
};
/* When LV2 reports 0 total sectors, use only verified GPT primary metadata
 * to estimate geometry. This does not prove large-sector IO works. */
int spooky_gpt_infer_device_sectors(SpookyReadSector rd,void *user,
                                    uint32_t logical_sector_bytes,uint64_t *sectors);
int spooky_find_ntfs_gpt(const SpookyDisk *disk,SpookyNtfsPartition *out);
#endif
