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
    ("TMDB Spanish remains the primary provider", "TMDB-ES" in art and "language=es-MX" in art),
    ("TMDB imagery endpoint preserved", "/3/movie/%d/images" in art),
    ("Fanart.tv provider present", "https://webservice.fanart.tv/v3.2/movies/" in art),
    ("Fanart API credential optional", 'movie_key(dir,"FANART_KEY.TXT"' in art),
    ("Fanart prioritizes Spanish movie posters", "movieposter" in art and 'sf_stricmp(lang,"es")' in art),
    ("OMDb metadata/poster fallback present", "https://www.omdbapi.com/" in art),
    ("OMDb optional credential on external disk", 'movie_key(dir,"OMDB_KEY.TXT"' in art),
    ("OMDb canonical image host guarded", 'https://m.media-amazon.com/' in art),
    ("MoviePosterDB stable image endpoint present", "https://posters.movieposterdb.com/m/%s" in art),
    ("MoviePosterDB own credential optional", 'movie_key(dir,"MOVIEPOSTERDB_KEY.TXT"' in art),
    ("No hardcoded API secrets", 'YOUR_API_KEY' not in art and 'mpdb_pk_EXAMPLE' not in art),
    ("MoviePosterDB token format validated", 'strncmp(mpdb,"mpdb_pk_",8)!=0' in art),
    ("3D title recognition remains", "is3d=movie_3d(stem);" in art),
    ("Blu-ray catalog community query provided", "site%%3Ablu-ray.com" in art),
    ("Blu-ray forums query provided", "site%%3Aforum.blu-ray.com" in art),
    ("CoverCity community query provided", "site%%3Acovercity.net" in art),
    ("Swarzycustom community query provided", "site%%3Aswarzycustom.com" in art),
    ("Edition-research references saved to external COVERS", 'COVERS/%s.sources.txt' in art),
    ("User-controlled manual cover takes precedence", "if(ps3_fs_is_file(NULL,cover))return 1;" in art),
    ("Existing translated .title never overwritten", "!ps3_fs_is_file(NULL,titlepath)" in art),
    ("Only JPEG bytes are persisted", "buf[0]==0xff && buf[1]==0xd8 && buf[2]==0xff" in art),
    ("HTTPS for image URLs", 'strncmp(url,"https://",8)' in art),
    ("Image buffer bounded", "#define MOVIE_JPEG_MAX (2u * 1024u * 1024u)" in art),
    ("Forum sources never automatically scraped", "www.google.com/search?q=site%%3A" in art),
    ("Network worker callback remains active", "sf_bdiso_ensure_art(path)" in covers),
    ("NTFS media writes remain in BDISO/COVERS", 'strstr(path,"/BDISO/COVERS/")' in ntfs),
    ("BDISO NTFS partition scan preserved", "bd_find_partition(u)" in ntfs),
    ("PS3 and BDISO mount backends remain separate", "host.launch_ps3" in host and "host.launch_bdiso" in host),
    ("PS3-style case maintained", "ui_set_case_image(PLAT_BDISO" in read("src/ps3/ps3_tabimg.c")),
    ("PS1/PS2/PS3 scan roots kept", all(v in scan for v in ('"PSXISO"','"PS2ISO"','"PS3ISO"'))),
    ("Documented community usage restrictions", "scraping de foros" in doc),
    ("Spanish titles still attempted", "movie_title_id(" in art),
]
failed=[name for name,good in tests if not good]
for i,(name,passed) in enumerate(tests,1):
    print(f"{i:02d}. {'PASS' if passed else 'FAIL'} - {name}")
print(f"BDISO 3D multisource checks: {len(tests)-len(failed)}/{len(tests)}")
if failed:raise SystemExit(1)
