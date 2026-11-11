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
 "02 Title ID unchanged": '<value name="TITLE_ID"' in sfo and 'SPKF00001' in sfo,
 "03 PNG signature": icon.startswith(b'\x89PNG\r\n\x1a\n'),
 "04 ICON0 dimensions": icon[16:24]==bytes((0,0,1,64,0,0,0,176)),
 "05 ICON0 alpha": icon[25] in (4,6) or b'tRNS' in icon,
 "06 local ISO override first": 'n = add(out, n, max, stem, NULL, NULL)' in art,
 "07 local folder cover": 'n = add(out, n, max, game_path, "/cover", NULL)' in art,
 "08 old multiMAN cover dirs": 'MM_STD "/covers"' in art,
 "09 official ID matching": 'F_COV_STD; i <= F_WMTMP_COV' in art,
 "10 custom name guard": '!cover_name_is_custom(game_path)' in art,
 "11 custom download protection": 'if (cover_name_is_custom(it->path))' in art,
 "12 manual rescan clears CVR": 'sf_ends_with_i(ent[i].name, ".cvr")' in art,
 "13 manual rescan clears none": 'sf_ends_with_i(ent[i].name, ".none")' in art,
 "14 404 missing marker": 'io->ctx, miss_path, "404", 3' in art,
 "15 no stale 404 after success": 'io->remove(io->ctx, miss_path)' in art,
 "16 no retry automatic cached 404": 'if (d->auto_mode && io->is_file' in art,
 "17 manual download remains": 'download_start(a, 0)' in app,
 "18 automatic download exists": 'download_start(a, 1)' in app,
 "19 trigger after library done": 'a->mode == MODE_FLOW' in app and 'auto_covers_started' in app,
 "20 avoid overlapping size worker": '!app_sizes_busy(a) && !a->dl' in app,
 "21 no synchronous auto fallback": 'auto scan skipped (background worker unavailable)' in app,
 "22 auto download is quiet": '!was_auto &&' in app,
 "23 has_art skips existing": 'cover_has_art(io, it->plat' in art,
 "24 PS1 target unchanged": '*w = 384; *h = 384;' in art,
 "25 PS2 target unchanged": '*w = 320; *h = 448;' in art,
 "26 PS3 target unchanged": '*w = 384; *h = 448;' in art,
 "27 original aspect pipeline": 'cover_process(rgba, w, h, cover_art_aspect' in art,
 "28 automatic field defined": 'int auto_mode;' in hdr and 'auto_covers_started;' in ah,
 "29 original Cobra preserved": 'host.launch_ps3 = sf_cobra_mount_ps3;' in main and 'sf_cobra_mount_ps3(' in mount,
 "30 original webMAN fallback": 'host.launch = launch_via_webman;' in main,
}
for k,v in checks.items(): print(('PASS' if v else 'FAIL'),k)
if not all(checks.values()): raise SystemExit('identity/cover regression failure')
print(str(len(checks))+' static integration checks passed')
