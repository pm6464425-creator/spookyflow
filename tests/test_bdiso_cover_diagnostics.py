#!/usr/bin/env python3
"""Regression checks for diagnosable BDISO network poster downloads.
These tests do not assert that an individual PS3 has Internet connectivity.
"""
from pathlib import Path

def rd(p):
    return Path(p).read_text(encoding="utf-8")

art=rd("src/ps3/sf_bdiso_art.c")
art_h=rd("src/ps3/sf_bdiso_art.h")
http=rd("src/ps3/ps3_webman.c")
main=rd("src/ps3/main.c")
scan=rd("src/core/scanner.c")
ntfs=rd("src/ps3/sf_bdiso_ntfs.c")
tests=[
    ("TMDB v3 key validation preserved when configured", 'k!=32' in art and 'isxdigit' in art),
    ("OMDb metadata credential still supported", '"OMDB_KEY.TXT"' in art),
    ("Missing keys trigger public lookup", 'trying keyless public movie catalog' in art),
    ("Retry guard runs after optional API-key checks", art.index("if(attempted(iso_path))")>art.index("if(!tmdb[0] && !omdb[0])")),
    ("Reset retry function available", 'void sf_bdiso_reset_attempts(void)' in art and 'void sf_bdiso_reset_attempts(void);' in art_h),
    ("Reset clears attempted path hashes", 'memset(s_attempted,0,sizeof(s_attempted))' in art),
    ("Rescan Library triggers reset", "sf_bdiso_reset_attempts();" in main),
    ("HTTP status logged", "HTTP/transport=%d" in art),
    ("HTTP logger does not include full API key query", "host=%.*s" in art and "width<80" in art),
    ("Generic HTTPS requests accept JSON and images", r"Accept: */*\r\n" in http),
    ("No direct HDD movie cover writes", "movie_usb_write" in art),
    ("Previously downloaded edition images preserved; fresh cinema JPG separate", '"%s/COVERS/POSTERS"' in art and "movie_usb_write(coverpng,port,jpeg,bytes)" in art),
    ("No candidates produce trace", "no poster candidates" in art),
    ("Rejected JPEG produces trace", "no usable image from" in art),
    ("USB output write failure produces trace", "USB write failed" in art),
    ("Missing folder produces trace", "failed to create/read BDISO/COVERS" in art),
    ("Failed poster leaves final trace", "cover fetch failed for %s" in art),
    ("PS1/PS2/PS3 scanning unchanged", all(x in scan for x in ('"PSXISO"','"PS2ISO"','"PS3ISO"'))),
    ("BDISO NTFS reader remains", "bd_find_partition(u)" in ntfs),
    ("Blu-ray platform mount remains", "host.launch_bdiso" in main),
]
for i,(name,ok) in enumerate(tests,1):
    print(f"{i:02d}. {'PASS' if ok else 'FAIL'} - {name}")
good=sum(int(x) for _,x in tests)
print(f"Poster diagnostics/regression checks: {good}/{len(tests)}")
raise SystemExit(0 if good==len(tests) else 1)
