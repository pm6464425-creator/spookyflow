#!/usr/bin/env python3
"""Validate key-free online BDISO poster fallbacks; not hardware network tests."""
from pathlib import Path

def rd(p): return Path(p).read_text(encoding="utf-8")
art=rd("src/ps3/sf_bdiso_art.c")
cat=rd("src/core/movie_catalog.c")
covers=rd("src/core/coverart.c")
ps3=rd("src/ps3/ps3_covers.c")
ntfs=rd("src/ps3/sf_bdiso_ntfs.c")
docs=rd("PORTADAS_3D_FUENTES.md")
checks=[
 ("Public movie search endpoint", "https://v3-cinemeta.strem.io/catalog/movie/top/search=%s.json" in art),
 ("Search does not require API token", "no API key in USB BDISO/COVERS; trying keyless public movie catalog" in art),
 ("Public HTTPS movie search uses the existing transport", "movie_http_json(url,json,&n)" in art),
 ("JSON metadata includes poster and title", 'mc_text(p,stop,"poster"' in cat and 'mc_text(p,stop,"name"' in cat),
 ("Protects from unrelated poster URLs", "mc_host_ok(url)" in cat),
 ("TMDB image host allowed", "https://image.tmdb.org/" in cat),
 ("Cinemeta image proxy host allowed", "https://images.metahub.space/" in cat),
 ("Matches film titles before cover choice", "strcmp(qalias,norm)==0" in cat),
 ("Rejects different known release years", "if(year && y && y!=year)score=0;" in cat),
 ("Chooses best matching search result", "if(score>best)" in cat),
 ("Avoids huge result scans", "seen<60" in cat),
 ("Automatically downloads first public JPEG with valid metadata", '"Cinemeta-no-key"' in art and "movie_http_jpeg(candidates[i].url" in art),
 ("Retries public catalog after broken commercial image URLs", "The public fallback must also run" in art),
 ("Only JPG image payload accepted", "buf[0]==0xff && buf[1]==0xd8" in art),
 ("Preserves user JPGs during compatibility repair", "if(repair)continue;" in art),
 ("Stores art on original external USB", "movie_usb_write(cover,port,jpeg,bytes)" in art),
 ("Never changes NTFS partition selection", "bd_find_partition(u)" in ntfs),
 ("Uses original PS3 background cover loader", "L->io.ensure_movie_art(L->io.ctx,j->path)" in covers),
 ("PS3 native hook remains", "sf_bdiso_ensure_art(path)" in ps3),
 ("API-key movie providers remain optional", "FANART_KEY.TXT" in art and "TMDB_KEY.TXT" in art),
 ("Rescan resets old failed-attempt guard", "sf_bdiso_reset_attempts" in art),
 ("Forum and exact Blu-ray 3D edition remain manual", "COVER" in docs and "3D" in docs),
]
for i,(name,p) in enumerate(checks,1):print(f"{i:02d}. {'PASS' if p else 'FAIL'} {name}")
n=sum(int(ok) for _,ok in checks)
print(f"Keyless cover checks: {n}/{len(checks)}")
raise SystemExit(0 if n==len(checks) else 1)
