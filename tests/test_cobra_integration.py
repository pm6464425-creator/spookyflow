#!/usr/bin/env python3
"""Static integration guards for optional PS3 Cobra backend.
These checks do not verify syscall runtime behavior on PS3 HEN.
"""
from pathlib import Path
root = Path(__file__).resolve().parents[1]
app = (root / "src/core/app.c").read_text()
main = (root / "src/ps3/main.c").read_text()
mount = (root / "src/ps3/sf_cobra_mount.c").read_text()
apph = (root / "src/core/app.h").read_text()
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
    "Early startup before Tiny3D": main.index('sf_cobra_diag_startup("BEFORE_TINY3D_INIT")') < main.index("tiny3d_Init(VERTEX_BUFFER_BYTES)"),
    "Unique build probe marker": '"EARLY_PROBE_20261109_R1"' in mount,
    "Early boot probe and fallback": "cobra_startup_probe.txt" in mount and "cobra_startup_probe_fallback.txt" in mount,
    "Selection probe and fallback": "cobra_selection_probe.txt" in mount and "cobra_selection_probe_fallback.txt" in mount,
    "Selection probe before settings": app.index("a->host.diag_ps3_select(path") < app.index("settings_game_get(&a->settings, lib_key(a->lib, g))"),
    "Selection failure prevents mounting": "Diagnostic file could not be written - launch canceled" in app,
    "Optional selection callback declared": "int (*diag_ps3_select)(const char *path, int is_folder);" in apph,
    "Host selection callback installed": "host.diag_ps3_select = sf_cobra_diag_selection;" in main,
    "Diagnostic snapshot file": 'cobra_last_mount.txt' in mount,
    "Bounded diagnostic history": 'cobra_trace.txt' in mount and '96L*1024L' in mount,
    "Previous interruption preserved": 'cobra_previous_interruption.txt' in mount and 'sf_cobra_diag_report_previous();' in main,
    "Before/after version syscall": 'sf_diag_before("VERSION_7000")' in mount and 'sf_diag_after("VERSION_7000",rc)' in mount,
    "Before/after virtual disc mount": 'sf_diag_before("MOUNT_PS3ISO_7024")' in mount and 'sf_diag_after("MOUNT_PS3ISO_7024",rc)' in mount,
    "Folder EBOOT preflight": '"%s/PS3_GAME/USRDIR/EBOOT.BIN"' in mount,
    "No boot plugin or flash operations": not any(x in mount for x in ("boot_plugins", "/dev_flash", "/dev_blind", "fan_control")),
}
for name, passed in checks.items():
    print(("PASS" if passed else "FAIL") + " " + name)
if not all(checks.values()):
    raise SystemExit(1)
print("STATIC INTEGRATION CHECKS ONLY; NOT A HARDWARE VALIDATION.")
