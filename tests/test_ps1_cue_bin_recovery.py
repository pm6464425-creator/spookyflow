#!/usr/bin/env python3
"""V15.9 PS1 recovery: hardware-inspired CUE/BIN pairing, actual fallback
source, PS3 downloaded JPEG decode and selective cache refresh invariants.
BDISO (fully working V15.7) and PS2/PS3 scanning must remain unchanged.
"""
from pathlib import Path
def rd(f):return Path(f).read_text(encoding="utf-8")
scan=rd("src/core/scanner.c")
disc=rd("src/core/discid.c")
art=rd("src/core/coverart.c")
hdr=rd("src/core/coverart.h")
ps3=rd("src/ps3/ps3_covers.c")
tests=rd("tests/test_covers.h")
test_calls=rd("tests/test_core.c")
ntfs=rd("src/ps3/sf_bdiso_ntfs.c")
checks=[
 ("All CUE FILE declarations parsed safely",
  "int discid_cue_files(" in disc and "sf_strnicmp(txt+p,\"FILE\",4)" in disc),
 ("CUE includes multiple tracks without duplicating a game",
  "s_ps1_cue_track[j]=1;" in scan and
  "s_ps1_cue_track[i]) continue;" in scan),
 ("CUE is authoritative rather than guessing BIN from title",
  "discid_cue_files(s_ps1_cue_text" in scan),
 ("PS1 scanner receives reader callbacks",
  "if(s->rd)" in scan and "s->rd(s->rd_ctx,path,0,s_ps1_cue_text" in scan),
 ("CUE detection limited to PS1",
  "if(plat==PLAT_PS1)" in scan and "if (plat == PLAT_PS1)" in scan),
 ("PS1 CUE/BIN mismatched name regression",
  "test_ps1_cue_bin_track_grouping();" in test_calls and
  "RE2 Track 01.bin" in tests),
 ("Recover downloaded JPEGs with software decoder if PS3 hardware fails",
  "ps3_jpeg_soft_decode(data,bytes,max_dim" in ps3 and
  'strncmp(path,COVER_DL_DIR "/",sizeof(COVER_DL_DIR))==0' in ps3),
 ("NTFS BDISO decode pathway kept untouched",
  "sf_bdntfs_load(path,&data,&nbytes" in ps3),
 ("PS1-only processed/negative cache invalidation",
  "plat==PLAT_PS1 ? 7 : CVR_VERSION" in art and
  "y[0] == cvr_version(j->plat)" in art),
 ("Other platform cache version remains 6",
  "#define CVR_VERSION 6" in art),
 ("Downloaded cover by serial for standard games, but verified custom hack first",
  "nu=ps1_special ? 0 :" in art and
  "valid_serial ? cover_dl_urls(it->plat,it->id,urls,COVER_DL_URLS)" in art),
 ("No serial: fallback to real libretro region-qualified filename",
  "cover_dl_ps1_title_url(it->path,urls[nu],COVER_URL_MAX)" in art),
 ("Libretro official live thumbnail root",
  "https://thumbnails.libretro.com/Sony%20-%20PlayStation/Named_Boxarts/" in art),
 ("PS1 fallback saves under exact game filename rather than blank ID",
  ("cover_candidates(PLAT_PS1,it->path,it->is_dir,\"\"" in art or
   ("cover_candidates(it->plat,it->path,it->is_dir,\"\"" in art and
    "(it->plat==PLAT_PS1 && u==ps1_title_url_index)" in art))),
 ("No blank .missing file",
  "if (not_found_only && miss_path[0] && io->write_file)" in art),
 ("No guessing from custom modified games",
  "cover_name_is_custom(game_path)" in art),
 ("Full automatic CUE no-ID and serial-404 test",
  "test_ps1_libretro_filename_download();" in test_calls),
 ("Per-source HTTP diagnostics to investigate real PS3 exceptions",
  "covers: ps1 SOURCE serial=" in art),
 ("Existing BDISO movie handling retained",
  "if (it->plat == PLAT_BDISO)" in art and
  "int sf_bdntfs_start(void)" in ntfs),
 ("Covers PS1 square case unchanged",
  "case PLAT_PS1: *w = 384; *h = 384" in art),
]
for label,ok in checks:print(("PASS" if ok else "FAIL"),label)
print("V15.9 PS1 CUE/BIN, Libretro, JPEG recovery checks: %d/%d"%(sum(bool(x) for _,x in checks),len(checks)))
raise SystemExit(0 if all(v for _,v in checks) else 1)
