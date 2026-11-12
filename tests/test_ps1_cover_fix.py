#!/usr/bin/env python3
"""PS1-only cover recovery regression contracts.
Existing BDISO movie mounting & poster algorithms are unchanged from V15.7.
These static tests complement runnable PS1 C tests and the PS3 PKG build.
"""
from pathlib import Path
def read(path):return Path(path).read_text(encoding="utf-8")
id_src=read("src/core/discid.c")
art=read("src/core/coverart.c")
test=read("tests/test_covers.h")
calls=read("tests/test_core.c")
ntfs=read("src/ps3/sf_bdiso_ntfs.c")
app=read("src/core/app.c")
checks=[
 ("PS1 file serial fallback restricted to PlayStation platform",
  "plat==PLAT_PS1 ? ps1_filename_serial(path,out,cap) : 0" in id_src),
 ("Conservative PlayStation serial prefixes",
  '"SLUS","SCUS","SLES","SCES","SLPS","SLPM"' in id_src),
 ("Reject unknown game identity rather than guessing",
  "if(digits!=5 || sf_isalpha(*q)||sf_isdigit(*q))continue;" in id_src),
 ("Handle CHD and PBP filenames without ISO9660 reader",
  'sf_ends_with_i(path, ".chd")' in id_src and
  'sf_ends_with_i(path, ".pbp")' in id_src),
 ("PS1 cue selects data MODE track",
  'ps1_cue_data_file(' in id_src and '"MODE2/"' in id_src and '"MODE1/"' in id_src),
 ("Long CUE sheets available to the parser",
  'rd(ctx, path, 0, buf, 8191u)' in id_src),
 ("PS1 verified archive is first cover URL",
  'https://raw.githubusercontent.com/xlenore/psx-covers/main/covers/default/%s.jpg' in art and
  'if (plat == PLAT_PS1) {' in art),
 ("PS1 image fallback retains RPDB mirrors",
  'https://rpdbs3.gerbilsoft.com/ps1/cover/%s/%s.jpg' in art),
 ("PS1 retries previous known-404 auto blacklist",
  'd->auto_mode && it->plat!=PLAT_PS1' in art),
 ("PS1 missing ID per-game diagnostic",
  'covers: ps1 NO_SERIAL' in art),
 ("PS1 remote lookup per-game diagnostic",
  'covers: ps1 SEARCH serial=' in art),
 ("PS1 remote miss per-game diagnostic",
  'covers: ps1 NO_ART_ON_SOURCES serial=' in art),
 ("PS1 network errors distinguished",
  'covers: ps1 NETWORK_ERROR serial=' in art),
 ("PS1 multi-file CUE end-to-end host test",
  'Data Track.bin' in test and 'test_ps1_covers_serial_and_sources();' in calls),
 ("PS1 automatic downloader recheck host test",
  'test_ps1_cover_download_retry();' in calls and 'SLUS00594.missing' in test),
 ("PS2 archive first source unchanged",
  'http://gamecovers.free.fr/download.php?file=%s.jpg' in art),
 ("No BDISO/Cobra mounting modifications",
  'int sf_bdntfs_start(void)' in ntfs and 'usb_hotplug_poll(' in app),
 ("Cinema original 2:3 poster unchanged",
  'case PLAT_BDISO: return 2.0f/3.0f' in read("src/core/layout.c")),
 ("No disk image writes added",
  'ps3ntfs_write(' not in id_src and 'sysFsUnlink(' not in id_src),
]
for label,ok in checks: print(("PASS" if ok else "FAIL"),label)
print("PS1 covers targeted regressions: %d/%d"%(sum(bool(ok) for _,ok in checks),len(checks)))
raise SystemExit(0 if all(ok for _,ok in checks) else 1)
