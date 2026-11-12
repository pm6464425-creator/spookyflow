#!/usr/bin/env python3
"""Regression guard: Under the Sea 3D (2009) gets an IMDb-verified
keyless poster candidate.  These source checks do not emulate the PS3."""
from pathlib import Path

art=Path("src/ps3/sf_bdiso_art.c").read_text("utf-8")
layout=Path("src/core/layout.c").read_text("utf-8")
covers=Path("src/core/coverart.c").read_text("utf-8")
tests=[
    ("Spanish ISO alias", '{"Bajo el mar","Under the Sea"}' in art),
    ("No mismatch against later same-name films",
     'if(year==2009 && movie_title_exact(query,"Under the Sea"))' in art),
    ("Verified film ID", 'tt1020876/img' in art),
    ("HTTPS image source", 'https://images.metahub.space/poster/medium/tt1020876/img' in art),
    ("Downloader accepts keyless candidates", 'movie_http_jpeg(candidates[i].url,jpeg,&bytes)' in art),
    ("PNG fallback for PS3 JPEG compatibility", 'movie_http_compatible_png(candidates[i].url,jpeg,&bytes)' in art),
    ("Manual posters still prioritized", "caratula manual preservada" in art),
    ("Poster stays in BDISO/COVERS/POSTERS", '"%s/COVERS/POSTERS"' in art),
    ("BDISO remains 2:3", "case PLAT_BDISO: return 2.0f/3.0f" in layout),
    ("PS1-3 original resampler unchanged", "cover_process_impl(rgba,w,h,aspect,dst,tw,th,0)" in covers)
]
for label,passed in tests: print(("PASS" if passed else "FAIL"),label)
print("Under the Sea 2009 identity regression: %d/%d" % (sum(ok for _,ok in tests),len(tests)))
raise SystemExit(0 if all(ok for _,ok in tests) else 1)
