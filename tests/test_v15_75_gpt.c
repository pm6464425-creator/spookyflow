/* Synthetic GPT drives: offline-only regression tests. No real USB I/O. */
#include "spooky_gpt.h"
#include <stdint.h>
#include <stdio.h>
#include <string.h>
enum { BROKEN_PRIMARY=1, BROKEN_BACKUP=2, NOT_GPT=4, NO_NTFS=8,
       BAD_ENTRY_CRC=16, BAD_HEADER_CRC=32, BAD_REVISION=64,
       BAD_PARTITION=128, BAD_NTFS_BPS=256, BAD_NTFS_SIG=512,
       BAD_HEADER_LBA=1024, BAD_ENTRIES=2048, BAD_USABLE=4096 };
typedef struct {uint64_t total;uint32_t bytes,mode;} Mock;
static void w32(unsigned char*p,uint32_t v){
 unsigned i;for(i=0;i<4;i++)p[i]=(unsigned char)(v>>(8*i));
}
static void w64(unsigned char*p,uint64_t v){
 unsigned i;for(i=0;i<8;i++)p[i]=(unsigned char)(v>>(8*i));
}
static uint32_t crc(const unsigned char*p,unsigned n){
 uint32_t v=~0u;unsigned i,j;
 for(i=0;i<n;i++){v^=p[i];for(j=0;j<8;j++)v=(v>>1)^(0xedb88320u&(0u-(v&1u)));}
 return ~v;
}
static void entry(Mock *m,unsigned char*s){
 memset(s,0,m->bytes);s[0]=0xA2;s[15]=0xC2;
 w64(s+32,(m->mode&BAD_PARTITION)?2:2048);
 w64(s+40,m->total-34);
}
static int rd(void*u,uint64_t lba,void*dst){
 Mock*m=(Mock*)u;unsigned char*s=(unsigned char*)dst,tmp[4096];
 if(lba>=m->total)return -1;
 memset(s,0,m->bytes);
 if(lba==0){
  s[510]=0x55;s[511]=0xAA;s[450]=(m->mode&NOT_GPT)?7:0xee;return 0;
 }
 if(lba==2 || lba==m->total-2){
  entry(m,s);if(m->mode&BAD_ENTRY_CRC)s[96]^=0x80;return 0;
 }
 if(lba==2048){
  if(m->mode&NO_NTFS)return 0;
  memcpy(s+3,"NTFS    ",8);
  s[510]=(m->mode&BAD_NTFS_SIG)?0:0x55;s[511]=0xAA;
  s[11]=(unsigned char)m->bytes;s[12]=(unsigned char)(m->bytes>>8);
  if(m->mode&BAD_NTFS_BPS)s[11]++;
  s[13]=8;w64(s+40,m->total-34-2048+1);return 0;
 }
 if(lba==1 || lba==m->total-1){
  int backup=lba!=1;
  if((backup&&(m->mode&BROKEN_BACKUP))||(!backup&&(m->mode&BROKEN_PRIMARY)))
   return 0;
  memcpy(s,"EFI PART",8);
  w32(s+8,(m->mode&BAD_REVISION)?0x00020000:0x00010000);
  w32(s+12,92);
  w64(s+24,(m->mode&BAD_HEADER_LBA)?lba+1:lba);
  w64(s+32,backup?1:m->total-1);
  w64(s+40,34);
  w64(s+48,(m->mode&BAD_USABLE)?m->total:m->total-34);
  w64(s+72,backup?m->total-2:2);
  w32(s+80,(m->mode&BAD_ENTRIES)?0:4);
  w32(s+84,128);
  entry(m,tmp);w32(s+88,crc(tmp,512));
  w32(s+16,crc(s,92));
  if(m->mode&BAD_HEADER_CRC)s[16]^=1;
 }
 return 0;
}
static int done,errors;
static void check(const char*name,uint32_t mode,uint32_t bytes,uint64_t total,int want,int recover){
 Mock m;SpookyDisk d;SpookyNtfsPartition p;int actual;
 m.total=total;m.bytes=bytes;m.mode=mode;
 d.user=&m;d.read_sector=rd;d.device_sectors=total;d.logical_sector_bytes=bytes;
 actual=spooky_find_ntfs_gpt(&d,&p);done++;
 if(actual!=want || (actual==SPOOKY_GPT_FOUND &&
   (p.partition_start_lba!=2048 || p.backup_gpt_used!=recover))){
  fprintf(stderr,"FAIL %s got=%d want=%d recover=%d\n",name,actual,want,p.backup_gpt_used);errors++;
 } else printf("PASS %02d %s\n",done,name);
}

