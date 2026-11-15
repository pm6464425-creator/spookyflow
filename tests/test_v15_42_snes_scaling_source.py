#!/usr/bin/env python3
"""V15.42: protect native PS3 launch, ROMs and unrelated systems from scaling fix."""
from pathlib import Path
import subprocess

def rd(path): return Path(path).read_text()
main=rd("src/ps3/main.c")
screen=rd("src/ps3/sf_snes_screen.c")
screen_h=rd("src/ps3/sf_snes_screen.h")
backend=rd("src/ps3/sf_snes_bundle.c")
app=rd("src/core/app.c")
build=rd("ci/build_ps3.sh")
start=main.index("static int launch_snes_psnes(")
end=main.index("\nint main(",start)
snes=main[start:end]
checks=[
 ("01 only native SNES invokes full-screen configuration",
  "sf_snes_force_widescreen(rom_path)" in snes and
  main.count("sf_snes_force_widescreen(")==1 and
  "sf_snes_screen.h" in main),
 ("02 16:9 depends on original pSNES scaling FULL, not unsafe ROM patches",
  'SCALING = \\"FULL\\"' in screen and
  'SNES_CFG_HOME "/config.cfg"' in screen and
  'SNES_CFG_HOME "/configs/%.*s.cfg"' in screen),
 ("03 config update is safe before same known-working APP SELF spawn",
  snes.index("sf_snes_force_widescreen") <
  snes.index("sf_psnes_plain_ready") <
  snes.index("sf_snes_diag_before_spawn") <
  snes.index("sysProcessExitSpawn2")),
 ("04 failure cannot disable the now working SNES launch",
  "DIAG_VIDEO_SCALING=CONFIG_UPDATE_FAILED" in snes and
  'return rc;' not in snes.split("sf_snes_force_widescreen(rom_path);",1)[1].split("/* New type-4",1)[0]),
 ("05 no dependency on webMAN, RetroArch, ROM replacement or renderer patch",
  "ps3_webman_send(" not in snes and
  "RetroArch" not in snes and
  "fopen(rom_path" not in snes and
  "sysProcessExitSpawn2(exe, args" in snes),
 ("06 native pSNES binary and converter source unchanged",
  "tools/build_psnes_plain_self.sh" in build and
  'sf_psnes_plain_ready()' in snes and
  "k_plain_sha256" in backend),
 ("07 no attempt to misrepresent SNES source as 1080p native",
  "1080p if" in snes and
  "tiny3d_Init" not in screen and
  "videoConfigure" not in screen),
 ("08 only PSNE00001 config data is modified",
  screen.count('"/dev_hdd0/game/PSNE00001/USRDIR"')==1 and
  "PS1" not in screen and "PS2" not in screen and
  "BDISO" not in screen and "remove(rom_path" not in screen),
 ("09 config fail-open preserves source and intended defaults",
  "sf_snes_config_set_full" in screen_h and
  "write_atomic" in screen and
  "EMULATION" in screen and
  "SCALING" in screen),
 ("10 unchanged PS1/PS2/PS3/BDISO mount modules",
  all(subprocess.check_output(["git","hash-object",p],text=True).strip()==sha for p,sha in {
    "src/ps3/sf_cobra_mount.c":"fe9faf92587a9c86ae866502ef5346f98fb76715",
    "src/ps3/sf_ps2_hen.c":"f20a63d83596a9b5079fd056e261656a4f536201",
    "src/ps3/ps3_webman.c":"333f9d3fe75f3ebbdc0840ddb696385306919eca"}.items())),
 ("11 SpookyFlow retains native PSNES button mapping from working V15.41",
  "host.launch_snes = launch_snes_psnes;" in main and
  "sf_psnes_prepare();" in snes and
  "PSNES_ENGINE.SELF" in snes),
 ("12 pSNES errors remain isolated to SNES tab",
  "if (a->tab == PLAT_SNES)" in app and
  "PS3 no pudo ejecutar pSNES" in app)
]
fail=[]
for name,passed in checks:
    print(("PASS " if passed else "FAIL ")+name)
    if not passed: fail.append(name)
print(f"V15.42 native 16:9 fullscreen integration: {len(checks)-len(fail)}/{len(checks)}")
if fail: raise SystemExit(1)
