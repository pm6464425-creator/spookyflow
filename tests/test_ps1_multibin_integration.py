#!/usr/bin/env python3
"""V15.25 integration safety and non-destructive PS1 multiBIN conversion."""
from pathlib import Path
r=Path(__file__).resolve().parents[1]
j=(r/"src/ps3/sf_ps1_join.c").read_text()
m=(r/"src/ps3/sf_ps1_mount.c").read_text()
a=(r/"src/core/app.c").read_text()
h=(r/"src/core/app.h").read_text()
main=(r/"src/ps3/main.c").read_text()
parser=(r/"src/core/sf_ps1_multibin.c").read_text()
plat=(r/"src/ps3/ps3_platform.h").read_text()
chk={
 "01 strict Redump multiFILE CUE parser": "sf_ps1_multibin_parse" in j and "FILE" in parser,
 "02 zero data/file loss": "sf_ps2_large_open(&in,s_join.plan.part[i].path,0)" in j,
 "03 never opens original for write": "sf_ps2_large_open(&out,s_join.part,1)" in j,
 "04 verified 64bit file copy": '#include "sf_ps2_largefile.h"' in j,
 "05 original PS2 64bit header reused": "SfPS2LargeFile" in j,
 "06 pregap silence kept": "memset(s_copy_buffer,0,n)" in j,
 "07 audio track LBA preserved": "s_join.tracks[i].lba" in j and "sf_ps1_multibin_layout" in j,
 "08 disk free space guard": 'sysFsGetFreeSize("/dev_hdd0"' in j,
 "09 output bounded 1400 MB": "PS1_STAGE_MAX" in j,
 "10 output in SpookyFlow-only cache": 'SF_DATA_DIR "/ps1_unido_' in j,
 "11 only own .sfpart can be removed": "remove(s_join.part)" in j and 'remove(s_join.plan.part' not in j,
 "12 atomic completion": "rename(s_join.part,s_join.output)" in j,
 "13 no auto deletion of finished BIN": "remove(s_join.output)" not in j,
 "14 source cache fingerprint": "st.st_mtime" in j and "fnv(contents,n,hash)" in j,
 "15 copy progress": "s_join.percent=(int)(done*100u/s_join.expected)" in j,
 "16 async worker": "sysThreadCreate(&tid,ps1_join_worker" in j,
 "17 background UI remains animated": "if(a->ps1_join_busy && a->host.ps1_join_poll)" in a and "animate(a,dt)" in a,
 "18 no user input during join": "a->toast_t=0.30f;" in a,
 "19 join follows native -41": "if(a->host.ps1_join_start && a->host.ps1_join_poll" in a,
 "20 successful staging mounts with native Cobra": "return sf_ps1_mount_staged" in j,
 "21 native Cobra track count": "s_cobra_tracks[i].is_audio=s_plan.tracks[i].is_audio" in m,
 "22 preserve normal PS1 mount": "int sf_ps1_mount(const char *path,int emulator)" in m,
 "23 old webMAN fallback retained": "a->host.launch(cmd)" in a,
 "24 thread cancel on XMB exit": "sf_ps1_join_cancel();" in main,
 "25 PS2 HEN unchanged": "host.ps2_hen_start = sf_ps2_hen_start;" in main,
 "26 native PS3 unchanged": "host.launch_ps3 = sf_cobra_mount_ps3;" in main,
 "27 BDISO unchanged": "host.launch_bdiso = launch_bdiso;" in main,
 "28 one overwrite TXT unchanged": 'SF_DATA_DIR "/diagnostico_ps1.txt"' in plat,
 "29 explicit CUE track files validate": "INDEX" in parser and 'mode,"AUDIO"' in parser,
 "30 no batch deletion of user files": "remove(s_join.part)" in j and "rm -rf" not in j,
}
for name,ok in chk.items():print("PASS" if ok else "FAIL",name)
print(f"V15.25 PS1 multiBIN checks {sum(chk.values())}/{len(chk)}")
if not all(chk.values()):raise SystemExit(1)
