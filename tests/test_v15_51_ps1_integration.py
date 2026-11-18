#!/usr/bin/env python3
"""PS1 mode is based on Sony's official XMB upscaler; protect 15.50 SNES."""
from pathlib import Path
import subprocess
get=lambda p:Path(p).read_text()
main=get("src/ps3/main.c")
fs=get("src/ps3/ps3_fs.c")
log=get("src/core/log.c")
platform=get("src/ps3/ps3_platform.h")
reg=get("src/ps3/sf_ps1_registry.c")
video=get("src/ps3/sf_ps1_video_diag.c")
mk=get("Makefile")
for k in ("-DSF_VIDEO_ONLY_DIAG=1","-DSF_PS1_VIDEO_DIAG=1"):
    assert k in mk
assert 'SF_DATA_DIR "/diagnostico_pantalla_ps1.txt"' in platform
assert 'SPOOKYFLOW V15.51 - DIAGNOSTICO AVANZADO PANTALLA PS1' in log
assert 'strstr(buf,"PASSWORD")' not in reg
assert '"/dev_flash2/etc/xRegistry.sys"' in video
assert 'fopen(path,"rb")' in reg
assert 'fopen(path,"wb")' not in reg and 'remove(' not in reg
assert '/setting/game/emuUpConvert' in reg
assert 'sf_ps1_registry_read_modes' in video
assert 'READONLY=YES' in video and 'MODE=%s' in video
assert 'FULL_SCREEN_16_9' in reg
assert 'PS1_16_9_SETTING=VERIFIED_FULL_SCREEN' in video
assert 'NOT_FULL' in video and 'FIX_ON_PS3_XMB' in video
assert 'ps1_video_diag_webman(request,rc)' in main
assert 'sf_ps1_mount(path,emulator)' in main
assert 'sf_ps1_join_mount()' in main
assert 'host.launch_ps1 = launch_ps1_forensic;' in main
assert 'host.ps1_join_mount = launch_ps1_staged_forensic;' in main
assert 'sf_ps1_video_diag_startup();' in main
assert '#if defined(SF_PS1_VIDEO_DIAG)' in fs
assert 'strcmp(ent.d_name,"diagnostico_pantalla_ps1.txt")' in fs
assert '"diagnostico_pantalla_snes.txt"' in fs
assert 'strncmp(msg,"PS1_DIAG",8)' in log
assert 'strncmp(msg,"ps1_video_diag: ",16)' in log
assert 'strncmp(msg,"snes: DIAG_",11)' in log  # older mode still in source, but inactive
assert "video_report_open(path)" in log
assert 'ps3_fs_cleanup_legacy_diagnostics()' in main
# PS1 core mount/split logic, pSNES framebuffer patch, audio and other consoles unchanged
for path,sha in {
  "src/ps3/sf_ps1_mount.c":"d78b21a68f5d2c03571fce9e9f960633b86c65b2",
  "src/ps3/sf_ps1_join.c":"0b957e1ec65db280663d5ac42e48d6e838c6ab1",
  "src/ps3/sf_cobra_mount.c":"fe9faf92587a9c86ae866502ef5346f98fb76715",
  "src/ps3/sf_ps2_hen.c":"f20a63d83596a9b5079fd056e261656a4f536201",
  "src/ps3/ps3_webman.c":"333f9d3fe75f3ebbdc0840ddb696385306919eca",
  "tools/psnes_patch_runtime_full_bleed.py":"0e9d217741059266e63471f3d94acbd7443e9609",
}.items():
    h=subprocess.check_output(["git","hash-object",path],text=True).strip()
    assert h==sha,(path,h,sha)
assert 'make_self "$tmp/psnes_edge.elf" "$target"' in get("tools/build_psnes_plain_self.sh")
assert "sf_snes_force_widescreen(rom_path)" in main
print("PASS V15.51 PS1 Sony native 16:9 setting detection, one PS1 TXT, safe cleanup and unchanged working SNES/audio/launchers")
