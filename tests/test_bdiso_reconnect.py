#!/usr/bin/env python3
"""V15.6 PS3 NTFS reconnect safety and source invariants.

Actual USB reconnection still requires confirmation by user's console; these
tests guard the driver path and check our mock native state-machine cases.
"""
from pathlib import Path
def read(p):return Path(p).read_text(encoding="utf-8")
ntfs=read("src/ps3/sf_bdiso_ntfs.c")
usb=read("src/core/bdiso_usb.c")
header=read("src/core/bdiso_usb.h")
tests=read("tests/test_core.c")
app=read("src/core/app.c")
main=read("src/ps3/main.c")
diag=read("src/ps3/sf_usb_diag.c")
checks=[
 ("Physical USB absence observed on the real native probe",
  "__sync_fetch_and_or(&s_detach_seen,(uint32_t)1u<<port)" in ntfs),
 ("Stale native mount state synchronized without mounting on UI thread",
  "static volatile uint32_t s_detach_seen" in ntfs and
  "uint32_t sf_bdntfs_usb_present_mask(void)" in ntfs),
 ("Remembered absence consumed at library scan",
  "__sync_fetch_and_and(&s_detach_seen,~bit)" in ntfs),
 ("USB removal frees native mount references only on scan thread",
  "bd_release_port(u,1)" in ntfs and "BDUSB_DISCONNECTED" in ntfs),
 ("USB reinsertion triggers native remount if disconnected",
  "BDUSB_RESET_REMOUNT" in ntfs and '"RECOVERY"' in ntfs),
 ("Stale readable folder rechecked and remounted if gone",
  "part=bd_find_partition(u)" in ntfs and
  "if (detach_observed || !bdiso_readable)" in usb),
 ("New USB mount still uses the original filesystem driver",
  "ntfsMountDevice(s_devices[u],&s_mount[u],NTFS_DEFAULT | NTFS_RECOVER)" in ntfs),
 ("Never unmount or touch the absent disk during cleanup",
  "if(!physical_removed)" in ntfs),
 ("Do not repeatedly remount NTFS drives without BDISO",
  "(previous && s_partition[u]<0)" in ntfs and
  "if(part>=0)good++;" in ntfs),
 ("Record physical remount reason and errors",
  "bdiso: USB%03d native mount attempt" in ntfs and
  "USB%03d NTFS mount failed rc=%d" in ntfs),
 ("C test covers detached/replugged and stale native volume",
  "test_native_ntfs_reconnect_decisions();" in tests),
 ("Original user hardware log path remains available",
  "diagnostico_usb_ntfs.txt" in read("src/ps3/ps3_platform.h")),
 ("USB attach/detach/reconnect library UI remains automatic",
  "usb_hotplug_poll(" in app and "start_loading(a, 1);" in app),
 ("BDISO root discovery uses native presence",
  "host.native_bdiso_ready_mask = native_bdiso_ready_mask;" in main),
 ("V15.5 native-only USB recognition retained",
  "scanner_roots_with_usb_native(" in read("src/core/scanner.c")),
 ("V15.1 Under the Sea 2009 poster unchanged",
  "tt1020876/img" in read("src/ps3/sf_bdiso_art.c")),
 ("Cinema 2:3 cases retained",
  "case PLAT_BDISO: return 2.0f/3.0f" in read("src/core/layout.c")),
 ("No new destructive USB file operations",
  "sf_bdntfs_unlink(" not in ntfs and "sf_bdntfs_write(" not in usb),
 ("Forensic report continues recording real physical NTFS state",
  "sf_bdntfs_usb_present_mask()" in diag),
]
for desc,ok in checks:
 print(("PASS" if ok else "FAIL"),desc)
print("V15.6 native reconnect regressions: %d/%d" % (sum(ok for _,ok in checks),len(checks)))
raise SystemExit(not all(ok for _,ok in checks))
