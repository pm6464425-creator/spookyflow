#!/usr/bin/env python3
"""Regression review for the V15.2 missing-movies fault.
Not a substitute for testing removable NTFS hardware on an actual PS3.
"""
from pathlib import Path
def rd(f):return Path(f).read_text("utf-8")
scan=rd("src/core/scanner.c")
ntfs=rd("src/ps3/sf_bdiso_ntfs.c")
usb=rd("src/core/bdiso_usb.c")
test=rd("tests/test_core.c")
app=rd("src/core/app.c")
checks=[
 ("Native NTFS discovery remains", "sf_bdntfs_start(void)" in ntfs),
 ("V15.2 got<20 hard rejection is absent", "got < 20 || !s->ntfs_bdiso_online" not in scan),
 ("Legacy short index sent through physical drive filter", "got > 0 ? s_ntfs_buf : NULL" in scan),
 ("Unknown index header with physical USB accepts movie", "if (port < 0) return 1;" in usb),
 ("No connected USB hides stale index", "if (!present_mask) return 0;" in usb),
 ("Recognized index must match its attached port", "present_mask & (1u << port)" in usb),
 ("Big- and little-endian USB headers covered", "be == usb_id(port) || le == usb_id(port)" in usb),
 ("Host exercises unreadable legacy webMAN index", "test_ntfs_movie_pointer_fallback();" in test),
 ("Host exercises USB removal and reinsertion", "test_usb_movie_hotplug();" in test),
 ("Only BDISO scanner entries filtered", "plat == PLAT_BDISO && s->ntfs_bdiso_online" in scan),
 ("No webMAN prepNTFS network blocking on hotplug", "a->usb_auto_rescan || !a->host.ntfs_refresh" in app),
 ("The Under the Sea 2009 film fix preserved", 'tt1020876/img' in rd("src/ps3/sf_bdiso_art.c")),
 ("2:3 BDISO cover dimensions preserved", "case PLAT_BDISO: return 2.0f/3.0f" in rd("src/core/layout.c")),
]
for label,ok in checks:print(("PASS" if ok else "FAIL"),label)
print("V15.3 physical NTFS restoration:",sum(int(ok) for _,ok in checks),"/",len(checks))
raise SystemExit(not all(ok for _,ok in checks))
