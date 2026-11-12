#!/usr/bin/env python3
"""Source-integrity checks for BDISO 3D poster providers and community links.
These are source-level tests, not proof of internet access on a real PS3.
"""
from pathlib import Path

def read(p):
    return Path(p).read_text("utf-8")

art=read("src/ps3/sf_bdiso_art.c")
scan=read("src/core/scanner.c")
ntfs=read("src/ps3/sf_bdiso_ntfs.c")
covers=read("src/ps3/ps3_covers.c")
host=read("src/ps3/main.c")
ui=read("src/core/ui.c")
doc=read("PORTADAS_3D_FUENTES.md")
tests=[
    ("TMDB Spanish remains a fallback provider", "TMDB-ES" in art and "language=es-MX" in art),
    ("TMDB imagery endpoint preserved", "/3/movie/%d/images" in art),
    ("Fanart.tv provider present", "https://webservice.fanart.tv/v3.2/movies/" in art),
    ("Fanart API credential optional", 'movie_key(dir,"FANART_KEY.TXT"' in art),
    ("Fanart personal key supported", 'movie_key(dir,"FANART_PERSONAL_KEY.TXT"' in art and "client_key=%s" in art),
    ("Fanart prioritizes Spanish movie posters", "movieposter" in art and 'sf_stricmp(lang,"es")' in art),
    ("OMDb metadata/poster fallback present", "https://www.omdbapi.com/" in art),
    ("OMDb optional credential on external disk", 'movie_key(dir,"OMDB_KEY.TXT"' in art),
    ("OMDb canonical image host guarded", 'https://m.media-amazon.com/' in art),
    ("MoviePosterDB stable image endpoint present", "https://posters.movieposterdb.com/m/%s" in art),
    ("MoviePosterDB own credential optional", 'movie_key(dir,"MOVIEPOSTERDB_KEY.TXT"' in art),
    ("No hardcoded API secrets", 'YOUR_API_KEY' not in art and 'mpdb_pk_EXAMPLE' not in art),
    ("MoviePosterDB token format validated", 'strncmp(mpdb,"mpdb_pk_",8)!=0' in art),
    ("3D label determined by ISO filename, not poster", "movie_filename_is_3d(lib_path(a->lib,g))" in ui),
    ("No 3D release lookup from Mubis", '"https://www.mubis.es/titulos/%s-blu-ray-3d"' not in art),
    ("No 3D Blu-ray release JPEG downloads", 'https://images.static-bluray.com/movies/covers/%d_front.jpg' not in art),
    ("Actual Spanish cinema poster parsed from SensaCine", "movie_sensacine_pick_year(" in art),
    ("Standard Spanish poster priority available", '"TMDB-ES"' in art and '"SensaCine-Cartel"' in art),
    ("Useless source TXT creation removed", "COVERS/%s.sources.txt" not in art),
    ("Legacy JPG remains in place; new cinema JPG separate", '"%s/COVERS/POSTERS"' in art and '"%s/%s.jpg",normaldir,stem' in art),
    ("English auto-generated title converted to Spanish without overriding manual titles", "movie_spanish_title(stem,existing?prior:title" in art),
    ("JPEG fallback still validates image signature", "buf[0]==0xff && buf[1]==0xd8 && buf[2]==0xff" in art),
    ("HTTPS for image URLs", 'strncmp(url,"https://",8)' in art),
    ("Image buffer bounded", "#define MOVIE_JPEG_MAX (2u * 1024u * 1024u)" in art),
    ("No forum-page scraping or search-engine scraping", "www.google.com/search?q=site%%3A" not in art),
    ("Network worker callback remains active", "sf_bdiso_ensure_art(path)" in covers),
    ("NTFS media writes remain in BDISO/COVERS", 'strstr(path,"/BDISO/COVERS/")' in ntfs),
    ("BDISO NTFS partition scan preserved", "bd_find_partition(u)" in ntfs),
    ("PS3 and BDISO mount backends remain separate", "host.launch_ps3" in host and "host.launch_bdiso" in host),
    ("PS3-style case maintained", "ui_set_case_image(PLAT_BDISO" in read("src/ps3/ps3_tabimg.c")),
    ("PS1/PS2/PS3 scan roots kept", all(v in scan for v in ('"PSXISO"','"PS2ISO"','"PS3ISO"'))),
    ("Documented cinema posters and paired watermarks", "póster cinematográfico" in doc and "marca de agua" in doc),
    ("Spanish titles still attempted", "movie_title_id(" in art),
]
failed=[name for name,good in tests if not good]
for i,(name,passed) in enumerate(tests,1):
    print(f"{i:02d}. {'PASS' if passed else 'FAIL'} - {name}")
print(f"BDISO 3D multisource checks: {len(tests)-len(failed)}/{len(tests)}")
if failed:raise SystemExit(1)
