#!/usr/bin/env python3
"""SNES V15.34: twelve isolated cross-version regressions.
The C host test simulates vertical 512x878 from a real PS3 report and confirms
only the horizontal Libretro Named_Titles replacement is stored.
"""
from pathlib import Path
import subprocess
def read(p): return Path(p).read_text(encoding="utf-8")
def sha_ok(p,sha):
    return subprocess.check_output(["git","hash-object",p],text=True).strip()==sha
src=read("src/core/coverart.c")
names=read("src/core/snes_name_match.c")
hdr=read("src/core/snes_name_match.h")
builder=read("tools/build_snes_boxart_catalog.py")
build=read("ci/build_ps3.sh")
tests=read("tests/test_covers.h")
pkg=read("tests/verify_compiled_pkg.py")
app=read("src/core/app.c")
checks=[
("01. Four actual Libretro catalogs, not an unverified image search engine",
 all(x in build for x in ("Named_Titles","Named_Logos","Named_Snaps",
                           "snes_title_names.txt","snes_logo_names.txt","snes_snap_names.txt"))
 and "KINDS = (\"Named_Boxarts\", \"Named_Titles\", \"Named_Logos\", \"Named_Snaps\")" in builder),
("02. All four verified sources are bundled in a real installable PKG",
 all(x in pkg for x in ("snes_boxart_names.txt","snes_title_names.txt",
                         "snes_logo_names.txt","snes_snap_names.txt"))),
("03. Every candidate has a real source-specific filename from that provider",
 "snes_name_catalog_match(it->path,snes_alts[kind]" in src and
 "snes_name_catalog_url_kind(real_name,kind+1" in src and
 "Named_Titles/" in names and "Named_Logos/" in names and "Named_Snaps/" in names),
("04. Do not re-download or touch already accepted horizontal covers",
 "cover_has_art(io,it->plat,it->path" in src and
 "if(plat==PLAT_SNES && snes_auto_png_portrait(io,f))" in src),
("05. PNG orientation is validated before the network image is written",
 "int orient=png ? snes_png_orientation_bytes(buf,len) : 0;" in src and
 "snes: REJECT_VERTICAL_OR_INVALID" in src),
("06. Old automatic vertical PNGs are not mistaken for valid completed art",
 "snes: VERTICAL_REPLACE_REQUIRED" in src and
 "snes: REJECT_EXISTING_VERTICAL" in src),
("07. Old cached 384x268 transformed portrait is invalidated only for bad SNES original",
 "snes: STALE_VERTICAL_CACHE_INVALIDATED" in src and
 "if(j->plat==PLAT_SNES)" in src and "read_cache(L,f,j->plat,tw,th,year)" in src),
("08. A successfully replaced SNES original also discards only that game's old .cvr",
 "snes: REPLACED_STALE_CVR rom=%s" in src and
 "cover_cache_stem(io->cache_dir,it->plat,it->path" in src and
 'snprintf(cached,sizeof(cached),"%s.cvr",stem)' in src),
("09. Exact manual SNES/COVERS art remains higher priority and cannot be overwritten",
 'if (plat == PLAT_SNES && slash)' in src and
 '/SNES/COVERS/' in tests and
 'if(it->plat==PLAT_SNES) {' in src and
 "COVER_DL_DIR" in src),
("10. Known official spelling alias from real 404 log is corrected",
 '"ancient magic bazoo mahou sekai"' in names and
 '"ancient magic bazoe mahou sekai"' in names and
 'test_snes_recover_vertical_and_alias();' in read("tests/test_core.c")),
("11. Controlled host test reproduces vertical PNG and checks fallback and good-cover reuse",
 "snes_fixture_png(b,512,878);" in tests and
 "snes_fixture_png(b,512,224);" in tests and
 "second.n_have==2 && second.n_got==0" in tests),
("12. PS1/PS2/PS3/BDISO mounts and entire game UI stay byte-for-byte unchanged",
 all(sha_ok(*x) for x in (
    ("src/ps3/sf_ps2_hen.c","f20a63d83596a9b5079fd056e261656a4f536201"),
    ("src/ps3/sf_cobra_mount.c","fe9faf92587a9c86ae866502ef5346f98fb76715"),
    ("src/ps3/ps3_webman.c","333f9d3fe75f3ebbdc0840ddb696385306919eca"),
    ("src/core/ps1_name_match.c","b5ea816d0c4d978db13a0a1c89452f56b7f78e9c"),
    ("src/core/ui.c","0da4dccbaef66846b3dcc15779e0363e10be431c")
 )) and "host.launch_snes = launch_snes_psnes;" in read("src/ps3/main.c"))
]
fail=[]
for label,ok in checks:
    print(("PASS " if ok else "FAIL ")+label)
    if not ok:fail.append(label)
print(f"SNES V15.34 specific regressions: {len(checks)-len(fail)}/12")
if fail:raise SystemExit(1)
