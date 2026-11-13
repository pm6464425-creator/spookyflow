#!/usr/bin/env python3
"""V15.18 one log, strict cleanup and useful PS2 Classics diagnosis."""
from pathlib import Path
import re
root=Path(__file__).resolve().parents[1]
def src(p):return (root/p).read_text(encoding="utf-8")
hdr=src("src/ps3/ps3_platform.h")
ps3fs=src("src/ps3/ps3_fs.c")
log=src("src/core/log.c")
main=src("src/ps3/main.c")
hen=src("src/ps3/sf_ps2_hen.c")
build=src("ci/build_ps3.sh")
mk=src("Makefile")
allowlist={
 "log.txt", "diagnostico_portadas.txt", "diagnostico_usb_ntfs.txt",
 "cobra_startup_probe.txt", "cobra_selection_probe.txt", "cobra_last_mount.txt",
 "cobra_trace.txt", "cobra_previous_interruption.txt", "ps2_hen_last.txt",
 "ps2_launcher_probe.txt", "ps2_last_mount.txt", "ps2_mount_trace.txt"
}
block=ps3fs.split('static const char *const names[]={',1)[1].split('};',1)[0]
entries=set(re.findall(r'"([^"]+)"',block))
checks={
 "01 exactly one central report name": '#define SF_DIAG_SINGLE_PATH SF_DATA_DIR "/diagnostico.txt"' in hdr,
 "02 same report reused on every app start": 'g_log=(path && path[0])?fopen(path,"wb"):NULL;' in log,
 "03 central report is opened after creating runtime data directory": main.index('ps3_fs_mkdirs();') < main.index('log_open(SF_DIAG_SINGLE_PATH)'),
 "04 cleanup is called during every startup": 'ps3_fs_cleanup_legacy_diagnostics();' in main,
 "05 cleanup strict list with 12 known reports": entries==allowlist and len(entries)==12,
 "06 cleanup confined to SpookyFlow data dir": 'snprintf(path,sizeof(path),"%s/%s",SF_DATA_DIR,names[i])' in ps3fs,
 "07 cleanup never removes directories": 'if((st.st_mode & SF_S_IFMT)==SF_S_IFDIR) continue;' in ps3fs,
 "08 no recursive deletion or glob": all(x not in ps3fs for x in ('system("rm','rm -rf','readdir','glob(','rmdir(')),
 "09 preserves game blank ISO and original settings": 'sf_blank.iso' not in block and 'settings.ini' not in block,
 "10 preserves folder music covers downloads": all(x not in block for x in ('covers','music','downloaded','tabs')),
 "11 existing TXT writers remain disabled": '-DSF_DISABLE_DIAG_TXT' in mk and '-DSF_SINGLE_DIAG_FILE' in mk,
 "12 legacy standalone PS2 diagnostics are not produced": '#ifdef SF_DISABLE_DIAG_TXT' in hen and '#ifndef SF_DISABLE_DIAG_TXT' in hen,
 "13 central diagnostic bounded at 256 KB": '#define SF_DIAG_MAX_BYTES (256u*1024u)' in log,
 "14 central diagnostic filters per-frame logs": 'strstr(msg,"PS2")' in log and 'strstr(msg,"error")' in log,
 "15 central diagnostic full session not each stage": 'fopen(path,"wb")' in log and 'log_printf("PS2_DIAG STAGE=%s' in hen,
 "16 inspect callable at application boot": 'sf_ps2_hen_report_state();' in main,
 "17 inspect called after successful encryption": 'if(rc==0)sf_ps2_hen_report_state();' in hen,
 "18 scan encrypted image header and content ID": 'PS2_DIAG HEADER_MAGIC_OK=%d CONTENT_ID_OK=%d' in hen,
 "19 check actual encrypted size against header": 'size==expected_size(vsize)' in hen,
 "20 authenticate encrypted first and last segments": 'SAMPLE_%s BLOCK=%llu HASH_OK=%d INDEX_OK=%d' in hen,
 "21 examine LIMG disc type and sector size": 'LIMG_PRESENT=%d DISC_TYPE=%u SECTORS=%u SECTOR_SIZE=%u VALID=%d' in hen,
 "22 inspect encrypted CONFIG presence and validity": 'PS2_DIAG CONFIG_PRESENT=%d CONFIG_ENCRYPTED_VALID=%d' in hen,
 "23 inspect CONFIG's actual game ID origin": 'PS2_DIAG CONFIG_ORIGIN=%.300s' in hen,
 "24 preserve source ISO and inspect manifest": 'PS2_DIAG SOURCE_MANIFEST=%.500s' in hen,
 "25 preserve pending backup and original": 'PS2_DIAG PENDING_PARTIAL=%d OLD_BACKUP_PENDING=%d' in hen,
 "26 acknowledge cannot capture external launcher execution": 'PS2_DIAG EXTERNAL_LAUNCH_STATUS=NOT_OBSERVABLE_FROM_SPOOKYFLOW' in hen,
 "27 no webMAN dependency in new diagnostics": all(x not in hen for x in ('webman_get','ps3_webman_send','127.0.0.1')),
 "28 strict mandatory mbedTLS still builds": 'make pkg SF_TLS=1' in build,
}
for title,ok in checks.items():print(("PASS" if ok else "FAIL"),title)
print("V15.18 unique diagnostic checks %d/%d"%(sum(checks.values()),len(checks)))
if not all(checks.values()):raise SystemExit(1)
