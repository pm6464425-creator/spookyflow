#!/usr/bin/env python3
"""V15.32 SNES cover diagnosis: ten independent protections."""
from pathlib import Path
import subprocess
def src(name):return Path(name).read_text(encoding="utf-8")
def hash_is(path,sha):return subprocess.check_output(["git","hash-object",path],text=True).strip()==sha
cover=src("src/core/coverart.c")
log=src("src/core/log.c")
main=src("src/ps3/main.c")
plat=src("src/ps3/ps3_platform.h")
fs=src("src/ps3/ps3_fs.c")
app=src("src/core/app.c")
scanner=src("src/core/scanner.c")
build=src("ci/build_ps3.sh")
match=src("src/core/snes_name_match.c")
pkgverify=src("tests/verify_compiled_pkg.py")
cases=[
("01. SNES official filename catalog fetched at build time",
 "build_snes_boxart_catalog.py" in build and
 "test -s pkgfiles/USRDIR/snes_boxart_names.txt" in build),
("02. SNES names verified inside the real PKG",
 'files.get("USRDIR/snes_boxart_names.txt"' in pkgverify and
 'len(snes_names)>=2500' in pkgverify),
("03. Indexed matching handles numbered ROM packs without guessing game IDs",
 'snes_name_catalog_match(it->path' in cover and
 'snes_name_catalog_url(real_name' in cover and
 'dig((unsigned char)s[0])' in match),
("04. Exact Libretro fallback if index missing",
 "cover_dl_snes_title_url(it->path" in cover and
 "snes: CATALOG_MISSING" in cover),
("05. SNES automatic files saved under managed directory, manual art stays separate",
 "if(it->plat==PLAT_SNES) {" in cover and
 'c[k].base[plen]==\'/\'' in cover and
 'snprintf(f,sizeof(f),"%s.%s",c[k].base' in cover and
 '"snes: SAVE_TARGET %s"' in cover and
 '"snes: STORAGE_ERROR rom=%s' in cover),
("06. Old PS1 forensic TXT removed on boot",
 '"diagnostico_ps1.txt"' in fs and
 'SF_DATA_DIR "/diagnostico_snes.txt"' in plat and
 'SF_DATA_DIR "/diagnostico_ps1.txt"' not in plat),
("07. A single SNES TXT is overwritten, capped, and excludes PS1/PS2/BDISO",
 'fopen(path,"wb")' in log and
 'SF_DIAG_MAX_BYTES (1024u*1024u)' in log and
 'strstr(msg,"snes:")' in log and
 'strstr(msg,"PS1_DIAG")' not in log),
("08. SNES log includes scan, HTTP, downloaded files, decode, cache, and emulator",
 all(x in scanner+cover+main for x in
 ("snes: ROM_FOUND","snes: HTTP status=","snes: SAVED_COVER",
  "snes: COVER_DECODE_ERROR","snes: COVER_GPU","snes: handoff pSNES"))),
("09. Original game paths, BDISO and PS2 HEN implementation unchanged",
 all(hash_is(*e) for e in (
  ("src/ps3/sf_ps2_hen.c","f20a63d83596a9b5079fd056e261656a4f536201"),
  ("src/ps3/sf_cobra_mount.c","fe9faf92587a9c86ae866502ef5346f98fb76715"),
  ("src/ps3/ps3_webman.c","333f9d3fe75f3ebbdc0840ddb696385306919eca"),
  ("src/core/ps1_name_match.c","b5ea816d0c4d978db13a0a1c89452f56b7f78e9c"),
  ("src/core/ui.c","0da4dccbaef66846b3dcc15779e0363e10be431c")
 ))),
("10. PS1 mounting routes retained; no outdated TXT message in UI",
 all(s in app for s in ('a->host.launch_ps1(path,c.emulator)',
                       'a->host.ps1_join_mount()',
                       'app_request_exit(a,"ps1 cobra mount")',
                       'app_request_exit(a,"ps1 multi-bin Cobra cache mount")')) and
 'diagnostico_ps1.txt' not in app),
]
failed=[]
for label,ok in cases:
 print(("PASS " if ok else "FAIL ")+label)
 if not ok:failed.append(label)
print("SNES V15.32 checks: %d/10"%(10-len(failed)))
if failed:raise SystemExit(1)
