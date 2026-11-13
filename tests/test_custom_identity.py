#!/usr/bin/env python3
"""30 integration checks: XMB identity, manual overrides, auto network,
unchanged dimensions and Cobra. These tests do NOT emulate PS3 hardware."""
from pathlib import Path
root=Path(__file__).resolve().parents[1]
src=lambda s:(root/s).read_text()
sfo=src('sfo.xml')
art=src('src/core/coverart.c')
hdr=src('src/core/coverart.h')
app=src('src/core/app.c')
ah=src('src/core/app.h')
main=src('src/ps3/main.c')
mount=src('src/ps3/sf_cobra_mount.c')
icon=(root/'pkgfiles/ICON0.PNG').read_bytes()
checks={
 "01 XMB name": 'MIS JUEGOS PS3 PS2 PS1' in sfo,
 "02 original Title ID": 'SPKF00001' in sfo,
 "03 PNG signature": icon.startswith(b'\x89PNG\r\n\x1a\n'),
 "04 ICON0 dimensions": icon[16:24]==bytes((0,0,1,64,0,0,0,176)),
 "05 ICON0 transparency": icon[25] in (4,6) or b'tRNS' in icon,
 "06 exclusive source folder": 'enum { F_SF_DL = 0, F_COUNT = 1 }' in art,
 "07 managed folder defined": 'k_folders[F_COUNT] = { COVER_DL_DIR }' in art,
 "08 local ISO sidecar excluded": 'n = add(out, n, max, stem, NULL, NULL)' not in art,
 "09 multiMAN paths excluded": 'MM_STD' not in art,
 "10 webMAN paths excluded": 'WMTMP' not in art,
 "11 filename-matched manual art": 'COVER_DL_DIR, "/", name' in art,
 "12 official ID fallback": 'COVER_DL_DIR, "/", id' in art,
 "13 mods avoid stale official ID": '!cover_name_is_custom(game_path)' in art,
 "14 download skips existing": 'cover_has_art(io,it->plat,it->path,it->is_dir,it->size,' in art and 'ps1_special?"":it->id' in art,
 "15 no processed cache considered as source": 'if (io->is_file(io->ctx, f)) return 1;' not in art,
 "16 versioned cache invalidates sidecars": '#define CVR_VERSION 6' in art,
 "17 manual rescan clears old CVR": 'sf_ends_with_i(ent[i].name, ".cvr")' in art,
 "18 manual rescan clears missing markers": 'sf_ends_with_i(ent[i].name, ".none")' in art,
 "19 missing marker preserved": 'io->write_file(io->ctx,miss_path,"404",3' in art,
 "20 no stale 404 after download": 'io->remove(io->ctx,miss_path)' in art,
 "21 manual download preserved": 'download_start(a, 0)' in app,
 "22 auto download preserved": 'download_start(a, 1)' in app,
 "23 avoid overlapping size worker": '!app_sizes_busy(a) && !a->dl' in app,
 "24 auto fetch remains asynchronous": 'auto scan skipped (background worker unavailable)' in app,
 "25 PS1 texture dimensions": '*w = 384; *h = 384;' in art,
 "26 PS2 texture dimensions": '*w = 320; *h = 448;' in art,
 "27 PS3 texture dimensions": '*w = 384; *h = 448;' in art,
 "28 original PS1 PS2 PS3 scaling unchanged": 'cover_process(rgba,w,h,cover_art_aspect(j->plat)' in art and 'cover_process_impl(rgba,w,h,aspect,dst,tw,th,0)' in art,
 "29 Cobra registration unchanged": 'host.launch_ps3 = sf_cobra_mount_ps3;' in main,
 "30 original webMAN fallback": 'host.launch = launch_via_webman;' in main,
}
for k,v in checks.items(): print(('PASS' if v else 'FAIL'),k)
if not all(checks.values()): raise SystemExit('identity/cover regression failure')
print(str(len(checks))+' static integration checks passed')
