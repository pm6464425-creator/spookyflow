#include "spooky_gpt.h"
#include <stddef.h>
#include <string.h>
#define GPT_MAX_ENTRIES 4096u
#define GPT_ENTRY_MIN 128u
#define GPT_HEADER_MIN 92u

static uint16_t le16(const unsigned char *p) {
 return (uint16_t)p[0] | ((uint16_t)p[1] << 8);
}
static uint32_t le32(const unsigned char *p) {
 return (uint32_t)p[0] | ((uint32_t)p[1] << 8) |
        ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}
static uint64_t le64(const unsigned char *p) {
 return (uint64_t)le32(p) | ((uint64_t)le32(p+4) << 32);
}
static uint32_t crc_update(uint32_t crc,const unsigned char *data,uint32_t length) {
 uint32_t i; unsigned j;
 for(i=0;i<length;i++) {
  crc ^= data[i];
  for(j=0;j<8;j++) crc=(crc>>1) ^ (0xEDB88320u & (0u-(crc&1u)));
 }
 return crc;
}
static int rawread(const SpookyDisk *d,uint64_t lba,unsigned char *sector) {
 if(lba>=d->device_sectors) return SPOOKY_GPT_INVALID;
 return d->read_sector(d->user,lba,sector)==0?0:SPOOKY_GPT_IO_ERROR;
}
typedef struct {
 uint64_t entry_lba,first_usable,last_usable;
 uint32_t entry_count,entry_bytes,entries_crc;
} ParsedHeader;

static int load_header(const SpookyDisk *d,uint64_t expected_lba,ParsedHeader *hdr) {
 unsigned char s[4096]; int rc=rawread(d,expected_lba,s);
 uint32_t head_size,expected_crc,crc; uint64_t other,array_bytes,sectors,i;
 if(rc) return rc;
 if(memcmp(s,"EFI PART",8)) return SPOOKY_GPT_INVALID;
 if(le32(s+8)!=0x00010000u || le32(s+20)!=0u) return SPOOKY_GPT_INVALID;
 head_size=le32(s+12);
 if(head_size<GPT_HEADER_MIN || head_size>d->logical_sector_bytes)
  return SPOOKY_GPT_INVALID;
 expected_crc=le32(s+16);
 memset(s+16,0,4);
 if((crc_update(0xFFFFFFFFu,s,head_size)^0xFFFFFFFFu)!=expected_crc)
  return SPOOKY_GPT_INVALID;
 if(le64(s+24)!=expected_lba) return SPOOKY_GPT_INVALID;
 other=le64(s+32);
 if(other>=d->device_sectors || other==expected_lba)
  return SPOOKY_GPT_INVALID;
 hdr->first_usable=le64(s+40);
 hdr->last_usable=le64(s+48);
 if(hdr->first_usable<=1 || hdr->first_usable>hdr->last_usable ||
    hdr->last_usable>=d->device_sectors) return SPOOKY_GPT_INVALID;
 hdr->entry_lba=le64(s+72);
 hdr->entry_count=le32(s+80);
 hdr->entry_bytes=le32(s+84);
 hdr->entries_crc=le32(s+88);
 if(!hdr->entry_count || hdr->entry_count>GPT_MAX_ENTRIES ||
    hdr->entry_bytes<GPT_ENTRY_MIN || hdr->entry_bytes>d->logical_sector_bytes ||
    (hdr->entry_bytes&(hdr->entry_bytes-1)) ||
    d->logical_sector_bytes%hdr->entry_bytes) return SPOOKY_GPT_INVALID;
 array_bytes=(uint64_t)hdr->entry_count*hdr->entry_bytes;
 sectors=(array_bytes+d->logical_sector_bytes-1)/d->logical_sector_bytes;
 if(hdr->entry_lba<2 || hdr->entry_lba>=d->device_sectors ||
    sectors>d->device_sectors-hdr->entry_lba ||
    !((hdr->entry_lba+sectors<=hdr->first_usable) ||
      (hdr->entry_lba>hdr->last_usable))) return SPOOKY_GPT_INVALID;
 crc=0xFFFFFFFFu;
 for(i=0;i<sectors;i++) {
  uint64_t left; uint32_t n;
  rc=rawread(d,hdr->entry_lba+i,s);
  if(rc) return rc;
  left=array_bytes-i*d->logical_sector_bytes;
  n=(uint32_t)(left<d->logical_sector_bytes?left:d->logical_sector_bytes);
  crc=crc_update(crc,s,n);
 }
 return (crc^0xFFFFFFFFu)==hdr->entries_crc?0:SPOOKY_GPT_INVALID;
}
static int scan_entries(const SpookyDisk *d,const ParsedHeader *h,SpookyNtfsPartition *out) {
 unsigned char table_sector[4096],vbr[4096];
 uint32_t per_sector=d->logical_sector_bytes/h->entry_bytes,index;
 uint64_t last_read=UINT64_MAX;
 for(index=0;index<h->entry_count;index++) {
  uint64_t lba=h->entry_lba+index/per_sector,start,last,ntfs_sectors;
  const unsigned char *entry; int used=0,k,rc; uint8_t spc;
  if(lba!=last_read) {
   rc=rawread(d,lba,table_sector);
   if(rc) return rc;
   last_read=lba;
  }
  entry=table_sector+(index%per_sector)*h->entry_bytes;
  for(k=0;k<16;k++) used |= entry[k];
  if(!used) continue;
  start=le64(entry+32);
  last=le64(entry+40);
  if(start<h->first_usable || last>h->last_usable || start>last) continue;
  rc=rawread(d,start,vbr);
  if(rc) return rc;
  if(memcmp(vbr+3,"NTFS    ",8) || vbr[510]!=0x55 || vbr[511]!=0xAA)
   continue;
  if(le16(vbr+11)!=d->logical_sector_bytes) continue;
  spc=vbr[13];
  if(!spc || (spc&(spc-1u))) continue;
  ntfs_sectors=le64(vbr+40);
  if(!ntfs_sectors || ntfs_sectors>last-start+1u) continue;
  out->partition_start_lba=start;
  out->partition_last_lba=last;
  out->volume_sectors=ntfs_sectors;
  out->sector_bytes=d->logical_sector_bytes;
  return SPOOKY_GPT_FOUND;
 }
 return SPOOKY_GPT_NO_NTFS;
}

