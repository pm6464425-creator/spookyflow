#!/usr/bin/env python3
"""BDISO integration: ten focused source-level regression checks.

This is NOT a PS3 hardware runtime test. Hardware still must verify NTFS
media indexing, Cobra Blu-ray playback and the user's TMDB API connectivity.
"""
from pathlib import Path
import re
import sys

ROOT = Path(__file__).resolve().parents[1]
def read(name):
    return (ROOT / name).read_text(encoding="utf-8")
def check(name, cond):
    status = "PASS" if cond else "FAIL"
    print(f"{status}: {name}")
    return bool(cond)

layout = read("src/core/layout.h")
scan = read("src/core/scanner.c")
app = read("src/core/app.c")
ui = read("src/core/ui.c")
mount = read("src/ps3/sf_cobra_mount.c")
host = read("src/ps3/main.c")
ntfs = read("src/ps3/sf_bdiso_ntfs.c")
art = read("src/ps3/sf_bdiso_art.c")
covers = read("src/core/coverart.c")
loader = read("src/ps3/ps3_covers.c")
library = read("src/core/library.c")
testcase = read("tests/test_core.c")

checks = [
    ("1. Fourth tab follows PS1 PS2 PS3",
     "PLAT_BDISO = 3, PLAT_COUNT = 4" in layout and
     '"Blu-ray"' in ui),
    ("2. BDISO scanned from HDD and external USB",
     '"/dev_hdd0/BDISO"' in scan and
     '"BDISO", PLAT_BDISO' in scan),
    ("3. webMAN NTFS BDISO pointer recognized separately",
     '".ntfs[BDISO]"' in scan and
     "PLAT_BDISO" in scan and
     "ntfs[BDISO]" in host),
    ("4. Blu-ray 0x7026 separate from PS3 0x7024",
     "lv2syscall3(8, 0x7026" in mount and
     "MOUNT_PS3ISO_7024" in mount),
    ("5. No unloading occupied VSH plugin slot",
     "cobra_unload_vsh_plugin" not in host and
     "cobra_unload_vsh_plugin" not in ntfs and
     "cobra_unload_vsh_plugin" not in mount),
    ("6. NTFS movies mounted via webMAN indexes, not raw path syscall",
     "sf_bdntfs_has_port" in host and
     "/refresh.ps3?prepntfs" in host and
     "/mount.ps3%s" in host),
    ("7. Native USB covers read in memory, not copied to HDD",
     "sf_bdntfs_load(path,&data,&nbytes" in loader and
     "jpgLoadFromBuffer" in loader and
     "pngLoadFromBuffer" in loader),
    ("8. BDISO raw artwork and metadata stored under external COVERS",
     '"%s/%s.jpg",normaldir,stem' in art and
     "/COVERS/%s.title" in art and
     'j->plat != PLAT_BDISO' in covers),
    ("9. TMDB titles and images requested in Spanish",
     "language=es-MX" in art and
     '"iso_639_1"' in art and
     "json_utf8(" in art),
    ("10. 3D detection by filename remains independent of UI",
     "movie_filename_is_3d(lib_path(a->lib,g))" in ui and
     '"3D", ALIGN_CENTER' in ui and
     "movie3d" not in ui),
    ("11. BDISO shares PS3 case without an added 3D stripe",
     "if (plat == PLAT_BDISO)" in ui and
     '"BLU-RAY 3D"' not in ui and
     '"Blu-ray", ALIGN_CENTER' in ui and
     "float head = h * 0.086f" not in ui),
    ("12. Spanish title applied without changing the ISO file",
     "movie_title_from_usb" in app and
     "lib_set_title" in library and
     'translated[i]' in app),
    ("13. Movie paths validated before writes",
     'strstr(path,"/BDISO/COVERS/")' in ntfs and
     "ps3ntfs_fsync" in ntfs),
    ("14. Existing PS3 regression remains in the host suite",
     "PLAT_PS3" in testcase and
     "PLAT_BDISO" in testcase),
]
passed = sum(check(name, condition) for name, condition in checks)
print(f"BDISO review: {passed}/{len(checks)} passed")
sys.exit(0 if passed == len(checks) else 1)
