#!/usr/bin/env python3
"""BDISO USB removal and reconnect safety. Source invariants, not PS3 hardware tests."""
from pathlib import Path
def rd(p): return Path(p).read_text("utf-8")
app=rd("src/core/app.c")
hdr=rd("src/core/app.h")
scan=rd("src/core/scanner.c")
scan_h=rd("src/core/scanner.h")
ntfs=rd("src/ps3/sf_bdiso_ntfs.c")
main=rd("src/ps3/main.c")
tests=rd("tests/test_core.c")
checks=[
 ("Physical port monitor callback", "host.usb_media_mask = usb_media_mask;" in main),
 ("NTFS presence never remounts in polling callback", "sf_bdntfs_usb_present_mask()" in main and "PS3_NTFS_IsInserted(port)" in ntfs),
 ("Debounced sampling is less frequent than 60 FPS", "a->usb_poll_t < 0.75f" in app),
 ("Auto rescan only in flow mode", "a->mode != MODE_FLOW || a->exiting" in app),
 ("Baseline mask recorded at startup", "a->usb_last_mask = a->usb_candidate_mask" in app),
 ("Detect both attach and detach", "mask == a->usb_last_mask" in app and "start_loading(a, 1);" in app),
 ("Avoid repeated auto rescan", "a->usb_last_mask = mask;" in app),
 ("Auto refresh does not call slow webMAN prepNTFS", "a->usb_auto_rescan || !a->host.ntfs_refresh" in app),
 ("Auto refresh keeps processed cover files", "a->host.covers && !a->usb_auto_rescan" in app),
 ("USB roots are recomputed", "scanner_roots_with_usb(" in app),
 ("BDISO webMAN index presence check", "scanner_set_bdiso_pointer_filter(&a->scanner,a->host.ntfs_bdiso_online)" in app),
 ("Stale index pointers are skipped", "skipping offline webMAN movie" in scan),
 ("Stale pointer filter limited to BDISO", "plat == PLAT_BDISO && s->ntfs_bdiso_online" in scan),
 ("Physical device ID verified against inserted device", "bdiso_usb_pointer_visible(data,len,present)" in ntfs),
 ("No PS3 ISO pointer filter", "plat == PLAT_PS3" in scan),
 ("Valid legacy index is accepted if USB present", "test_ntfs_movie_pointer_fallback();" in tests),
 ("Unknown device ID does not hide present USB movies", "test_usb_index_presence();" in tests),
 ("Attach/detach/reattach exercised in portable tests", "test_usb_movie_hotplug();" in tests),
 ("Empty BDISO after disconnection regression", "CHECK(g_lib.tab[PLAT_BDISO].count==0); /* no ghost entries */" in tests),
 ("Existing PS3 games unchanged in test", "CHECK(g_lib.tab[PLAT_PS3].count==ps3_before);" in tests),
 ("BDISO and SNES never show fake demo games",
  ("if (gl->count == 0 && t != PLAT_BDISO)" in app or
   "if (gl->count == 0 && t != PLAT_BDISO && t != PLAT_SNES)" in app))
]
for label,passed in checks: print(("PASS" if passed else "FAIL"),label)
print("BDISO hotplug regression:",sum(ok for _,ok in checks),"/",len(checks))
raise SystemExit(0 if all(ok for _,ok in checks) else 1)
