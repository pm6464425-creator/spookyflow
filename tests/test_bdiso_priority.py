#!/usr/bin/env python3
"""Prevent recurrence of inappropriate 3D-edition art, extra banners or
accidental deletion of already downloaded/manually supplied movie covers."""
from pathlib import Path
art=Path("src/ps3/sf_bdiso_art.c").read_text()
covers=Path("src/core/coverart.c").read_text()
ui=Path("src/core/ui.c").read_text()
cc=Path("src/core/cover_cache.c").read_text()
ntfs=Path("src/ps3/sf_bdiso_ntfs.c").read_text()
tests=[
 ("Only normal 2D Spanish + English sources", all(x not in art for x in
    ['"3D-ES-Mubis"', '"3D-ES-Edition"', '"3D-EN-Edition"',
     '"/%s-blu-ray-3d"', "movie_edition_bluray_id("])),
 ("Spanish TMDB movie poster available", '"TMDB-ES"' in art),
 ("Spanish standard Mubis movie poster available", '"2D-ES-Mubis"' in art),
 ("English Cinemeta last resort available", '"Cinemeta-no-key"' in art),
 ("Spanish normal ahead of English poster", art.index('"2D-ES-Mubis"')<art.index('"TMDB-default"')),
 ("3D printed only as a transparent case watermark", '"3D", ALIGN_CENTER' in ui and
    "movie_filename_is_3d(lib_path(a->lib,g))" in ui and
    'alpha * 0.68f' in ui),
 ("Blu-ray original watermark unchanged", '"Blu-ray", ALIGN_CENTER' in ui),
 ("3D text opposite Blu-ray", 'x + w * 0.22f' in ui and 'x + w * 0.74f' in ui),
 ("No new case banner or opaque 3D stripe", '"BLU-RAY 3D"' not in ui and
    'visible,ALIGN_CENTER,win.w*0.91f' not in ui),
 ("Downloaded normal art gets a distinct folder", '"%s/COVERS/NORMAL"' in art and
    '"%s/%s.jpg",normaldir,stem' in art),
 ("Normal folder created on NTFS", 'static int movie_normal_dir(' in art and
    'sf_bdntfs_mkdir(dest)' in art),
 ("Manual covers beside ISO have highest priority", 'n=add(out,n,max,dir,"/",name);' in covers and
    'n=add(out,n,max,dir,"/",slash+1);' in covers),
 ("New normal JPEG has precedence over old 3D release", 'n=add(out,n,max,normal,"/",name);' in covers and
    covers.index('n=add(out,n,max,normal,"/",name);')<covers.index('n=add(out,n,max,covers,"/",name);')),
 ("Old COVERS JPG retained as fallback", 'n=add(out,n,max,covers,"/",name);' in covers),
 ("Legacy 3D art does not block automatic normal download", 'if(!preferred){' in covers and
    'int preferred=movie_has_normal_art(' in covers),
 ("Download button does not count legacy 3D JPG as normal art", "movie_has_normal_art(io,it->path" in covers),
 ("After normal download a ready BDISO texture is refreshed", 'tab != PLAT_BDISO' in cc),
 ("NTFS writes still scoped to external BDISO/COVERS", 'strstr(path,"/BDISO/COVERS/")' in ntfs),
 ("No ISO files overwritten", 'movie_usb_write(cover,port,jpeg,bytes)' in art and
    'strncmp(iso_path,"/dev_usb00",10)' in art),
]
for i,(name,ok) in enumerate(tests,1):
 print(f"{i:02d} {'PASS' if ok else 'FAIL'} {name}")
passed=sum(x for _,x in tests)
print(f"Normal-cover priority/integration checks: {passed}/{len(tests)}")
raise SystemExit(passed!=len(tests))
