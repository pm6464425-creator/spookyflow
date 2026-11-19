/* V15.75: read-only NTFS-GPT discovery. Do not modify ISO launchers.
 * Legacy NTFS library and raw_iso have 32-bit sector map limits.
 */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>
#include <malloc.h>
#include <ppu-lv2.h>
#include <cobra.h>
#include "spooky_gpt.h"
#include "sf_gpt_ps3.h"
#include "ps3_platform.h"
#include "log.h"

#define SF_GPT_REPORT SF_DATA_DIR "/diagnostico_gpt.txt"
typedef struct {
 uint8_t name[7],unknown01;
 uint32_t unknown02,zero01,unknown03,unknown04;
 uint8_t zero02[16];
 uint64_t total_sectors;
 uint32_t sector_size,unknown05;
 uint8_t writable,unknown06[3];
 uint32_t unknown07;
} __attribute__((packed)) SfDeviceInfo;
typedef struct {
 int fd;
 uint32_t sector_bytes;
 unsigned char *aligned;
 uint64_t last_lba;
 int last_rc;
 uint32_t last_count;
 unsigned reads;
} SfReader;
static inline int sf_storage_info(uint64_t id,SfDeviceInfo *info) {
 lv2syscall2(609,id,(uint64_t)(uintptr_t)info);
 return_to_user_prog(int);
}
static inline int sf_storage_open(uint64_t id,int *fd) {
 lv2syscall4(600,id,0,(uint64_t)(uintptr_t)fd,0);
 return_to_user_prog(int);
}
static inline int sf_storage_close(int fd) {
 lv2syscall1(601,fd);
 return_to_user_prog(int);
}
static inline int sf_storage_read(int fd,uint64_t lba,unsigned char *buffer,uint32_t *count) {
 lv2syscall7(602,fd,0,lba,1,(uint64_t)(uintptr_t)buffer,(uint64_t)(uintptr_t)count,0);
 return_to_user_prog(int);
}
static int sf_read_sector(void *user,uint64_t lba,void *destination) {
 SfReader *r=(SfReader *)user; uint32_t got=0; int result;
 if(!r || !destination || !r->aligned)return -1;
 result=sf_storage_read(r->fd,lba,r->aligned,&got);
 r->last_lba=lba;r->last_rc=result;r->last_count=got;r->reads++;
 if(result!=0 || got!=1)return -1;
 memcpy(destination,r->aligned,r->sector_bytes);
 return 0;
}
static void sf_diag(int port,const char *event,int result,uint64_t value) {
 FILE *f=fopen(SF_GPT_REPORT,"ab");
 if(f) {
  fprintf(f,"USB%03d EVENT=%s RESULT=%d VALUE=%" PRIu64 "\n",port,event,result,value);
  fclose(f);
 }
 log_printf("gpt: USB%03d %s result=%d value=%llu",port,event,result,(unsigned long long)value);
}
int sf_gpt_diag_begin(void) {
 FILE *f=fopen(SF_GPT_REPORT,"wb");
 if(!f)return -1;
 fputs("SPOOKYFLOW V15.76 - DIAGNOSTICO NTFS GPT CAPACIDAD CERO\n"
       "Informe nuevo en cada inicio; datos reales obtenidos en PS3 HEN.\n"
       "La deteccion GPT es SOLO LECTURA y no altera particiones USB.\n"
       "NTFS MBR y FAT32 conservan sus rutas originales.\n"
       "ADVERTENCIA: detectar 4/6/8 TB no garantiza ISO mas alla de 2 TiB.\n"
       "El NTFS GPT experimental se monta en solo lectura como proteccion.\n"
       "--------------------------------------------------------------\n",f);
 return fclose(f)==0?0:-1;
}
void sf_gpt_diag_mbr_mounted(int port,int partitions) {
 if(port>=0 && port<8 && partitions>0)
  sf_diag(port,"EXISTING_NTFS_MBR_MOUNT",0,(uint64_t)partitions);
}
int sf_gpt_try_mount(int port,const DISC_INTERFACE *device,ntfs_md **mounts) {
 SfDeviceInfo info; SfReader reader; SpookyDisk disk; SpookyNtfsPartition partition;
 int rc,found; uint64_t device_id; char name[32]; ntfs_md *mounted;
 if(mounts)*mounts=NULL;
 if(port<0 || port>=8 || !device || !mounts || !device->isInserted || !device->isInserted())return 0;
 device_id=USB_MASS_STORAGE(port);
 memset(&info,0,sizeof(info));
 info.unknown03=0x12345678u;
 rc=sf_storage_info(device_id,&info);
 if(rc!=0){sf_diag(port,"STORAGE_INFO_FAILED",rc,0);return 0;}
 sf_diag(port,"PHYSICAL_CAPACITY_SECTORS",0,info.total_sectors);
 sf_diag(port,"LOGICAL_SECTOR_BYTES",0,info.sector_size);
 if(info.sector_size!=512 && info.sector_size!=4096) {
  sf_diag(port,"UNSUPPORTED_SECTOR_SIZE",-1,info.sector_size);
  return 0;
 }
 if(info.total_sectors<34)
  sf_diag(port,"LV2_CAPACITY_ZERO_TRY_GPT_HEADER",0,info.total_sectors);
 memset(&reader,0,sizeof(reader));
 reader.fd=-1;
 reader.sector_bytes=info.sector_size;
 reader.aligned=(unsigned char*)memalign(128,info.sector_size);
 if(!reader.aligned){sf_diag(port,"NO_ALIGNED_MEMORY",-1,info.sector_size);return 0;}
 rc=sf_storage_open(device_id,&reader.fd);
 if(rc!=0){
  sf_diag(port,"STORAGE_OPEN_FAILED",rc,0);
  free(reader.aligned);return 0;
 }
 disk.user=&reader;
 disk.read_sector=sf_read_sector;
 disk.device_sectors=info.total_sectors;
 disk.logical_sector_bytes=info.sector_size;
 if(disk.device_sectors<34) {
  found=spooky_gpt_infer_device_sectors(sf_read_sector,&reader,
                                         info.sector_size,&disk.device_sectors);
  if(found!=SPOOKY_GPT_FOUND) {
   sf_diag(port,"GPT_CAPACITY_INFERENCE_FAILED",found,reader.reads);
   if(found==SPOOKY_GPT_IO_ERROR) {
    sf_diag(port,"RAW_READ_FAILED_LBA",reader.last_rc,reader.last_lba);
    sf_diag(port,"RAW_READ_SECTORS_RETURNED",0,reader.last_count);
   }
   (void)sf_storage_close(reader.fd);
   free(reader.aligned);
   return 0;
  }
  sf_diag(port,"GPT_HEADER_INFERRED_SECTORS_NOT_PHYSICAL",0,disk.device_sectors);
 }
 found=spooky_find_ntfs_gpt(&disk,&partition);
 sf_diag(port,"GPT_RAW_SECTORS_ATTEMPTED",0,reader.reads);
 if(found==SPOOKY_GPT_IO_ERROR) {
  sf_diag(port,"RAW_READ_FAILED_LBA",reader.last_rc,reader.last_lba);
  sf_diag(port,"RAW_READ_SECTORS_RETURNED",0,reader.last_count);
 }
 (void)sf_storage_close(reader.fd);
 free(reader.aligned);
 if(found==SPOOKY_GPT_NOT_GPT) {
  sf_diag(port,"NOT_GPT_LEGACY_OR_FAT32",found,0);
  return 0;
 }
 if(found!=SPOOKY_GPT_FOUND) {
  sf_diag(port,"GPT_NOT_MOUNTABLE",found,0);
  return 0;
 }
 sf_diag(port,"GPT_NTFS_PARTITION_FOUND",0,partition.partition_start_lba);
 sf_diag(port,"GPT_NTFS_VOLUME_SECTORS",0,partition.volume_sectors);
 if(partition.backup_gpt_used)sf_diag(port,"BACKUP_GPT_RECOVERED",0,1);
 if(partition.partition_start_lba>UINT32_MAX) {
  sf_diag(port,"NTFS_DRIVER_32BIT_START_LBA_UNSUPPORTED",-1,partition.partition_start_lba);
  return 0;
 }
 if(partition.partition_last_lba>UINT32_MAX)
  sf_diag(port,"WARNING_VOLUME_BEYOND_32BIT_ABI",-1,partition.partition_last_lba);
 if(snprintf(name,sizeof(name),"sfgpt%d",port)>=(int)sizeof(name))return 0;
 /* The legacy NTFS driver has a 32-bit LBA ABI. GPT mounts stay read-only
    so trying GPT cannot corrupt 4TB+ media through cover writes. */
 if(!ntfsMount(name,device,(sec_t)partition.partition_start_lba,
               CACHE_DEFAULT_PAGE_COUNT,CACHE_DEFAULT_PAGE_SIZE,
               NTFS_DEFAULT|NTFS_READ_ONLY)) {
  sf_diag(port,"GPT_NATIVE_NTFS_MOUNT_FAILED",-1,partition.partition_start_lba);
  return 0;
 }
 mounted=(ntfs_md*)calloc(1,sizeof(*mounted));
 if(!mounted) {
  ntfsUnmount(name,true);
  sf_diag(port,"GPT_MOUNT_METADATA_MEMORY_FAILED",-1,0);
  return 0;
 }
 snprintf(mounted->name,sizeof(mounted->name),"%s",name);
 mounted->interface=device;
 mounted->startSector=(sec_t)partition.partition_start_lba;
 *mounts=mounted;
 sf_diag(port,"GPT_MOUNT_READ_ONLY_PROTECT_HIGH_LBA",0,1);
 sf_diag(port,"GPT_NATIVE_NTFS_MOUNT_OK",0,1);
 return 1;
}
