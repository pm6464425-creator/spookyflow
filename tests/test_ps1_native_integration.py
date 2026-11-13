#!/usr/bin/env python3
"""V15.23 safe native PS1 mount integration invariants."""
from pathlib import Path
r=Path(__file__).resolve().parents[1]
p=(r/"src/ps3/sf_ps1_mount.c").read_text()
c=(r/"src/core/sf_ps1_native_plan.c").read_text()
app=(r/"src/core/app.c").read_text()
h=(r/"src/core/app.h").read_text()
main=(r/"src/ps3/main.c").read_text()
chk={
"PS1 own callback": "int (*launch_ps1)(const char *path, int emulator);" in h,
"PS1 main register": "host.launch_ps1 = sf_ps1_mount;" in main,
"PS1 native before webMAN": app.index("if(a->tab==PLAT_PS1 && a->host.launch_ps1)") < app.index("if (a->tab == PLAT_PS3 && a->host.launch_ps3)"),
"PS1 legacy webMAN fallback retained": 'host.launch = launch_via_webman;' in main and 'if(result!=-41)' in app,
"PS1 preserves webMAN for complex cues": "return -41;" in p and "sf_ps1_plan_cue" in p,
"PS1 cue track metadata is forwarded": "s_cobra_tracks[i].lba=s_plan.tracks[i].lba;" in p and "s_cobra_tracks[i].is_audio=s_plan.tracks[i].is_audio;" in p,
"PS1 native actual Cobra PSX opcode": "cobra_mount_psx_disc_image" in p,
"PS1 Cobra guard": "ps1_cobra_version(&v)" in p,
"PS1 no alteration before guard": p.index("rc=prepare(path,emulator);")<p.index("cobra_send_fake_disc_eject_event()"),
"PS1 undo disc events on error": "if(ejected)(void)cobra_send_fake_disc_insert_event()" in p,
"PS1 no fake success": 'PS1_DIAG STAGE=MOUNT_PSX_7027 SYS_RC=%d' in p and "if(rc!=0){rc=-44;goto fail;}" in p,
"PS1 no extra TXT": 'fopen(' not in p.replace('fopen(path,"rb")',''),
"PS1 supports PSXISO and PSXGAMES": "/PSXISO/" in p and "/PSXGAMES/" in p,
"PS1 no 2048 sectors": "st.st_size%2352" in p and "RESULT=UNSUPPORTED_SECTOR_OR_TRACK_LAYOUT" in p,
"PS1 rejects multi-BIN": "++files!=1" in c,
"PS1 rejects unsafe cue paths": 'strstr(base,"..")' in c,
"PS1 rejects explicit pregap": 'word(cur,"PREGAP")' in c,
"PS1 recognizes AUDIO": 'mode,"AUDIO"' in c,
"PS1 no PS2 changes": "host.launch_ps2 = sf_ps2_mount;" in main,
"PS1 no PS3 changes": "host.launch_ps3 = sf_cobra_mount_ps3;" in main,
"PS1 no BDISO changes": "host.launch_bdiso = launch_bdiso;" in main,
}
for name,ok in chk.items():print("PASS" if ok else "FAIL",name)
if not all(chk.values()):raise SystemExit("PS1 native safety checks failed")
print("V15.23 PS1 native: %d/%d tests passed"%(sum(chk.values()),len(chk)))
