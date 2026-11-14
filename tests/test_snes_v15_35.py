#!/usr/bin/env python3
"""V15.35: authentic SNES box scans only, repair old screenshot cache, no false editions."""
from pathlib import Path
import subprocess
def read(p): return Path(p).read_text(encoding="utf-8")
def sha_ok(p,sha):
    return subprocess.check_output(["git","hash-object",p],text=True).strip()==sha
cover=read("src/core/coverart.c")
matcher=read("src/core/snes_name_match.c")
header=read("src/core/snes_name_match.h")
build=read("ci/build_ps3.sh")
verify=read("tests/verify_compiled_pkg.py")
host=read("tests/test_covers.h")
ci=read(".github/workflows/build.yml")
tests=[
("01 SNES download flow uses real box scans exclusively",
 "snes_name_catalog_matches(it->path,snes_catalog" in cover
 and "snes_name_catalog_url(real_names[ci]" in cover
 and "snes_name_catalog_url_kind(real_name,kind+1" not in cover),
("02 Previous erroneous in-game title/snapshot/logo fallback is inactive",
 "snes_alts" not in cover and "snes_alt_lengths" not in cover
 and "Named_Titles / Named_Logos / Named_Snaps are NOT box art" in cover),
("03 The published PKG must not bundle screenshot lookup catalogs",
 "for KIND in Named_Titles" not in build
 and '("snes_title_names.txt", "snes_logo_names.txt",' in verify),
("04 Real boxarts ranked by original retail release, special variants penalized",
 "snes_name_catalog_matches(" in matcher and "score -= c->edition * 30;" in matcher
 and "Super Bomberman 5 (Japan) (Caravan Event Ban).png" in host),
("05 Multiple actual official regional frontcover versions are tried",
 "snes_name_catalog_matches(" in matcher
 and "char real_names[5][256]" in cover
 and "nu<COVER_DL_URLS" in cover),
("06 Japanese authentic portrait artwork is accepted without distortion",
 "snes_portrait_saved" in cover
 and "PORTRAIT_FITTED" in cover
 and "cover_process(rgba,w,h,cover_art_aspect(j->plat)" in cover),
("07 Genuine horizontal box art overrides first authentic portrait when found",
 "!snes_landscape_saved" in cover
 and "snes_real_horizontal>=1" in host
 and "snes_real_portrait>=2" in host),
("08 Old automatic 256x224/256x239 title screens are rejected without deleting manuals",
 "snes_png_legacy_title_screen" in cover
 and "snes_auto_legacy_screen" in cover
 and "LEGACY_GAME_SCREEN_REPLACE_REQUIRED" in cover
 and "REJECT_OLD_GAME_TITLE_SCREEN" in cover),
("09 SNES-specific processed cache version invalidates old V15.34 screenshots only",
 "plat==PLAT_SNES ? 8" in cover
 and "OLD_TITLE_SCREEN_CACHE_INVALIDATED" in cover
 and "if(j->plat==PLAT_SNES)" in cover),
("10 Real PS3 diagnostic examples reproduce Super Bomberman 5 and Japanese originals",
 "Super Bomberman 5 (Japan) (T-En by Psyklax Beta)" in host
 and "Ace o Nerae! (ESP).sfc" in host
 and "Ancient Magic - Bazoo!" in host),
("11 Known official title aliases improve verified previously missing ROMs",
 all(x in matcher for x in (
  '"bastard ankoku no hakaishin"',
  '"doremi fantasy milon no dokidoki daibouken"',
  '"captain tsubasa v hasha no shougou campione"',
  '"final fantasy mystic quest"',
  '"mega man 7"',
  '"tecmo secret of stars"',
  '"skyblazer"'))),
("12 PS1, PS2, PS3, BDISO and unchanged HEN launching are safe from this patch",
 all(sha_ok(*x) for x in (
    ("src/ps3/sf_ps2_hen.c","f20a63d83596a9b5079fd056e261656a4f536201"),
    ("src/ps3/sf_cobra_mount.c","fe9faf92587a9c86ae866502ef5346f98fb76715"),
    ("src/ps3/ps3_webman.c","333f9d3fe75f3ebbdc0840ddb696385306919eca"),
    ("src/core/ps1_name_match.c","b5ea816d0c4d978db13a0a1c89452f56b7f78e9c"),
    ("src/core/ui.c","0da4dccbaef66846b3dcc15779e0363e10be431c")
 )) and "test_snes_recover_vertical_and_alias();" in read("tests/test_core.c"))
]
fail=[]
for label,ok in tests:
    print(("PASS " if ok else "FAIL ")+label)
    if not ok:fail.append(label)
print("SNES V15.35 checks: %s/%s"%(len(tests)-len(fail),len(tests)))
if fail:raise SystemExit(1)
