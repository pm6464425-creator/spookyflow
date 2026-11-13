#!/usr/bin/env python3
"""V15.13 PS2 local mounting gates. Static analysis, NOT PS3 hardware proof."""
from pathlib import Path
base=Path(__file__).resolve().parents[1]
def get(path): return (base/path).read_text(encoding="utf-8")
app=get("src/core/app.c")
apph=get("src/core/app.h")
main=get("src/ps3/main.c")
mount=get("src/ps3/sf_ps2_mount.c")
plan=get("src/core/ps2_mount_plan.c")
checks={
 "01 host PS2 preflight is declared": "int (*ps2_preflight)(const char *path);" in apph,
 "02 local PS2 callback is declared": "int (*launch_ps2)(const char *path, int emulator);" in apph,
 "03 PS2 Cobra host preflight installed": "host.ps2_preflight = sf_ps2_preflight;" in main,
 "04 PS2 Cobra host mount installed": "host.launch_ps2 = sf_ps2_mount;" in main,
 "05 PS3 original callback remains": "host.launch_ps3 = sf_cobra_mount_ps3;" in main,
 "06 BDISO original callback remains": "host.launch_bdiso = launch_bdiso;" in main,
 "07 PS1 legacy path still available": "host.launch = launch_via_webman;" in main,
 "08 PS2 branch precedes old webMAN request": app.index('if (a->tab == PLAT_PS2 && a->host.launch_ps2)') < app.index('launch_build(&req, &wm'),
 "09 PS2 preflight before CONFIG writes": app.index('result = a->host.ps2_preflight ?') < app.index('ps2cfg_apply(a, path, lib_str'),
 "10 PS2 per-game CONFIG still handled": "ps2cfg_apply(a, path, lib_str(a->lib, d ? d->id : g->id), c.ps2cfg);" in app,
 "11 PS2 mount returns to XMB on success": 'app_request_exit(a, "ps2 cobra mount")' in app,
 "12 no webMAN network in direct mount": "ps3_webman" not in mount and "127.0.0.1" not in mount,
 "13 HEN guard uses official opcode": "lv2syscall1(8,0x1337)" in mount,
 "14 HEN is rejected before virtual disc mutations": mount.index('if (raw_hen()==0x1337) return -41;') < mount.index("cobra_send_fake_disc_eject_event()"),
 "15 correct PS2 native opcode through libCobra": "cobra_mount_ps2_disc_image(parts,n,tracks,1)" in mount,
 "16 PS2 has only data track for ISO": "tracks[0].lba=0;tracks[0].is_audio=0;" in mount,
 "17 no flash or occupied plugin manipulation": all(x not in mount for x in ("/dev_flash", "/dev_blind", "cobra_unload_vsh_plugin","boot_plugins")),
 "18 journal exists before virtual disc changes": 'journal("PREFLIGHT_OK",path,0,1)' in mount and mount.index('journal("PREFLIGHT_OK",path,0,1)') < mount.index('cobra_send_fake_disc_eject_event()'),
 "19 failure path attempts disc reinsert": "if(ejected)(void)cobra_send_fake_disc_insert_event();" in mount,
 "20 no PS2 files get renamed": not any(x in mount for x in ("rename(", "unlink(", "remove(", "fopen(path")),
 "21 internal HDD PS2ISO allowlist": 'strncmp(path,"/dev_hdd0/PS2ISO/",17)' in plan,
 "22 correct split ISO listing bounded to 64": "i<SF_PS2_MAX_PARTS" in plan,
 "23 Cobra kernel version before changes": mount.index('raw_version(&version)') < mount.index('cobra_send_fake_disc_eject_event()'),
 "24 HEN user-facing diagnosis": "PS3 HEN: ISO de PS2 requiere PS2 Classics" in app,
 "25 Cobra types available before header": mount.index('#include <stdint.h>') < mount.index('#include <cobra.h>'),
}
for name,ok in checks.items():print(("PASS " if ok else "FAIL ")+name)
print("PS2 static checks %d/%d"%(sum(map(bool,checks.values())),len(checks)))
if not all(checks.values()):raise SystemExit(1)
