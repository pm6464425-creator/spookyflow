#!/usr/bin/env python3
"""Static integration guards for optional PS3 Cobra backend.
These checks do not verify syscall runtime behavior on PS3 HEN.
"""
from pathlib import Path
root = Path(__file__).resolve().parents[1]
app = (root / "src/core/app.c").read_text()
main = (root / "src/ps3/main.c").read_text()
mount = (root / "src/ps3/sf_cobra_mount.c").read_text()
checks = {
    "PS3-only interception": "a->tab == PLAT_PS3 && a->host.launch_ps3" in app,
    "PS1/PS2 original launch preserved": "host.launch = launch_via_webman" in main,
    "Poweroff original callback preserved": "host.power_off = power_off_via_webman;" in main,
    "NTFS refresh original callback preserved": "host.ntfs_refresh = ntfs_refresh;" in main,
    "webMAN config original callback preserved": "host.read_wm_config = read_wm_config;" in main,
    "PS3 Cobra backend assigned": "host.launch_ps3 = sf_cobra_mount_ps3;" in main,
    "Cobra version detected": "sf_cobra_version(&version)" in mount,
    "ISO mount syscall": "lv2syscall3(8, 0x7024" in mount,
    "Folder path map": 'sf_cobra_map("/dev_bdvd",path)' in mount,
    "Folder PARAM.SFO validation": '"%s/PS3_GAME/PARAM.SFO"' in mount,
    "No boot plugin or flash operations": not any(x in mount for x in ("boot_plugins", "/dev_flash", "/dev_blind", "fan_control")),
}
for name, passed in checks.items():
    print(("PASS" if passed else "FAIL") + " " + name)
if not all(checks.values()):
    raise SystemExit(1)
print("STATIC INTEGRATION CHECKS ONLY; NOT A HARDWARE VALIDATION.")
