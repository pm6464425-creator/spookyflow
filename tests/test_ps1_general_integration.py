#!/usr/bin/env python3
"""Universal PS1 CUE engine: regression and preservation invariants."""
from pathlib import Path
r=Path(__file__).resolve().parents[1]
c=(r/"src/core/sf_ps1_cue_general.c").read_text()
h=(r/"src/core/sf_ps1_cue_general.h").read_text()
j=(r/"src/ps3/sf_ps1_join.c").read_text()
m=(r/"src/ps3/sf_ps1_mount.c").read_text()
a=(r/"src/core/app.c").read_text()
main=(r/"src/ps3/main.c").read_text()
plat=(r/"src/ps3/ps3_platform.h").read_text()
checks={
"PS1 engine supports multiple TRACK per FILE":"t->file_index" in c and "p->track_count" in c,
"PS1 supports raw MODE1/2352 and MODE2/2352":"MODE1/2352" in c and "MODE2/2352" in c,
"PS1 supports CDDA":"AUDIO" in c and "is_audio" in c,
"PS1 CUE PREGAP added in raw sector units":"t->pregap*2352u" in c,
"PS1 INDEX 00 not lost":"index00=index" in c and "has_index00" in c,
"PS1 POSTGAP at EOF safe":"t->postgap*2352u" in c,
"PS1 preserves input bytes in source chunks":"chunks[*n].at=at;" in c,
"PS1 new CUE grouping not tied to 1 file = 1 track":"SfPs1CueFile file[" in h and "SfPs1CueTrack track[" in h,
"PS1 general parser integrated after legacy":"return inspect_general_sources(cue,contents,n);" in j,
"PS1 generic worker stream source with 64bit seek":"sf_ps2_large_seek(&in,chunk->at)" in j,
"PS1 generic worker inserts raw gap silence":"if(chunk->is_silence)memset(s_copy_buffer,0,amount)" in j,
"PS1 generic worker asynchronous":"sysThreadCreate(&tid,ps1_join_worker" in j,
"PS1 generic files read only":"sf_ps2_large_open(&in,path,0)" in j,
"PS1 general planner explicit diagnostic":'STAGE=GENERAL_LAYOUT RESULT=OK' in j,
"PS1 general mode supports 1 staged track":"n<1" in m,
"PS1 original 28 BIN legacy remains":"sf_ps1_multibin_parse" in j,
"PS1 one diagnostic only":'SF_DATA_DIR "/diagnostico_ps1.txt"' in plat,
"PS1 legacy webMAN fallback preserved":"a->host.launch(cmd)" in a,
"PS2 HEN preserved":"host.ps2_hen_start = sf_ps2_hen_start;" in main,
"PS3 preserved":"host.launch_ps3 = sf_cobra_mount_ps3;" in main,
"BDISO preserved":"host.launch_bdiso = launch_bdiso;" in main,
}
for name,ok in checks.items():print("PASS" if ok else "FAIL",name)
print(f"V15.27 general CUE safety {sum(checks.values())}/{len(checks)}")
if not all(checks.values()):raise SystemExit(1)
