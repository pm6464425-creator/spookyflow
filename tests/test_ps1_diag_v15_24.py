#!/usr/bin/env python3
"""PS1 V15.24: prevent blind fallback and ensure diagnostic reports true mount errors."""
from pathlib import Path
root=Path(__file__).resolve().parents[1]
mnt=(root/"src/ps3/sf_ps1_mount.c").read_text()
app=(root/"src/core/app.c").read_text()
hdr=(root/"src/ps3/ps3_platform.h").read_text()
fs=(root/"src/ps3/ps3_fs.c").read_text()
log=(root/"src/core/log.c").read_text()
main=(root/"src/ps3/main.c").read_text()
checks={
 "01 native mount does not reject Netemu selection": "if(!is_ps1_path(path) || emulator==1)return -41;" not in mnt,
 "02 Netemu option logged without falsifying forcing": "NATIVE_COBRA_DOES_NOT_FORCE_EMU" in mnt,
 "03 native route for selected PS1": "result=a->host.launch_ps1(path,c.emulator)" in app,
 "04 source logged on select": "PS1_DIAG STAGE=SELECTION GAME=%s PATH=%s" in app,
 "05 root validation logged": "RESULT=UNSUPPORTED_ROOT" in mnt,
 "06 unreadable CUE reports errno": "STAGE=CUE_OPEN RESULT=FAIL ERRNO=%d" in mnt,
 "07 invalid CUE records multi BIN": "STAGE=CUE_PARSE RESULT=UNSUPPORTED FILE_COUNT=%d" in mnt,
 "08 missing image logs errno": "STAGE=IMAGE_STAT RESULT=FAIL ERRNO=%d" in mnt,
 "09 invalid sector diagnostics": "REMAINDER_2352=%llu" in mnt,
 "10 Cobra firmware probe logs actual rc": "STAGE=COBRA_VERSION SYS_RC=%d" in mnt,
 "11 Cobra disc type logs rc": "STAGE=DISC_TYPE RESULT=ERROR SYS_RC=%d" in mnt,
 "12 native Cobra PSX call result recorded": "STAGE=MOUNT_PSX_7027 SYS_RC=%d" in mnt,
 "13 eject/insert logged": "STAGE=FAKE_EJECT SYS_RC=%d" in mnt and "STAGE=FAKE_INSERT SYS_RC=%d" in mnt,
 "14 rollback logged": "STAGE=ROLLBACK RESULT=ERROR RC=%d" in mnt,
 "15 webMAN fallback result logged": "PS1_DIAG STAGE=WEBMAN_TRANSPORT RESULT_RC=%d" in app,
 "16 no misleading generic error for PS1": '"PS1: no montado. Ver diagnostico_ps1.txt"' in app,
 "17 no forced webMAN for native success": 'app_request_exit(a,"ps1 cobra mount");' in app,
 "18 original PS2 backend retained": 'host.launch_ps2 = sf_ps2_mount;' in main,
 "19 original PS3 backend retained": 'host.launch_ps3 = sf_cobra_mount_ps3;' in main,
 "20 BDISO retained": 'host.launch_bdiso = launch_bdiso;' in main,
 "21 PS1 report path": 'SF_DATA_DIR "/diagnostico_ps1.txt"' in hdr,
 "22 old report allowlist": '"diagnostico.txt"' in fs,
 "23 only PS1-related messages persisted": 'strstr(msg,"PS1_DIAG")' in log and 'strstr(msg,"PS2")' not in log,
 "24 report truncates at every launch": 'fopen(path,"wb")' in log,
 "25 no previous PS2 forensic dump at startup": 'sf_ps2_hen_report_state(); /* Read-only inspection' not in main,
}
for title,ok in checks.items():print(("PASS" if ok else "FAIL"),title)
print("V15.24 PS1 focused diagnostics: %d/%d"%(sum(checks.values()),len(checks)))
if not all(checks.values()):raise SystemExit(1)
