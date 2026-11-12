#!/usr/bin/env python3
"""Read-only PS3 USB/NTFS forensic build assertions.
These checks are NOT proof of PS3 hardware behavior; the TXT from the console is.
"""
from pathlib import Path
def rd(p):return Path(p).read_text("utf-8")
probe=rd("src/ps3/sf_usb_diag.c")
main=rd("src/ps3/main.c")
ntfs=rd("src/ps3/sf_bdiso_ntfs.c")
app=rd("src/core/app.c")
host=rd("src/core/app.h")
test=rd("tests/test_core.c")
headers=rd("src/ps3/ps3_platform.h")
checks=[
("Diagnostic location on internal HDD", 'SF_USB_DIAG_PATH SF_DATA_DIR "/diagnostico_usb_ntfs.txt"' in headers),
("Report generated at start", "sf_usb_diag_begin()" in main),
("Probe before native mount", 'sf_usb_diag_snapshot("PRE_NATIVE_NTFS"' in main),
("Probe after native mount", 'sf_usb_diag_snapshot("POST_NATIVE_NTFS"' in main),
("Real physical NTFS USB mask recorded", "sf_bdntfs_usb_present_mask()" in probe),
("Per-port sysFS root status recorded", 'lv2_root_stat=%d' in probe),
("Per-port NTFS mount return recorded", "last_mount_rc=%d" in probe),
("Native partition index recorded", "bdiso_partition=%d" in probe),
("Real native BDISO listing recorded", "sf_bdntfs_list(bd,list,4)" in probe),
("webMAN index counted", "WEBMAN_TOTAL movies_BDISO" in probe),
("webMAN pointer identity checked", "bdiso_usb_pointer_visible(" in probe),
("Scanner roots shown", "SCANNER_ROOT kind=" in probe),
("Scanner finds visible BDISO movies", "LIBRARY BDISO_visible=%d" in probe),
("Separate initial / hotplug / manual snapshots", '?"HOTPLUG_RESCAN":"MANUAL_RESCAN"' in app and "INITIAL_SCAN" in app),
("Always flush and close report", "fflush(f);" in probe and "fclose(f);" in probe),
("Avoid webMAN network refresh from snapshot", "ps3_webman_get" not in probe and "ntfs_refresh(" not in probe),
("Diagnostic does not call native NTFS remount explicitly", "sf_bdntfs_start(" not in probe and "ntfsMountDevice(" not in probe),
("Reports do not write to USB", 'fopen(SF_USB_DIAG_PATH,"a")' in probe and "sf_bdntfs_write" not in probe),
("PS3 tests cover four scan stages", "CHECK(g_usb_diag_events==4)" in test),
("Previous V15.3 USB movie hotplug preserved", "usb_hotplug_poll(" in app),
("Previous Under the Sea movie fix preserved", "tt1020876/img" in rd("src/ps3/sf_bdiso_art.c")),
("Previous BDISO 2:3 art preserved", "case PLAT_BDISO: return 2.0f/3.0f" in rd("src/core/layout.c"))
]
for label,ok in checks:print(("PASS" if ok else "FAIL"),label)
print("PS3 forensic USB diagnostic checks:",sum(bool(ok) for _,ok in checks),"/",len(checks))
raise SystemExit(not all(ok for _,ok in checks))
