#!/usr/bin/env python3
"""V15.21 HEN autoplay static checks, not hardware confirmation."""
from pathlib import Path
r=Path(__file__).resolve().parents[1]
a=(r/"src/core/app.c").read_text()
h=(r/"src/ps3/sf_ps2_hen.c").read_text()
m=(r/"src/ps3/main.c").read_text()
l=(r/"src/core/log.c").read_text()
old=(r/"src/core/launch.c").read_text()
checks={
 "01 wait XMB + start title": '"/wait.ps3?xmb;/wait.ps3?3;/play.ps3?PS2U10000"' in a,
 "02 correct launcher ID": "PS2U10000" in a,
 "03 shared helper exists": "static void ps2_hen_return_to_xmb" in a,
 "04 exactly two success paths": a.count("ps2_hen_return_to_xmb(a,")==2,
 "05 cached is supported": 'ps2_hen_return_to_xmb(a, "ps2 classics ready (cached)")' in a,
 "06 successful new conversion": 'ps2_hen_return_to_xmb(a,"ps2 classics prepared")' in a,
 "07 success checked before launch": 'if (status==1) {' in a,
 "08 failure path separate": 'ps2_hen_return_to_xmb(a,"ps2 classics prepared");\n        } else {' in a,
 "09 webMAN optional": "a->host.launch ? a->host.launch(request) : -1" in a,
 "10 request status checked": "if (rc == 0)" in a,
 "11 fallback manual": "AUTOPLAY=MANUAL_FALLBACK" in a,
 "12 queued diagnostics": "AUTOPLAY=QUEUED" in a,
 "13 normal app exit": "app_request_exit(a, reason)" in a,
 "14 source image preserved": "source ISO is never modified" in h,
 "15 crypto has no launch commands": "/play.ps3" not in h,
 "16 COBRA CFW preserved": "a->host.launch_ps2(path, c.emulator)" in a,
 "17 native PS3 preserved": "a->host.launch_ps3" in a,
 "18 original PS1 preserved": "/mount_ps3%s?random=0" in old,
 "19 BDISO preserved": "a->host.launch_bdiso" in a,
 "20 one log version": "DIAGNOSTICO PS1 (V15.24)" in l,
 "21 main log version": "SPOOKYFLOW V15.24 - PS1 diagnostic started" in m,
 "22 CD conversion untouched": "cooked_cd_fill(&in,&cdstream,blk,readn)" in h,
 "23 CONFIG stage untouched": "ps2_stage_config(s_job.gameid)" in h,
 "24 skip native PS2 mount on HEN": "if (raw_hen()==0x1337) return -41;" in (r/"src/ps3/sf_ps2_mount.c").read_text(),
}
for n,v in checks.items():
 print("PASS" if v else "FAIL",n)
if not all(checks.values()):raise SystemExit("Failed V15.21 checks")
print("PS2 HEN V15.21 autoplay: 24/24 checks passed")
