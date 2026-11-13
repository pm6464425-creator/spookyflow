#!/usr/bin/env python3
"""V15.19 static package and safety audit: country/edition != disc layout."""
from pathlib import Path
root=Path(__file__).resolve().parents[1]
def src(x):return (root/x).read_text(encoding="utf-8")
hen=src("src/ps3/sf_ps2_hen.c")
m=src("src/core/sf_ps2_media.c")
cd=src("src/ps3/sf_ps2_cdrom.c")
ui=src("src/core/app.c")
checks={
 "01 PAL region detected by SLES/SCES": '"SLES"' in m and '"SCES"' in m,
 "02 NTSC-U detected by SLUS/SCUS": '"SLUS"' in m and '"SCUS"' in m,
 "03 NTSC-J detected by SLPM/SLPS/SCPS": all('"'+p+'"' in m for p in ('SLPM','SLPS','SCPS')),
 "04 Asia codes detected": '"SCKA"' in m and '"SLKA"' in m,
 "05 unknown region kept unknown": 'return "DESCONOCIDA";' in m,
 "06 US Rumble cooked CD media decision": 'id,"SLUS_201.74"' in m,
 "07 PAL Rumble cooked CD media decision": 'id,"SLES_501.20"' in m,
 "08 all raw CD images preferred over cooked": 'if(has_2352_pvd) return SF_PS2_IMAGE_CD_RAW_2352' in m,
 "09 unrelated cooked ISO not relabeled CD": 'SF_PS2_IMAGE_DVD_2048' in m,
 "10 region-neutral packaged CONFIG lookup": '"%s/%s.ENC"' in hen,
 "11 installed CONFIG database checked first": hen.index('"CONFIG_LOOKUP=PS2CONFIG') < hen.index('"CONFIG_LOOKUP=BUNDLED'),
 "12 absent exact CONFIG never cross-region fallback": 'CONFIG_LOOKUP=NONE' in hen and 'strcmp(id,"SLUS_201.74")' not in hen,
 "13 PAL encrypted CONFIG file included": (root/"pkgfiles/USRDIR/ps2configs/SLES_501.20.ENC").exists(),
 "14 US encrypted CONFIG retained": (root/"pkgfiles/USRDIR/ps2configs/SLUS_201.74.ENC").exists(),
 "15 cooked CD stream uses real EDC/ECC sector function": 'sf_cdrom_mode2_form1(stream->sector,stream->userdata,stream->lba++)' in hen,
 "16 Mode2/Form1 subheader is duplicated": 'out[18]=out[22]=8' in cd,
 "17 Mode2/Form1 ECC P and Q calculated": 'ecc_block(out+12,86,24,2,86,out+0x81c)' in cd and 'ecc_block(out+12,52,43,86,88,out+0x8c8)' in cd,
 "18 cooked CD transform is streamed not written as source": 'cooked_cd_fill(in,&cdstream,blk,readn)' in hen and 'in=fopen(path,"rb");' in hen,
 "19 LIMG for CD uses 2352 sector size and 2 disc type": 'limg[14]=0x09;limg[15]=0x30' in hen and 'limg[7]=2' in hen,
 "20 copied CD input validates 2048-sector alignment": 'if(size%2048u) return -24;' in hen,
 "21 raw CD input validates 2352-sector alignment": 'size%2352u' in hen,
 "22 cached old DVD-misidentified Rumble invalidated": 'SF-PS2-CLASSICS3 %llu %s %s' in hen and 'SF-PS2-CLASSICS2' not in hen,
 "23 external PS2 Classics launcher still used without webMAN": '"PS2U10000"' in hen and '127.0.0.1' not in hen,
 "24 single overwrite-only diagnostics retained": 'SF_SINGLE_DIAG_FILE' in src("src/core/log.c") and 'SF_DIAG_SINGLE_PATH' in src("src/ps3/main.c"),
 "25 PS3 and BDISO mount callbacks retained": 'host.launch_ps3 = sf_cobra_mount_ps3;' in src("src/ps3/main.c") and 'host.launch_bdiso = launch_bdiso;' in src("src/ps3/main.c"),
 "26 original UI caratula search untouched": 'ps2: mounting directly' in ui,
 "27 source encrypted only with mandatory mbedTLS": '#error "PS2 Classics encryption requires mbedTLS' in hen,
 "28 raw CD output does not invent audio tracks": 'do NOT restore missing audio/form2' in cd or 'do NOT restore missing audio/form2' in src("src/ps3/sf_ps2_cdrom.h") or 'do NOT restore missing audio' in cd or 'do not reencode files in RAM' in hen,
}
for name,ok in checks.items():print(("PASS" if ok else "FAIL"),name)
print("V15.19 PS2 cross-region and CD audit: %d/%d passed"%(sum(checks.values()),len(checks)))
if not all(checks.values()):raise SystemExit(1)
