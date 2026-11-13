#!/usr/bin/env python3
"""V15.26 real-file diagnostic regression: Metal Slug X regional CUE missing BIN."""
from pathlib import Path
r=Path(__file__).resolve().parents[1]
src=lambda f:(r/f).read_text()
mount=src("src/ps3/sf_ps1_mount.c")
join=src("src/ps3/sf_ps1_join.c")
resolver=src("src/ps3/sf_ps1_resolve.c")
match=src("src/core/sf_ps1_file_match.c")
main=src("src/ps3/main.c")
app=src("src/core/app.c")
log=src("src/core/log.c")
platform=src("src/ps3/ps3_platform.h")
checks={
 "01 all single-BIN CUE references resolved": "sf_ps1_resolve_existing(s_plan.image," in mount,
 "02 all multi-BIN track references resolved": "sf_ps1_resolve_existing(p,s_join_resolved," in join,
 "03 no accidental duplicated track mapping": "RESULT=DUPLICATE_RESOLVED_BIN" in join,
 "04 direct BIN/IMG still accepted": 'ends(path,".bin")' in mount and 'ends(path,".img")' in mount,
 "05 exact path used if already readable": "stat(expected,&st)==0" in resolver,
 "06 other errors not mistaken for missing file": "errno!=ENOENT" in resolver,
 "07 matching uses same folder only": "opendir(s_dirname)" in resolver,
 "08 no recursive directory walk": "readdir(dir)" in resolver and "scandir(" not in resolver,
 "09 same-title normalized match": "region_tag" in match and "normalize" in match,
 "10 exact case-insensitive filename first": "strcasecmp(m->expected,candidate)" in match,
 "11 refuse ambiguous matches": "m->same_score!=1" in match and "AMBIGUOUS" in resolver,
 "12 no unrelated filename auto-selection": "if(!score || score<m->top_score)return;" in match,
 "13 CUE not edited or rewritten": "fopen(" not in resolver and "rename(" not in resolver,
 "14 sources still read-only": "sf_ps2_large_open(&in,s_join.plan.part[i].path,0)" in join,
 "15 safety on unmatched multi-bin": "if(source_rc!=0)return source_rc;" in join,
 "16 preserve PS2 HEN": "host.ps2_hen_start = sf_ps2_hen_start;" in main,
 "17 preserve PS3 direct mount": "host.launch_ps3 = sf_cobra_mount_ps3;" in main,
 "18 preserve BDISO": "host.launch_bdiso = launch_bdiso;" in main,
 "19 single diagnostics only": 'SF_DATA_DIR "/diagnostico_ps1.txt"' in platform,
 "20 logs exact attempted and recovered name": "RESULT=RECOVERED" in resolver and
    "EXPECTED=%s FOUND=%s" in resolver,
 "21 errors still reported in same file": 'strstr(msg,"PS1_DIAG")' in log,
 "22 keep existing webMAN fallback": "a->host.launch(cmd)" in app,
}
for k,v in checks.items():print(("PASS" if v else "FAIL"),k)
print(f"PS1 V15.26 safe name resolution: {sum(checks.values())}/{len(checks)}")
if not all(checks.values()):raise SystemExit(1)
