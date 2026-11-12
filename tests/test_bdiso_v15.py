#!/usr/bin/env python3
"""V15 non-invasive invariants: BDISO 2:3 fill, verified Rio 2011,
no PS3 game changes, no reuse of the wrong V14 cached poster."""
from pathlib import Path
p=lambda file:Path(file).read_text("utf-8")
layout=p("src/core/layout.c")
case=p("src/ps3/ps3_tabimg.c")
cover=p("src/core/coverart.c")
art=p("src/ps3/sf_bdiso_art.c")
matcher=p("src/core/movie_sensacine.c")
catalog=p("src/core/movie_catalog.c")
coretest=p("tests/test_core.c")
ui=p("src/core/ui.c")
checks=[
 ("BDISO exact 2:3 aspect", "case PLAT_BDISO: return 2.0f/3.0f" in layout),
 ("BDISO case narrowed without editing the PS3 geometry",
  "const float sx = (2.0f/3.0f) / (g[4]/g[5]);" in case and
  "layout_set_case_image(PLAT_BDISO, vw" in case),
 ("PS1 PS2 PS3 aspect constants retained",
  "case PLAT_PS1: return 1.00f" in layout and
  "case PLAT_PS2: return 0.70f" in layout and
  "return 0.86f" in layout),
 ("BDISO poster fills visible area by cropping, no letterboxing",
  "if (cinema_fill)" in cover and "ch = (float)h; cw = ch * aspect" in cover and
  "cw = (float)w; ch = cw / aspect" in cover),
 ("Original game resampling remains unchanged",
  "cover_process_impl(rgba,w,h,aspect,dst,tw,th,0)" in cover),
 ("BDISO alone uses cinema crop resampling",
  "if(j->plat == PLAT_BDISO)" in cover and
  "cover_process_fill(rgba,w,h,cover_art_aspect(j->plat)" in cover),
 ("Test 2:3 geometry and 4 source aspect ratios",
  "test_bdiso_cinema_crop_fill();" in coretest and
  "layout_window_aspect(PLAT_BDISO),2.0/3.0" in coretest),
 ("SensaCine search images gated on title AND year",
  "movie_sensacine_pick_year(" in art and
  "card_year(end+1)!=year" in matcher),
 ("No Rio to Extraño río suffix matching",
  "if(b>=7" in matcher and
  "movie_title_exact(query,\"Rio\")" in art),
 ("Official verified 2011 Blue Sky Rio film ID",
  "pelicula-146550/" in art and
  "movie_sensacine_page_poster(" in art and
  '"SensaCine-Rio2011"' in art),
 ("TMDB movie result matched to query and release year",
  "movie_title_exact(candidate,query)" in art and
  "movie_title_exact(original,query)" in art and
  "(!year || yr==year)" in art),
 ("Cinemeta rejects mismatched or unverified years",
  "if(year && y!=year)score=0" in catalog),
 ("Wrong V14 Rio JPG cannot block new V15 download",
  '"%s/COVERS/POSTERS"' in art and
  '"/POSTERS"' not in cover and
  '"%s/POSTERS"' in cover and
  '"%s/CINE"' not in cover),
 ("Manual posters still win", '"%s/MANUAL"' in cover and
  'n=add(out,n,max,dir,"/",name)' in cover),
 ("Paired Blu-ray and tiny 3D watermarks remain unchanged",
  '"Blu-ray", ALIGN_CENTER' in ui and '"3D", ALIGN_CENTER' in ui),
 ("BDISO cover cache fourth tab preserved",
  "#define CC_TABS             PLAT_COUNT" in p("src/core/cover_cache.h")),
]
for i,(label,ok) in enumerate(checks,1):
 print(f"{i:02d} {'PASS' if ok else 'FAIL'} {label}")
print(f"V15 BDISO movie-cinema regressions {sum(bool(v) for _,v in checks)}/{len(checks)}")
raise SystemExit(not all(v for _,v in checks))