static void test_capacity(const char *label,uint32_t mode,uint32_t bytes,uint64_t total,int want){
 Mock m;uint64_t got=0;int rc;
 m.total=total;m.bytes=bytes;m.mode=mode;
 rc=spooky_gpt_infer_device_sectors(rd,&m,bytes,&got);done++;
 if(rc!=want || (want==SPOOKY_GPT_FOUND && got!=total) ||
    (want!=SPOOKY_GPT_FOUND && got!=0)) {
  fprintf(stderr,"FAIL %s rc=%d expected=%d count=%llu\n",label,rc,want,(unsigned long long)got);errors++;
 } else printf("PASS %02d %s\n",done,label);
}
static int always_fail(void *ctx,uint64_t lba,void *buffer){
 (void)ctx;(void)lba;(void)buffer;return -1;
}

int main(void){
 const uint64_t eight=15625000000ull;
 check("8TB GPT",0,512,eight,0,0);
 check("6TB GPT",0,512,eight*3/4,0,0);
 check("4TB GPT",0,512,eight/2,0,0);
 check("8TB 4K GPT",0,4096,eight,0,0);
 check("GPT primary recovery",BROKEN_PRIMARY,512,eight,0,1);
 check("GPT 4K recovery",BROKEN_PRIMARY,4096,eight,0,1);
 check("both GPT copies broken",BROKEN_PRIMARY|BROKEN_BACKUP,512,eight,-1,0);
 check("legacy MBR",NOT_GPT,512,eight,1,0);
 check("valid GPT no NTFS",NO_NTFS,512,eight,2,0);
 check("entry CRC",BAD_ENTRY_CRC,512,eight,-1,0);
 check("header CRC",BAD_HEADER_CRC,512,eight,-1,0);
 check("header revision",BAD_REVISION,512,eight,-1,0);
 check("partition bounds",BAD_PARTITION,512,eight,2,0);
 check("NTFS sector mismatch",BAD_NTFS_BPS,512,eight,2,0);
 check("NTFS VBR signature",BAD_NTFS_SIG,512,eight,2,0);
 check("GPT current LBA",BAD_HEADER_LBA,512,eight,-1,0);
 check("zero partition entries",BAD_ENTRIES,512,eight,-1,0);
 check("GPT out-of-bounds usable",BAD_USABLE,512,eight,-1,0);
 check("unsupported sector size",0,1024,eight,-3,0);
 check("invalid device size",0,512,20,-3,0);
 test_capacity("ZERO_CAP GPT 8TB 512",0,512,eight,SPOOKY_GPT_FOUND);
 test_capacity("ZERO_CAP GPT 6TB 512",0,512,eight*3/4,SPOOKY_GPT_FOUND);
 test_capacity("ZERO_CAP GPT 4TB 512",0,512,eight/2,SPOOKY_GPT_FOUND);
 test_capacity("ZERO_CAP GPT 8TB 4K",0,4096,eight,SPOOKY_GPT_FOUND);
 test_capacity("ZERO_CAP GPT backup bad primary good",BROKEN_BACKUP,512,eight,SPOOKY_GPT_FOUND);
 test_capacity("ZERO_CAP GPT no NTFS primary still good",NO_NTFS,512,eight,SPOOKY_GPT_FOUND);
 test_capacity("ZERO_CAP GPT legacy MBR",NOT_GPT,512,eight,SPOOKY_GPT_NOT_GPT);
 test_capacity("ZERO_CAP GPT primary bad",BROKEN_PRIMARY,512,eight,SPOOKY_GPT_INVALID);
 test_capacity("ZERO_CAP GPT CRC bad",BAD_HEADER_CRC,512,eight,SPOOKY_GPT_INVALID);
 test_capacity("ZERO_CAP GPT revision bad",BAD_REVISION,512,eight,SPOOKY_GPT_INVALID);
 test_capacity("ZERO_CAP GPT LBA mismatch",BAD_HEADER_LBA,512,eight,SPOOKY_GPT_INVALID);
 test_capacity("ZERO_CAP GPT invalid usable range",BAD_USABLE,512,eight,SPOOKY_GPT_INVALID);
 test_capacity("ZERO_CAP GPT zero entries",BAD_ENTRIES,512,eight,SPOOKY_GPT_INVALID);
 test_capacity("ZERO_CAP unsupported 1KB",0,1024,eight,SPOOKY_GPT_BAD_ARGUMENT);
 {uint64_t cap=0;int rc=spooky_gpt_infer_device_sectors(always_fail,NULL,512,&cap);
  done++;if(rc!=SPOOKY_GPT_IO_ERROR || cap!=0){errors++;fprintf(stderr,"FAIL ZERO_CAP raw IO\n");}else printf("PASS %02d ZERO_CAP raw IO\n",done);}
 printf("SPOOKY GPT: %d/%d passing\n",done-errors,done);
 return errors?1:0;
}
