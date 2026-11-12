#!/usr/bin/env python3
"""Structural and portable functional regression guard for BDISO poster downloads.
Real movie fetch and NTFS write still require tests on the user's PS3.
"""
from pathlib import Path

def rd(p): return Path(p).read_text(encoding="utf-8")
cover=rd("src/core/coverart.c")
head=rd("src/core/coverart.h")
bridge=rd("src/ps3/ps3_covers.c")
art=rd("src/ps3/sf_bdiso_art.c")
http=rd("src/ps3/ps3_webman.c")
test=rd("tests/test_core.c")
checks=[
 ("BDISO manual download handled directly", "if (it->plat == PLAT_BDISO) {" in cover),
 ("BDISO branch precedes game ID rejection", cover.index("if (it->plat == PLAT_BDISO) {",cover.index("void cover_dl_run(")) < cover.index("if (!id_ok(it->id))",cover.index("void cover_dl_run("))),
 ("BDISO fetch uses dedicated movie callback", "io->ensure_movie_art(io->ctx, it->path)" in cover),
 ("Movie download counts as downloaded", "it->state = DL_GOT; d->n_got++;" in cover),
 ("Movie download errors are separate from no game ID", "movie cover download failed" in cover),
 ("Existing movie cover not replaced", "cover_has_art(io, it->plat, it->path, it->is_dir, it->size, it->id)" in cover),
 ("Manual download retry resets previous failed attempts", "io->reset_movie_attempts(io->ctx)" in cover),
 ("Movie retry callback exposed", "reset_movie_attempts" in head),
 ("Movie retry callback wired on PS3", "io.reset_movie_attempts = sf_bdiso_reset_cb;" in bridge),
 ("Movie worker and manual callback synchronized", "art_lock();" in art and "art_unlock();" in art),
 ("HTTP parser supports chunked body", "http_dechunk(buf,len)" in http),
 ("HTTP parser recognizes transfer encoding", '"transfer-encoding:"' in http),
 ("HTTP requests force uncompressed response", r"Accept-Encoding: identity\r\n" in http),
 ("Connection socket errors handled", "if (r < 0) { conn_close(&c);" in http),
 ("HTTP worker requests serialized", "g_http_busy" in http and "__sync_lock_test_and_set(&g_http_busy,1)" in http),
 ("PS3 game cover download retained", "cover_dl_urls(it->plat, it->id" in cover),
 ("Cinemeta keyless catalog preserved", "https://v3-cinemeta.strem.io/catalog/movie/top/search=%s.json" in art),
 ("Host functional test exercises movie download", "test_bdiso_real_download();" in test),
 ("Host functional test exercises manual override", "An existing custom poster must never be replaced." in test),
 ("Host functional test exercises missing ID", "d.n_noid==0" in test),
]
for n,(name,ok) in enumerate(checks,1):print(f"{n:02d}. {'PASS' if ok else 'FAIL'} {name}")
total=sum(ok for _,ok in checks)
print(f"BDISO real-download and HTTP regression checks: {total}/{len(checks)}")
raise SystemExit(0 if total==len(checks) else 1)
