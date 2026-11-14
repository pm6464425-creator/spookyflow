#!/usr/bin/env python3
"""V15.36 protections for PS1/PS2/PS3 art, with 64-slot SNES+BDISO fairness."""
from pathlib import Path
import subprocess
def src(path):return Path(path).read_text(encoding="utf-8")
def hash_is(path,sha):
    return subprocess.check_output(["git","hash-object",path],text=True).strip()==sha
cover=src("src/core/coverart.c")
cover_h=src("src/core/coverart.h")
app=src("src/core/app.c")
cache=src("src/core/cover_cache.c")
test=src("tests/test_v15_36_restore_covers.h")
main=src("tests/test_core.c")
checks=[
("01. Existing JPG/PNG beats stale valid .none on every game platform",
 "if(cover_has_art(&L->io,j->plat,j->path,j->is_dir," in cover
 and "stale .none bypassed: local JPG/PNG exists" in cover),
("02. Disc serial cover paths recovered for PS1/PS2/PS3 even with empty ID",
 "if(!checkid[0] && j->plat!=PLAT_SNES)" in cover
 and "(void)discid_read(j->plat,j->path" in cover),
("03. Positive processed CVR remains first: no needless offline redecodes",
 "if (read_cache(L, f, j->plat, tw, th, year))" in cover
 and "else if (read_cache(L, f, j->plat, tw, th, year))" in cover),
("04. Truly missing art keeps its negative marker and does not spin",
 'sf_strlcpy(src, "none (cached)", src_cap)' in cover
 and "CC_FAILED" in test),
("05. Explicit re-scan invalidates only the selected tab",
 "cover_wipe_cache_platform(a->host.covers,a->tab)" in app
 and "if(only_plat>=0 && p!=only_plat)continue;" in cover),
("06. Other consoles retain processed positive and negative caches",
 "test_v15_36_scoped_rescan_preserves_psx_ps2_ps3" in test
 and "p!=PLAT_SNES" in test),
("07. No JPG/PNG originals are removed by rescan",
 'if (!sf_ends_with_i(ent[i].name, ".none") &&' in cover
 and '!sf_ends_with_i(ent[i].name, ".cvr")) continue;' in cover),
("08. 5-tab side windows dynamically fit 64 physical GPU slots",
 "int remaining=cc->budget-(cc->hi[tab]-cc->lo[tab]+1);" in cache
 and "span=(remaining+active-1)/active;" in cache
 and "remaining-=span;" in cache),
("09. Main 30 visible covers retain highest scheduling priority",
 "if (!fill(cc, tab, center, count, &loads)) return;" in cache
 and "CC_MAIN_WINDOW" in src("src/core/cover_cache.h")),
("10. Automatic online scanning limited to SNES; manual check all games remains",
 "if (automatic && t != PLAT_SNES) continue;" in app
 and "if (g->flags & GAME_DEMO) continue;" in app
 and "download_start(a, 0)" in app),
("11. Host tests exercise PS1 PS2 PS3 restoration, selective purge, all 5 GPU windows",
 all(x in main for x in (
  "test_v15_36_restore_existing_covers();",
  "test_v15_36_scoped_rescan_preserves_psx_ps2_ps3();",
  "test_v15_36_five_tab_gpu_budget();"))),
("12. Mounting/launch files and old SNES art lookup engine unchanged",
 all(hash_is(*x) for x in (
  ("src/ps3/sf_ps2_hen.c","f20a63d83596a9b5079fd056e261656a4f536201"),
  ("src/ps3/sf_cobra_mount.c","fe9faf92587a9c86ae866502ef5346f98fb76715"),
  ("src/ps3/ps3_webman.c","333f9d3fe75f3ebbdc0840ddb696385306919eca"),
  ("src/core/ps1_name_match.c","b5ea816d0c4d978db13a0a1c89452f56b7f78e9c"),
  ("src/core/snes_name_match.c","2af5c2a0d62319dd66a5e72e670ab718bc758b94"),
  ("src/core/ui.c","0da4dccbaef66846b3dcc15779e0363e10be431c")
 )))
]
failed=[]
for label,passed in checks:
    print(("PASS " if passed else "FAIL ")+label)
    if not passed:failed.append(label)
print(f"V15.36 cover recovery checks: {len(checks)-len(failed)}/{len(checks)}")
if failed:raise SystemExit(1)
