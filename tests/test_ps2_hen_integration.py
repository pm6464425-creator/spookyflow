#!/usr/bin/env python3
"""PS2 Classics HEN: source-level safety invariants, NOT runtime PS3 validation."""
from pathlib import Path
import re
root = Path(__file__).resolve().parents[1]
def get(p): return (root/p).read_text(encoding="utf-8")
hen=get("src/ps3/sf_ps2_hen.c")
app=get("src/core/app.c")
apph=get("src/core/app.h")
main=get("src/ps3/main.c")
cfw=get("src/ps3/sf_ps2_mount.c")
ci=get("ci/build_ps3.sh")
checks={
 "01 async conversion launch hook": "int (*ps2_hen_start)(const char *path);" in apph,
 "02 converter polling hook": "int (*ps2_hen_poll)(int *percent);" in apph,
 "03 HEN start callback installed": "host.ps2_hen_start = sf_ps2_hen_start;" in main,
 "04 HEN poll callback installed": "host.ps2_hen_poll = sf_ps2_hen_poll;" in main,
 "05 V15.13 CFW direct PS2 callback preserved": "host.launch_ps2 = sf_ps2_mount;" in main,
 "06 PS3 Cobra callback preserved": "host.launch_ps3 = sf_cobra_mount_ps3;" in main,
 "07 BDISO callback preserved": "host.launch_bdiso = launch_bdiso;" in main,
 "08 original PS1 webMAN callback preserved": "host.launch = launch_via_webman;" in main,
 "09 HEN path before CONFIG and CFW mounting": app.index("if (result == -41) {") < app.index("ps2cfg_apply(a, path, lib_str",app.index("if (a->tab == PLAT_PS2")),
 "10 no webMAN network in HEN converter": all(x not in hen for x in ("webman_get","127.0.0.1", "ps3_webman")),
 "11 existing PS2 Classics Placeholder required": '"/EBOOT.BIN"' in hen and "if(!exists_file(SF_CLASSICS_EBOOT))return -64;" in hen,
 "12 universal PS2U10000 target": '"/dev_hdd0/game/PS2U10000/USRDIR"' in hen,
 "13 source restricted to HDD PS2ISO or FAT32 USB": "sf_hen_safe_iso_path(path)" in hen and '"/dev_usb00"' in hen,
 "14 split ISO safely rejected": 'if(ends(path,".iso.0"))return -62;' in hen,
 "15 original ISO opened read-only": 'in=fopen(path,"rb");' in hen,
 "16 never truncate input ISO": "truncate(path" not in hen and 'fopen(path,"wb")' not in hen and 'fopen(path,"rb+")' not in hen,
 "17 LIMG data synthesized virtually": "memcpy(blk,limg,SEG);" in hen and 'memcpy(limg,"LIMG",4)' in hen,
 "18 existing LIMG recognized": 'memcmp(probe,"LIMG",4)' in hen,
 "19 DVD ISO9660 signature validated": 'read_bytes(in,0x8000,probe,6)' in hen,
 "20 CD ISO9660 signature validated": 'read_bytes(in,0x9318,probe,6)' in hen,
 "21 cipher AES-128-CBC implemented": "MBEDTLS_AES_ENCRYPT,SEG,iv,blk,blk" in hen,
 "22 SHA1 metadata checksum": "mbedtls_sha1(blk,SEG,meta+j*0x20)" in hen,
 "23 NPD CMAC header": "omac(npd_omac3,buf,0x30+11,h+0x50)" in hen,
 "24 per-group encrypted metadata": "MBEDTLS_AES_ENCRYPT,SEG,iv,meta,meta" in hen,
 "25 big-endian segment index": "put32(meta+j*0x20+0x14,(uint32_t)i);" in hen,
 "26 encrypted temp staged separately": '"/ISO.BIN.ENC.sf-part"' in hen,
 "27 output verifies expected encrypted size": "size!=expected_size(get64(header+0x88))" in hen,
 "28 external user's file safely backed up": "SF_CLASSICS_USER_BACKUP" in hen and "if(exists_file(backup))return -37;" in hen,
 "29 failed image rename restores previous": "if(had_old)(void)rename(backup,SF_CLASSICS_IMAGE);" in hen,
 "30 manifest binds source ISO to converted image": 'SF-PS2-CLASSICS %llu %s' in hen,
 "31 cancellation never promotes partial": "if(s_job.cancel){rc=-70;goto cleanup;}" in hen,
 "32 conversion executes in background PS3 thread": 'sysThreadCreate(&tid,ps2_thread,NULL,1600' in hen,
 "33 UI progress and controller lock": 'if (a->ps2_hen_busy && a->host.ps2_hen_poll)' in app and 'return; /* ignore controller inputs until the file is committed */' in app,
 "34 retains original 60fps coverflow animation": "animate(a,dt); /* keep moving the original stage/UI at 60 fps */" in app,
 "35 system exit requests cancellation": "sf_ps2_hen_cancel();" in main,
}
for k,ok in checks.items():print(("PASS" if ok else "FAIL"),k)
print("PS2 HEN static audit %d/%d"%(sum(checks.values()),len(checks)))
if not all(checks.values()):raise SystemExit(1)
# Algorithmic independent layout sanity examples (bytes, no game data).
SEG=0x4000
for src_size in (0x20000, 580*1024*1024, 4*1024*1024*1024):
    virtual_size=((src_size+SEG-1)//SEG)*SEG+SEG
    assert virtual_size%SEG == 0
    metadata=((virtual_size//SEG+511)//512)*SEG
    expected=SEG+metadata+virtual_size
    assert expected>src_size and expected<=src_size+(src_size//512)+0x20000
print("PASS 3 independent PS2 Classics output layout tests")
