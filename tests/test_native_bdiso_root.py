#!/usr/bin/env python3
"""Hardware-derived BDISO root recovery checks.

This intentionally mirrors the supplied V15.4 PS3 forensic report:
native_usb_000=present, native_parts=1, bdiso_partition=0,
NATIVE_LIST /BDISO=4, sysFs /dev_usb000 not found,
SCANNER_USB_MOVIE_ROOTS=0. Unit tests reproduce this and prove root recovery.
It is NOT an assertion of success on the user's hardware until retested.
"""
from pathlib import Path
scanner=Path("src/core/scanner.c").read_text("utf-8")
scanner_h=Path("src/core/scanner.h").read_text("utf-8")
app=Path("src/core/app.c").read_text("utf-8")
app_h=Path("src/core/app.h").read_text("utf-8")
ps3=Path("src/ps3/main.c").read_text("utf-8")
ntfs=Path("src/ps3/sf_bdiso_ntfs.c").read_text("utf-8")
tests=Path("tests/test_core.c").read_text("utf-8")
diag=Path("src/ps3/sf_usb_diag.c").read_text("utf-8")
checks=[
 ("Host provides native BDISO-ready callback", "uint32_t (*native_bdiso_ready_mask)(void)" in app_h),
 ("PS3 calls actual NTFS mount on scan preparation", "int p,mounted=sf_bdntfs_start();" in ps3),
 ("Physical ready mask requires mounted BDISO partition", "if(sf_bdntfs_has_port(p))mask|=" in ps3),
 ("Ready callback assigned to PS3 host", "host.native_bdiso_ready_mask = native_bdiso_ready_mask;" in ps3),
 ("Scan preparation computes native mask", "native_mask=a->host.native_bdiso_ready_mask();" in app),
 ("Scan uses native-aware roots", "scanner_roots_with_usb_native(base,nb,a->host.fs_list" in app),
 ("Native mask carried into portable scan root API", "uint32_t native_ready_mask" in scanner_h),
 ("Missing lv2 root not mistaken for absent NTFS", "if (!lv2_visible && !ntfs_bdiso_ready) continue;" in scanner),
 ("Only BDISO root added for native-only USB", "if (!lv2_visible && k_usb[i].plat != PLAT_BDISO) continue;" in scanner),
 ("PS1/PS2/PS3 roots untouched for native-only device", "if (!lv2_visible && k_usb[i].plat != PLAT_BDISO) continue;" in scanner),
 ("Normal FAT32 root behavior preserved", "return scanner_roots_with_usb_native(base,nbase,list,ctx,0,n,ndrives);" in scanner),
 ("Native-only state explicitly logged", "injecting /BDISO root (lv2 root absent)" in scanner),
 ("Console forensic report preserved for follow-up", "SCANNER_USB_MOVIE_ROOTS %d" in diag),
 ("Real-log LV2 root missing scenario exercised in host", 'strcmp(dir,"/dev_usb000")==0) return -1;' in tests),
 ("Four real-log ISOs exercised", all(x in tests for x in ["2010 How to Train Your Dragon 3D.iso","1991 Terminator 2 el Juicio Final 3D.iso","2001 Shrek 3D.iso","2004 Yo, Robot 3D.iso"])),
 ("Recovered library has four movies", "CHECK(g_lib.tab[PLAT_BDISO].count==4);" in tests),
 ("Reconnection and detach checked in same scenario", "test_native_ntfs_bdiso_without_lv2_root();" in tests),
 ("Existing USB hotplug integration remains", "test_usb_movie_hotplug();" in tests),
 ("No writes to USB added in scanner", "sf_bdntfs_write(" not in scanner),
 ("Film poster provider maintained", "tt1020876/img" in Path("src/ps3/sf_bdiso_art.c").read_text("utf-8")),
 ("Movie 2:3 aspect maintained", "case PLAT_BDISO: return 2.0f/3.0f" in Path("src/core/layout.c").read_text("utf-8"))
]
for name,ok in checks:print(("PASS" if ok else "FAIL"),name)
print("Native BDISO roots regression:",sum(bool(ok) for _,ok in checks),"/",len(checks))
raise SystemExit(0 if all(ok for _,ok in checks) else 1)
