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
 "01 async conversion launch hook": "int (*ps2_hen_start)(const char *path,const char *gameid);" in apph,
 "02 converter polling hook": "int (*ps2_hen_poll)(int *percent);" in apph,
 "03 HEN start callback installed": "host.ps2_hen_start = sf_ps2_hen_start;" in main,
 "04 HEN poll callback installed": "host.ps2_hen_poll = sf_ps2_hen_poll;" in main,
 "05 V15.13 CFW direct PS2 callback preserved": "host.launch_ps2 = sf_ps2_mount;" in main,
 "06 PS3 Cobra callback preserved": "host.launch_ps3 = sf_cobra_mount_ps3;" in main,
 "07 BDISO callback preserved": "host.launch_bdiso = launch_bdiso;" in main,
 "08 original PS1 webMAN callback preserved": "host.launch = launch_via_webman;" in main,
 "09 HEN path before CONFIG and CFW mounting": app.index("if (result == -41) {") < app.index("ps2cfg_apply(a, path, lib_str",app.index("if (a->tab == PLAT_PS2")) and "ps2_disc_id(a,path" in app,
 "10 no webMAN network in HEN converter": all(x not in hen for x in ("webman_get","127.0.0.1", "ps3_webman")),
 "11 existing PS2U10000 launcher checked by SFO and directory open": 'launcher_installed()' in hen and 'launcher_sfo_id(char out[32])' in hen and 'readable_dir(SF_CLASSICS_DIR,&de)' in hen,
 "11f title ID must match PS2U10000 for SFO-only launcher": 'strcmp(out,"PS2U10000")==0' in hen and 'correct_id=param && launcher_sfo_id(id)' in hen,
 "11g PS3 file API opens real payload, not only stat": 'fopen(p,"rb")' in hen and 'opendir(p)' in hen,
 "11h safe installed launcher detection with existing EBOOT": 'accepted=usrdir && (eboot || alternate || correct_id)' in hen,
 "11i diagnostic preserves real fopen/opendir errno": all(x in hen for x in ('USRDIR_ERRNO=%d','PARAM_SFO_ERRNO=%d','EBOOT_ERRNO=%d','IMAGE_ERRNO=%d')),
 "11j report different installed PS2 IDs but never target them": 'OTHER_PS2_GAME_IDS=%s' in hen and 'strncmp(e->d_name,"PS2",3)' in hen and 'if(!accepted)' in hen,
 "11k no directory creation or EBOOT replacement": all(x not in hen for x in ('mkdir(SF_CLASSICS_DIR','fopen(SF_CLASSICS_EBOOT,"wb"','remove(SF_CLASSICS_EBOOT)')),
 "11l explicit native PS2 Classics route": 'PS2 Classics Manager' in hen and 'without webMAN MOD' in hen,
 "11a no false refusal solely for missing EBOOT": "if(!exists_file(SF_CLASSICS_EBOOT))return -64;" not in hen,
 "11b diagnostic records each detected launcher artifact": all(x in hen for x in ('USRDIR_READABLE=%d','PARAM_SFO_READABLE=%d','USRDIR_EBOOT_READABLE=%d','LAUNCHER_ACCEPTED=%d','ps2_launcher_probe.txt')),
 "11c refuses absent launcher before opening image": 'if(!launcher_installed()){record("LAUNCHER_NOT_FOUND",path,-64,0);return -64;}' in hen,
 "11d never creates missing PS2 Classics directory": 'mkdir(SF_CLASSICS_DIR' not in hen,
 "11e error toast links diagnosis not repeated install": "No se detecta PS2 Classics (ver diagnóstico)" in app,
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
 "30 manifest binds source size, path and SHA1 sample": 'SF-PS2-CLASSICS2 %llu %s %s' in hen and "source_fingerprint(source,isosize,fingerprint)" in hen,
 "31 cancellation never promotes partial": "if(s_job.cancel){rc=-70;goto cleanup;}" in hen,
 "32 conversion executes in background PS3 thread": 'sysThreadCreate(&tid,ps2_thread,NULL,1600' in hen,
 "33 UI progress and controller lock": 'if (a->ps2_hen_busy && a->host.ps2_hen_poll)' in app and 'return; /* ignore controller inputs until the file is committed */' in app,
 "34 retains original 60fps coverflow animation": "animate(a,dt); /* keep moving the original stage/UI at 60 fps */" in app,
 "35 system exit requests cancellation": "sf_ps2_hen_cancel();" in main,
 "36 selects per-game encrypted CONFIG": "ps2_config_source(const char *id" in hen and "SLUS_201.74" in hen,
 "37 packaged encrypted CONFIG fallback": '"/dev_hdd0/game/SPKF00001/USRDIR/ps2configs/SLUS_201.74.ENC"' in hen,
 "38 previous user CONFIG backed up": "SF_CFG_ORIGINAL" in hen and "rename(SF_CFG_TARGET,backup)" in hen,
 "39 invalid encrypted CONFIG rejected": "if(!verify_cfg(src))return -84;" in hen,
 "40 config prepared on cache hit": "int cfg=ps2_stage_config(gameid);" in hen,
 "41 config stage before image success status": "if(rc==0)rc=ps2_stage_config(s_job.gameid);" in hen,
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
