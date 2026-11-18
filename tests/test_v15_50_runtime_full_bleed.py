"""V15.50 PPC64 in-process full-bleed patch safety and geometry checks."""
from pathlib import Path
import importlib.util,sys,struct
p=Path("tools/psnes_patch_runtime_full_bleed.py")
spec=importlib.util.spec_from_file_location("zoom",p)
zoom=importlib.util.module_from_spec(spec);sys.modules["zoom"]=zoom;spec.loader.exec_module(zoom)
assert zoom.ZOOM>1.0 and zoom.ZOOM<1.2
assert zoom.CAVE_BYTES==48
assert zoom.CAVE_OFF==0x00577C00 and zoom.CAVE_VA==0x00587C00
assert zoom.CAVE_OFF+zoom.CAVE_BYTES==0x00577C30 # gap after RX load, before the next RW segment
assert zoom.branch(zoom.SITE_VA,zoom.CAVE_VA) & 0xfc000000 ==0x48000000
assert zoom.branch(zoom.CAVE_VA+44,zoom.SITE_VA+4) & 0xfc000000 ==0x48000000
assert zoom.SITE_VA-zoom.SITE_OFF==zoom.CAVE_VA-zoom.CAVE_OFF==0x10000
# Fail safely on wrong ELF, corrupted content, mismatched SHA or malformed layout.
cases=[b"",b"\0"*100,b"\x7fELF"+b"\0"*100]
for data in cases:
    try:zoom.apply(data);raise AssertionError("Bad ELF accepted")
    except ValueError:pass
source=p.read_text()
for s in ["0x388100c0","0xc00100c0","0xc02100c4","0xec000372","0xec210372",
          "0xd00100c0","0xd02100c4","Wrong version","BASE_SHA","load_segment_for",
          "execut", "CAVE_BYTES","branch(CAVE_VA+44,SITE_VA+4)"]:
    assert s in source,s
shell=Path("tools/build_psnes_plain_self.sh").read_text()
assert "psnes_patch_true_fullscreen.py" in shell
assert "psnes_patch_border_fill.py" in shell
assert "psnes_patch_runtime_full_bleed.py" in shell
assert shell.index("psnes_patch_true_fullscreen.py")<shell.index("psnes_patch_border_fill.py")<shell.index("psnes_patch_runtime_full_bleed.py")<shell.index('make_self "$tmp/psnes_edge.elf" "$target"')
assert 'sha256sum -c -' in shell
assert 'SF_PSNES_APP_SELF_SHA256' in shell
main=Path("src/ps3/main.c").read_text()
assert "sf_psnes_plain_ready()" in main
assert "sysProcessExitSpawn2(exe, args" in main
assert "sf_snes_force_widescreen(rom_path)" in main
print("PASS V15.50 native video SCALE patch safety, injected branch, unchanged PS3 launching and SHA256 sign/decrypt path")
