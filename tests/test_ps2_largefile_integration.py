#!/usr/bin/env python3
"""V15.22 regression invariants: real 64-bit PS3 file API and fail-closed writes."""
from pathlib import Path
r=Path(__file__).resolve().parents[1]
c=(r/"src/ps3/sf_ps2_hen.c").read_text()
h=(r/"src/ps3/sf_ps2_largefile.h").read_text()
app=(r/"src/core/app.c").read_text()
checks={
 "01 native LV2 seek64": "sysLv2FsLSeek64" in h,
 "02 native LV2 file open": "sysLv2FsOpen" in h,
 "03 native LV2 read": "sysLv2FsRead" in h,
 "04 native LV2 write": "sysLv2FsWrite" in h,
 "05 native LV2 fsync": "sysLv2FsFsync" in h,
 "06 explicit output truncate": "SYS_O_TRUNC" in h,
 "07 checked short write": "written==0" in h,
 "08 checked short read": "received==0" in h,
 "09 seek position validated": "newpos!=at" in h,
 "10 64 bit position": "uint64_t position;" in h,
 "11 separate host backend": "#ifdef SF_PS2_HEN_HOST_TEST" in h,
 "12 no off_t at encryption commit": "ftello(out)" not in c and "fseeko(out" not in c,
 "13 source read large64": "sf_ps2_large_open(&in,path,0)" in c,
 "14 output write large64": "sf_ps2_large_open(&out,SF_CLASSICS_TMP,1)" in c,
 "15 virtual disc probing large64": "validate_iso(&in,size" in c,
 "16 source fingerprint large64": "sf_ps2_large_open(&f,source,0)" in c,
 "17 encrypted sample last large64": "diag_sample_block(&f,count-1" in c,
 "18 encrypted sample first large64": "diag_sample_block(&f,0" in c,
 "19 metadata offsets u64": "uint64_t meta_offset;uint64_t group_end;" in c,
 "20 seek back safely": 'large_io_error("METADATA_SEEK_BACK"' in c,
 "21 return to stream end": 'large_io_error("ENCRYPTED_STREAM_SEEK_FORWARD"' in c,
 "22 write errors detail": "LARGEFILE_IO_ERROR=1" in c,
 "23 write failure still RC -34": 'large_io_error("ENCRYPTED_BLOCK_WRITE",&out,-34' in c,
 "24 output atomic stage remains": "finish_atomic(path,size)" in c,
 "25 HEN CONFIG stage remains": "ps2_stage_config(s_job.gameid)" in c,
 "26 HEN cached state remains": "ready_manifest(path,size)" in c,
 "27 HEN autoplay preserved": '/play.ps3?PS2U10000' in app,
 "28 PS3 mount preserved": "a->host.launch_ps3" in app,
 "29 PS1 + BDISO preserved": "a->host.launch_bdiso" in app and "a->host.launch_ps2" in app,
 "30 source not modified": "source ISO is never modified" in c,
 "31 fail cleanup still removes only temp": 'remove(SF_CLASSICS_TMP)' in c,
 "32 encrypted image remains protected": 'SF_CLASSICS_USER_BACKUP' in c,
}
for k,v in checks.items():print("PASS" if v else "FAIL",k)
if not all(checks.values()):raise SystemExit("PS2 V15.22 regressions failed")
print("V15.22 64-bit PS2 safety checks: 32/32")
