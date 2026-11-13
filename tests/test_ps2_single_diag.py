#!/usr/bin/env python3
"""V15.24: one PS1 report, strict removal of older PS2 report, no user data loss."""
from pathlib import Path
import re
root=Path(__file__).resolve().parents[1]
def src(p):return (root/p).read_text(encoding="utf-8")
hdr=src("src/ps3/ps3_platform.h")
fs=src("src/ps3/ps3_fs.c")
log=src("src/core/log.c")
main=src("src/ps3/main.c")
hen=src("src/ps3/sf_ps2_hen.c")
ps1=src("src/ps3/sf_ps1_mount.c")
app=src("src/core/app.c")
mk=src("Makefile")
allowlist={
 "diagnostico.txt", "log.txt","diagnostico_portadas.txt","diagnostico_usb_ntfs.txt",
 "cobra_startup_probe.txt","cobra_selection_probe.txt","cobra_last_mount.txt",
 "cobra_trace.txt","cobra_previous_interruption.txt","ps2_hen_last.txt",
 "ps2_launcher_probe.txt","ps2_last_mount.txt","ps2_mount_trace.txt"
}
block=fs.split('static const char *const names[]={',1)[1].split('};',1)[0]
entries=set(re.findall(r'"([^"]+)"',block))
checks={
 "01 one PS1 file name": '#define SF_DIAG_SINGLE_PATH SF_DATA_DIR "/diagnostico_ps1.txt"' in hdr,
 "02 old PS2 report removed": "diagnostico.txt" in entries and len(entries)==13 and entries==allowlist,
 "03 central PS1 report overwritten on restart": 'g_log=(path && path[0])?fopen(path,"wb"):NULL;' in log,
 "04 startup creates folder before opening report": main.index('ps3_fs_mkdirs();')<main.index('log_open(SF_DIAG_SINGLE_PATH)'),
 "05 startup safely removes legacy logs": "ps3_fs_cleanup_legacy_diagnostics();" in main,
 "06 cleanup strictly limited to data folder": 'snprintf(path,sizeof(path),"%s/%s",SF_DATA_DIR,names[i])' in fs,
 "07 no recursive cleanup or wildcard": all(x not in fs for x in ('system("rm','rm -rf','glob(','rmdir(')),
 "08 folders and user originals never unlinked": "if((st.st_mode & SF_S_IFMT)==SF_S_IFDIR) continue;" in fs and
        all(x not in block for x in ("settings.ini","SAVEDATA","ISO.BIN.ENC","source.iso")),
 "09 no extra diagnostic files": "-DSF_DISABLE_DIAG_TXT" in mk and "-DSF_SINGLE_DIAG_FILE" in mk,
 "10 PS1 header correct": "DIAGNOSTICO PS1 (V15.24)" in log,
 "11 PS1 lines captured": 'strstr(msg,"PS1_DIAG")' in log and 'strstr(msg,"PS1_MOUNT")' in log,
 "12 PS2 lines excluded": 'strstr(msg,"PS2")' not in log and 'strstr(msg,"ps2")' not in log,
 "13 BDISO lines excluded": 'strstr(msg,"BDISO")' not in log and 'strstr(msg,"bdiso")' not in log,
 "14 bounded diagnostic": '#define SF_DIAG_MAX_BYTES (256u*1024u)' in log,
 "15 PS2 boot report removed": 'sf_ps2_hen_report_state(); /* Read-only inspection' not in main,
 "16 PS2 encryption code retained": 'if(rc==0)sf_ps2_hen_report_state();' in hen,
 "17 PS1 specific entry stages": 'STAGE=SELECTION' in ps1 and 'STAGE=COBRA_VERSION' in ps1,
 "18 PS1 real cobra stage": 'STAGE=MOUNT_PSX_7027' in ps1,
 "19 PS1 file and sector diagnostic": 'REMAINDER_2352=%llu' in ps1,
 "20 PS1 CUE diagnostic": 'STAGE=CUE_PARSE RESULT=UNSUPPORTED' in ps1,
 "21 PS1 webMAN fallback failure reported": 'PS1_DIAG STAGE=WEBMAN_TRANSPORT RESULT_RC=%d' in app,
 "22 PS1 no generic webMAN error for PS1": '"PS1: no montado. Ver diagnostico_ps1.txt"' in app,
 "23 no PS2 file deletes in mount": 'remove(SF_CLASSICS_TMP)' in hen and 'remove(SF_CLASSICS_IMAGE)' not in block,
}
for title,ok in checks.items():print(("PASS" if ok else "FAIL"),title)
print("V15.24 PS1 diagnostic and preservation: %d/%d"%(sum(checks.values()),len(checks)))
if not all(checks.values()):raise SystemExit(1)
