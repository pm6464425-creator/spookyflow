#!/usr/bin/env python3
"""V15.7: hardware-log-derived FlowMovie 3D USB NTFS reconnect checks.

Reference implementation: pm6464425-creator/3D-ISO-PLAYER-PS3
source/main.cpp refresh_usb_state, mount_port, release_port.
A real V15.6 PS3 log showed 6 ntfsMountDevice==0 results followed by a
successful mount AFTER the scanner had frozen ready_mask at zero.
"""
from pathlib import Path
def read(f):return Path(f).read_text("utf-8")
app=read("src/core/app.c")
hdr=read("src/core/app.h")
ntfs=read("src/ps3/sf_bdiso_ntfs.c")
sc=read("src/core/scanner.c")
core=read("tests/test_core.c")
diag=read("src/ps3/sf_usb_diag.c")
checks=[
 ("Live-app remembers newly inserted native ports",
  'a->usb_ntfs_pending_mask | ((mask & ~a->usb_last_mask) & 0xffu)' in app),
 ("App retries before it snapshots USB scan roots",
  app.index("Waiting for USB NTFS")<app.index("scanner_roots_with_usb_native(base,nb")),
 ("Two mount attempts cannot finalize the library",
  "if(a->usb_ntfs_retry_attempts < 12) break;" in app),
 ("Retries are spaced instead of 6 calls in the same millisecond",
  "a->usb_ntfs_retry_t < 0.60f" in app),
 ("Earlier USB content remains stable while waiting",
  app.index("Waiting for USB NTFS") < app.index("lib_clear(a->lib);")),
 ("Ready mask is reused from actual successful probe",
  "native_mask=a->usb_ntfs_probed_mask;" in app),
 ("Still mounted but late USB can trigger automatic library rebuilding",
  "bdiso: late native NTFS mount became ready" in app and
  "start_loading(a,1);" in app),
 ("Pending retries cleared after mount readiness verified",
  "a->usb_ntfs_pending_mask &= ~a->usb_ntfs_probed_mask;" in app),
 ("Removed USB cancels pending mount requests",
  "a->usb_ntfs_pending_mask | ((mask & ~a->usb_last_mask) & 0xffu)) & mask;" in app),
 ("Legacy path no longer mounts after scanner freezes readiness",
  'if(strlen(dir)==11 && strncmp(dir,"/dev_usb00",10)==0)\n        (void)sf_bdntfs_start();' not in ntfs),
 ("Native volume discovery still happens via explicit callback",
  "native_mask=a->host.native_bdiso_ready_mask();" in app),
 ("Same driver and mount flags as FlowMovie retained",
  "ntfsMountDevice(s_devices[u],&s_mount[u],NTFS_DEFAULT | NTFS_RECOVER)" in ntfs),
 ("C regression tests 6 fails then later successful native mount",
  "test_flowmovie_delayed_usb_mount(6);" in core),
 ("C regression tests late success AFTER 12 attempts",
  "test_flowmovie_delayed_usb_mount(15);" in core),
 ("C regression tests non-BDISO device has bounded loading",
  "test_flowmovie_missing_bdiso_does_not_hang();" in core),
 ("Physical removal still drops movie library",
  "test_native_ntfs_bdiso_without_lv2_root();" in core),
 ("No new destructive USB operations in app",
  "ps3ntfs_unlink" not in app and "ntfsUnmount(" not in app),
 ("USB diagnostics preserved",
  "HOTPLUG_RESCAN" in app and "WEBMAN_TOTAL movies_BDISO" in diag),
 ("Movies keep cinema aspect and title fixes",
  "case PLAT_BDISO: return 2.0f/3.0f" in read("src/core/layout.c") and
  "tt1020876/img" in read("src/ps3/sf_bdiso_art.c"))
]
for name,ok in checks:print(("PASS" if ok else "FAIL"),name)
print("FlowMovie native NTFS retry recovery:",sum(int(ok) for _,ok in checks),"/",len(checks))
raise SystemExit(not all(ok for _,ok in checks))
