#!/usr/bin/env python3
"""Source-level invariants: user-supplied movie artwork and authentic 3D
priorities must survive future styling and provider changes."""
from pathlib import Path
art=Path("src/ps3/sf_bdiso_art.c").read_text()
cover=Path("src/core/coverart.c").read_text()
ui=Path("src/core/ui.c").read_text()
edition=Path("src/core/movie_edition.c").read_text()
def check(label,test):
 print(("PASS " if test else "FAIL ")+label)
 if not test: raise SystemExit(1)
cut=art.index("/* PRIORITY 1: Spanish")
es3d=art.index('"3D-ES-Mubis"',cut)
es_verified=art.index('"3D-ES-Edition"',cut)
en3d=art.index('"3D-EN-Edition"',cut)
es2d=art.index('"2D-ES-Mubis"',cut)
en2d=art.index('"TMDB-default"',cut)
check("3D ES before 3D EN before normal Spanish before English",
      es3d<es_verified<en3d<es2d<en2d)
check("No overlays or 3D banner",'"BLU-RAY 3D"' not in ui and
      "visible,ALIGN_CENTER,win.w*0.91f" not in ui)
check("Only original Blu-ray case watermark kept",
      '"Blu-ray", ALIGN_CENTER' in ui)
check("No extra source text files", 'COVERS/%s.sources.txt' not in art)
check("Physical disc basename preserved in automatic JPG path",
      '"/COVERS/%s.jpg",dir,stem' in art)
check("Older FlowMovie same-name root image accepted",
      'n=add(out,n,max,dir,"/",name)' in cover)
check("BDISO/COVERS same-name image accepted",
      'n=add(out,n,max,covers,"/",name)' in cover)
check("ISO-name.jpg cover accepted",
      'n=add(out,n,max,dir,"/",slash+1)' in cover and
      'n=add(out,n,max,covers,"/",slash+1)' in cover)
check("Native NTFS reader still present",
      "sf_bdntfs_is_file(path)" in Path("src/ps3/ps3_covers.c").read_text())
check("Manual downloaded images not overwritten",
      "if(!repair && movie_http_jpeg" in art)
check("External image URLs restricted to release hosts",
      'https://www.mubis.es/media/releases/' in edition and
      "images.static-bluray.com" in art)
print("Movie-cover priority/integration checks: 11/11")