/* A 4-8 TB PS3 USB may return a successful storage-info call but sector_count=0.
 * Read ONLY sector 0 and sector 1 to get the backup-header LBA. Reconstruct a
 * bounded logical capacity only after validating the GPT header CRC and fields.
 * This never changes the underlying device-size report or patches LV2. */
int spooky_gpt_infer_device_sectors(SpookyReadSector rd,void *user,
                                    uint32_t logical_sector_bytes,uint64_t *sectors)
{
 unsigned char sector[4096];uint32_t size,original,calc;
 uint64_t alternate,first,last,entry_lba;
 unsigned i;int protective=0;
 if(sectors)*sectors=0;
 if(!rd || !sectors || (logical_sector_bytes!=512 && logical_sector_bytes!=4096))
  return SPOOKY_GPT_BAD_ARGUMENT;
 if(rd(user,0,sector)!=0)return SPOOKY_GPT_IO_ERROR;
 if(sector[510]!=0x55 || sector[511]!=0xAA)return SPOOKY_GPT_NOT_GPT;
 for(i=0;i<4;i++)if(sector[446+i*16+4]==0xEE)protective=1;
 if(!protective)return SPOOKY_GPT_NOT_GPT;
 if(rd(user,1,sector)!=0)return SPOOKY_GPT_IO_ERROR;
 if(memcmp(sector,"EFI PART",8)!=0 ||
    le32(sector+8)!=0x00010000u || le32(sector+20)!=0)
  return SPOOKY_GPT_INVALID;
 size=le32(sector+12);
 if(size<GPT_HEADER_MIN || size>logical_sector_bytes)
  return SPOOKY_GPT_INVALID;
 original=le32(sector+16);
 memset(sector+16,0,4);
 calc=crc_update(0xFFFFFFFFu,sector,size)^0xFFFFFFFFu;
 if(calc!=original || le64(sector+24)!=1)return SPOOKY_GPT_INVALID;
 alternate=le64(sector+32);
 first=le64(sector+40);
 last=le64(sector+48);
 entry_lba=le64(sector+72);
 /* 2^40 sectors covers >500 TB at 512 bytes and prevents overflow. */
 if(alternate<34 || alternate>(UINT64_C(1)<<40) ||
    first<34 || last<first || last>=alternate ||
    entry_lba<2 || entry_lba>=first ||
    !le32(sector+80) || le32(sector+80)>GPT_MAX_ENTRIES ||
    le32(sector+84)<GPT_ENTRY_MIN || le32(sector+84)>logical_sector_bytes ||
    (le32(sector+84)&(le32(sector+84)-1u)) ||
    logical_sector_bytes%le32(sector+84))
  return SPOOKY_GPT_INVALID;
 *sectors=alternate+1u;
 return SPOOKY_GPT_FOUND;
}

int spooky_find_ntfs_gpt(const SpookyDisk *d,SpookyNtfsPartition *out) {
 unsigned char mbr[4096]; int rc,protective=0; unsigned i;
 ParsedHeader h;
 if(!d || !out || !d->read_sector ||
    (d->logical_sector_bytes!=512 && d->logical_sector_bytes!=4096) ||
    d->device_sectors<34) return SPOOKY_GPT_BAD_ARGUMENT;
 memset(out,0,sizeof(*out));
 rc=rawread(d,0,mbr);
 if(rc) return rc;
 if(mbr[510]!=0x55 || mbr[511]!=0xAA) return SPOOKY_GPT_NOT_GPT;
 for(i=0;i<4;i++) if(mbr[446+i*16+4]==0xEE) protective=1;
 if(!protective) return SPOOKY_GPT_NOT_GPT;
 rc=load_header(d,1,&h);
 if(rc==SPOOKY_GPT_INVALID) {
  rc=load_header(d,d->device_sectors-1,&h);
  if(!rc) out->backup_gpt_used=1;
 }
 if(rc) return rc;
 return scan_entries(d,&h,out);
}
