#!/usr/bin/env python3
"""V15.48 forensic report source integrity and transition checks.
These host checks cannot observe pSNES GPU frames; hardware test required.
"""
from pathlib import Path
import subprocess
get=lambda x:Path(x).read_text(encoding="utf-8")
main=get("src/ps3/main.c")
log=get("src/core/log.c")
video=get("src/ps3/sf_video_diag.c")
fs=get("src/ps3/ps3_fs.c")
conf=get("src/ps3/sf_snes_screen.c")
hdr=get("src/ps3/ps3_platform.h")
wf=get("Makefile")
diag=get("src/ps3/sf_snes_diag.c")
assert "-DSF_VIDEO_ONLY_DIAG=1" in wf
assert "/diagnostico_pantalla_snes.txt" in hdr
assert "SINGLE_REPORT=" in main and "sf_snes_diag_on_startup();" in main
assert "video_report_open(path)" in log
assert "SF_VIDEO_REUSE_LIMIT" in log
assert "SF_VIDEO_MAX" in log
assert 'strncmp(msg,"video_diag: ",12)' in log
assert 'strncmp(msg,"snes: DIAG_",11)' in log
assert "sf_video_diag_before_snes(rom_path, rc)" in main
assert "sf_video_diag_verified_self()" in main
assert main.index("sf_snes_force_widescreen(rom_path)") < main.index("sf_video_diag_before_snes(rom_path, rc)") < main.index("sf_psnes_plain_ready()")
assert "sf_video_diag_startup()" in main
assert "videoGetState(0,0,&state)" in video
assert "videoGetResolution(state.displayMode.resolution,&actual)" in video
assert "SF_PSNES_NATIVE_BORDER_FILL_ELF_SHA256" in video
assert "SF_PSNES_APP_SELF_SHA256" in video
assert "sf_snes_config_video_probe" in video
assert "PSNES_BOOT_FORENSIC" in diag and "MAIN_LOOP_NOT_CONFIRMED" in diag
assert "NOT_OBSERVABLE=TV_active_pixels" in video
assert "PSNES_BOOT_LIMITATION=" in diag
assert '\"diagnostico_audio.txt\"' in fs and '\"diagnostico_snes.txt\"' in fs
assert "strcmp(ent.d_name,\"diagnostico_pantalla_snes.txt\")" in fs
assert 'strncmp(ent.d_name,"diagnostico",11)' in fs
assert "sysFsUnlink(full)" in fs
assert '#define sf_cobra_diag_startup(stage) (0)' in main
assert '#define sf_cobra_diag_report_previous() ((void)0)' in main
# pSNES, PS2/PS3/PS1 and BDISO payload and launches not changed by a screen TXT.
for path,blob in {
  "src/ps3/sf_cobra_mount.c":"fe9faf92587a9c86ae866502ef5346f98fb76715",
  "src/ps3/sf_ps2_hen.c":"f20a63d83596a9b5079fd056e261656a4f536201",
  "src/ps3/ps3_webman.c":"333f9d3fe75f3ebbdc0840ddb696385306919eca",
}.items():
    assert subprocess.check_output(["git","hash-object",path],text=True).strip()==blob
assert 'make_self "$tmp/psnes_edge.elf" "$target"' in get("tools/build_psnes_plain_self.sh")
assert "SF_V15_45_AUDIO_ISOLATION_TEST 1" in main
print("PASS V15.48 forensic video report integration: stable single TXT, PS3 state, config probes, boot merge, SHA and code isolation")
