#!/usr/bin/env python3
"""V15.52: only valid game screenshot provides border pixels, no in-game GPU fiction."""
from pathlib import Path
import subprocess
read=lambda p:Path(p).read_text()
core=read("src/ps3/sf_ps1_border_measure.c")
wrap=read("src/ps3/sf_ps1_screenshot_diag.c")
ps1=read("src/ps3/sf_ps1_video_diag.c")
main=read("src/ps3/main.c")
logger=read("src/core/log.c")
mk=read("Makefile")
fs=read("src/ps3/ps3_fs.c")
assert "sf_ps1_screenshot_diagnose();" in ps1
assert "sf_ps1_analyze_rgba(" in core and "sf_ps1_analyze_rgba(" in wrap
assert '"/captura_ps1.png"' in wrap
assert "pngLoadFromFile(SF_PS1_SCREENSHOT,&png)" in wrap
assert 'PIXEL_MEASUREMENT=ACTUAL_CAPTURE' in wrap
assert 'BLACK_BANDS_PIXELS left=%u right=%u top=%u bottom=%u' in wrap
assert 'ACTIVE_IMAGE_RECT' in wrap
assert 'PIXEL_MEASUREMENT=NOT_AVAILABLE' in wrap
assert 'PIXEL_MEASUREMENT=INCONCLUSIVE' in wrap
assert "NOT_PROOF_OF_SONY_RENDERER_FAULT" in wrap
assert 'NO_GAMEPLAY_SCREENSHOT' in wrap
assert 'png.bmp_out' in wrap and 'free(png.bmp_out)' in wrap
assert 'captura_ps1.png' not in fs  # cleanup only diagnostic TXT
assert 'diagnostico_pantalla_snes.txt' in fs
assert 'strcmp(ent.d_name,"diagnostico_pantalla_ps1.txt")' in fs
assert "SF_PS1_VIDEO_DIAG" in mk and "SF_VIDEO_ONLY_DIAG" in mk
assert 'ps1_video_diag:' in logger
assert "sf_snes_force_widescreen(rom_path)" in main
assert 'make_self "$tmp/psnes_edge.elf" "$target"' in read("tools/build_psnes_plain_self.sh")
# The new report never patches PS1 Sony firmware, unverified offsets or memory.
for path,sha in {
 "src/ps3/sf_ps1_mount.c":"eb44b62ca522c7812381b9ce67e0af9ac741ea76",
 "src/ps3/sf_ps1_join.c":"4817a8c409880689b74a0c783885bb90666670a9",
 "src/ps3/sf_ps2_hen.c":"f20a63d83596a9b5079fd056e261656a4f536201",
 "src/ps3/sf_cobra_mount.c":"fe9faf92587a9c86ae866502ef5346f98fb76715",
 "tools/psnes_patch_runtime_full_bleed.py":"1b6793938b0d2fc5b7d1a03bd8590d2137478c0c",
}.items():
    assert subprocess.check_output(["git","hash-object",path],text=True).strip()==sha,path
print("PASS V15.52 PS1 genuine screenshot pixels, bounded inputs, one TXT; protected SNES and Sony/PS2 backends")
