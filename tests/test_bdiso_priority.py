#!/usr/bin/env python3
"""Prevents Blu-ray-box scans from returning inside SpookyFlow's own case."""
from pathlib import Path
art=Path("src/ps3/sf_bdiso_art.c").read_text()
covers=Path("src/core/coverart.c").read_text()
ui=Path("src/core/ui.c").read_text()
cc=Path("src/core/cover_cache.c").read_text()
ntfs=Path("src/ps3/sf_bdiso_ntfs.c").read_text()
poster=Path("src/core/movie_sensacine.c").read_text()
checks=[
 ("SensaCine movie search is the first cover source",
   art.index('movie_sensacine_pick(')<art.index('"TMDB-ES"')),
 ("SensaCine movie poster images only, no Blu-ray release pages",
   'www.sensacine.com/buscar/?q=%s' in art and
   'www.mubis.es/titulos/' not in art and
   'images.static-bluray.com/movies/covers/' not in art),
 ("Never download Blu-ray 3D edition art",
   all(x not in art for x in ['"3D-ES-Mubis"', '"3D-ES-Edition"',
                              '"3D-EN-Edition"', '"2D-ES-Mubis"'])),
 ("Strict image poster HTML selector", 'alt="Cartel de' in poster and
    "title_score(" in poster),
 ("Only original SensaCine poster image host accepted",
   '"https://es.web.img"' in poster and '".acsta.net/"' in poster),
 ("Full-size theatrical poster requested before compact fallback",
   art.index('full,"SensaCine-Cartel"') < art.index('thumb,"SensaCine-Compact"')),
 ("Spanish TMDB posters can still be used",
   '"TMDB-ES"' in art),
 ("English theatrical fallback remains if Spanish fails",
   '"Cinemeta-no-key"' in art and '"TMDB-default"' in art),
 ("New poster path uses CINE folder, not older Blu-ray-box NORMAL folder",
   '"%s/COVERS/CINE"' in art and '"%s/%s.jpg",normaldir,stem' in art),
 ("Poster folder is created on NTFS", 'static int movie_cinema_dir(' in art and
    'sf_bdntfs_mkdir(dest)' in art),
 ("Manual poster can be placed alongside ISO",
   'n=add(out,n,max,dir,"/",name)' in covers),
 ("COVERS/MANUAL is explicitly supported",
   '"/MANUAL"' in covers),
 ("New SensaCine poster file path preferred over legacy artwork",
   '"/CINE"' in covers and
   'n=add(out,n,max,covers,"/",name)' not in covers and
   '"/NORMAL"' not in covers),
 ("Old Blu-ray JPGs are not deleted",
   'movie_usb_write(cover,port,jpeg,bytes)' in art and
   '"%s/%s.jpg",normaldir,stem' in art),
 ("Automatic background loader retries when no CINE poster exists",
   'if(!preferred){' in covers and
   'int preferred=movie_has_normal_art(' in covers),
 ("Download Missing Covers does not count legacy Blu-ray JPGs",
   'movie_has_normal_art(io,it->path' in covers),
 ("Ready BDISO textures get refreshed after a download",
   'tab != PLAT_BDISO' in cc),
 ("Native NTFS write restriction remains limited to BDISO/COVERS",
   'strstr(path,"/BDISO/COVERS/")' in ntfs),
 ("Blu-ray right watermark remains faint",
   '"Blu-ray", ALIGN_CENTER' in ui and
   'x + w * 0.74f' in ui and 'alpha * 0.68f' in ui),
 ("3D left watermark only for 3D ISO filenames",
   'movie_filename_is_3d(lib_path(a->lib,g))' in ui and
   'x + w * 0.22f' in ui and '"3D", ALIGN_CENTER' in ui),
 ("No extra case artwork/banner added",
   '"BLU-RAY 3D"' not in ui and
   '0x277BE6FFu' not in ui)
]
for i,(name,ok) in enumerate(checks,1):
 print(f"{i:02d} {'PASS' if ok else 'FAIL'} {name}")
print(f"SensaCine cinema poster invariants: {sum(v for _,v in checks)}/{len(checks)}")
raise SystemExit(not all(v for _,v in checks))
