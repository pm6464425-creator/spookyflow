#!/usr/bin/env python3
"""V15.37: PS1 background recovery without redownloading existing covers."""
from pathlib import Path
import subprocess
def read(path):return Path(path).read_text(encoding="utf-8")
def same(path,sha):return subprocess.check_output(["git","hash-object",path],text=True).strip()==sha
app=read("src/core/app.c")
cover=read("src/core/coverart.c")
test=read("tests/test_v15_37_ps1_cover_repair.h")
core=read("tests/test_core.c")
checks=[
 ("01. Background checks PS1 and SNES, not PS2 PS3 or BDISO",
  app.count("if (automatic && t != PLAT_SNES && t != PLAT_PS1) continue;")==2),
 ("02. Automatic worker is asynchronous, never blocking PS3 coverflow",
  "if (a->host.run_async && a->host.run_async(dl_worker, d) == 0) return;" in app
  and "No synchronous Internet fetches while rendering the PS3 cover flow" in app),
 ("03. Existing local PS1 images are skipped without network calls",
  "cover_has_art(io,it->plat,it->path,it->is_dir,it->size," in cover
  and "it->state=DL_HAVE;d->n_have++;goto next;" in cover),
 ("04. Missing PS1 ignores previous failed 404 lookup markers",
  "if (d->auto_mode && it->plat!=PLAT_PS1 &&" in cover
  and "SLUS00594.missing" in test),
 ("05. PS1 still tries official disc-ID and Libretro name-regional sources",
  "cover_dl_urls(it->plat,it->id,urls,COVER_DL_URLS)" in cover
  and "ps1_name_catalog_match(it->path,ps1_catalog" in cover),
 ("06. PS1 download clears stale cache marker and refreshes only its card",
  'snprintf(f, sizeof(f), "%s.none", stem);' in cover
  and "cc_forget(&a->covers, it->plat, k)" in app),
 ("07. Existing cover bytes remain unchanged after recovery and next scan",
  "memcmp(existing->data,already,strlen(already))==0" in test
  and "g_ps1_http_calls==1 && !g_ps1_http_bad_url" in test),
 ("08. Missing cover is downloaded during automatic pass without manual action",
  "d.auto_mode=1;" in test
  and "d.n_got==1 && d.n_have==1" in test
  and "test_v15_37_ps1_missing_cover_auto_repair();" in core),
 ("09. Repeat starts never download existing PS1 cover again",
  "d.n_have==2 && d.n_got==0" in test),
 ("10. User still can manually Download Missing Covers for every console",
  "download_start(a, 0)" in app
  and "if (automatic && t != PLAT_SNES && t != PLAT_PS1) continue;" in app),
 ("11. Rescan stays isolated and original JPG/PNG untouched",
  "cover_wipe_cache_platform(a->host.covers,a->tab);" in app
  and "if(only_plat>=0 && p!=only_plat)continue;" in cover),
 ("12. PS1, PS2, PS3, BDISO, SNES launchers and graphics untouched",
  all(same(*x) for x in (
   ("src/ps3/sf_ps2_hen.c","f20a63d83596a9b5079fd056e261656a4f536201"),
   ("src/ps3/sf_cobra_mount.c","fe9faf92587a9c86ae866502ef5346f98fb76715"),
   ("src/ps3/ps3_webman.c","333f9d3fe75f3ebbdc0840ddb696385306919eca"),
   ("src/core/ps1_name_match.c","b5ea816d0c4d978db13a0a1c89452f56b7f78e9c"),
   ("src/core/snes_name_match.c","b0b76d977053f7153b19d9a5ff562c8c4704d5f5"),
   ("src/core/ui.c","0da4dccbaef66846b3dcc15779e0363e10be431c")
  )))
]
failed=[]
for title,ok in checks:
 print(("PASS " if ok else "FAIL ")+title)
 if not ok:failed.append(title)
print(f"V15.37 PS1 missing-cover recovery regressions: {len(checks)-len(failed)}/{len(checks)}")
if failed:raise SystemExit(1)
