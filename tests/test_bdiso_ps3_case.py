#!/usr/bin/env python3
"""Static rendering regression checks for BDISO case styling.
These checks do not claim graphical validation on PS3 hardware.
"""
from pathlib import Path

def src(path):
    return Path(path).read_text(encoding="utf-8")

ui = src("src/core/ui.c")
tabimg = src("src/ps3/ps3_tabimg.c")
layout = src("src/core/layout.c")
ntfs = src("src/ps3/sf_bdiso_ntfs.c")
main = src("src/ps3/main.c")
scanner = src("src/core/scanner.c")
art = src("src/ps3/sf_bdiso_art.c")

before_overlay, after_overlay = ui.split('/* Preserve the original case: two SMALL translucent watermarks at', 1)
checks = [
    ("BDISO remains the fourth tab", '"BDISO"' in ui),
    ("PS3 case texture remains loaded normally", "ui_set_case_image(plat, TEX_CASE_PS1 + plat);" in tabimg),
    ("PS3 case texture shared with BDISO", "ui_set_case_image(PLAT_BDISO, TEX_CASE_PS3)" in tabimg),
    ("PS3 case geometry copied to BDISO", "layout_set_case_image(PLAT_BDISO" in tabimg),
    ("case image shared, not loaded twice", "for (p = 0; p < PLAT_BDISO; p++)" in tabimg),
    ("same custom PS3 case reused", 'SF_CASES_DIR "/ps3.png"' in tabimg),
    ("PS3 case remains controlled by Custom Cases setting", "(plat == PLAT_BDISO || plat == PLAT_SNES || g_cases_on)" in layout),
    ("BDISO case remains visible irrespective of case toggle", "plat == PLAT_BDISO || plat == PLAT_SNES || g_cases_on" in layout),
    ("solid blue old banner removed", "0x277BE6FFu" not in ui and "0x08357CFFu" not in ui),
    ("old blue side spine removed", "0x4C9EE5FFu" not in ui and "0x103E79FFu" not in ui),
    ("BDISO placeholder has no extra blue header", "if (plat != PLAT_BDISO)" in ui),
    ("Blu-ray fallback watermark retained", '"Blu-ray", ALIGN_CENTER' in after_overlay),
    ("Blu-ray fallback watermark is translucent", "alpha * 0.68f" in after_overlay),
    ("No unwanted 3D stripe on the case", '"BLU-RAY 3D"' not in after_overlay and '"3D", ALIGN_CENTER' in after_overlay),
    ("3D ISO filename gate remains", "movie_filename_is_3d(lib_path(a->lib,g))" in ui),
    ("movie and game mounts still separate", "host.launch_bdiso" in main and "host.launch_ps3" in main),
    ("NTFS reader still included", "int sf_bdntfs_start(void)" in ntfs),
    ("BDISO directory scanner retained", '"BDISO", PLAT_BDISO' in scanner),
]
for number,(name,ok) in enumerate(checks,1):
    print(f"{number:02d}. {'PASS' if ok else 'FAIL'} - {name}")
passed=sum(1 for _,ok in checks if ok)
print(f"BDISO visual checks: {passed}/{len(checks)}")
raise SystemExit(0 if passed==len(checks) else 1)
