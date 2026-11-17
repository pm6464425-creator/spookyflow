#!/usr/bin/env python3
"""V15.49: proof from the actual V15.48 user's report and production source."""
from pathlib import Path
import subprocess
p=lambda name:Path(name).read_text()
src=p("src/ps3/sf_snes_screen.c")
main=p("src/ps3/main.c")
diag=p("src/ps3/sf_video_diag.c")
test=p("tests/test_v15_49_cfg_repair.c")
assert "static int write_atomic(" in src
assert "RENAME_TARGET_FAILED" in src and "TEMP_CREATE_FAILED" in src
assert "BACKUP_VERIFIED" in src and "ROLLED_BACK_ORIGINAL" in src
assert "CFG_RECOVERY stage=ORIGINAL_RESTORED" in src
assert "CFG_GLOBAL_POSTWRITE" in src and "CFG_PER_ROM_POSTWRITE" in src
assert "sf_snes_config_video_probe(SNES_CFG_HOME" in src
assert 'SNES_CFG_HOME "/config.cfg"' in src
assert "SF_SN_CFG_BACKUP_DIR" in src
assert "sysFsChmod(path,0666)" in src and "SF_SN_TEST_RENAME_EEXIST" in src
assert "sf_snes_force_widescreen(rom_path)" in main
assert "video_diag: PRESPAWN_CONFIG_WRITER_RESULT=" in diag
assert "CONFIG_UPDATE_FAILED" in main and "FULL_16_9" in main
assert 'sysProcessExitSpawn2(exe, args' in main
assert "PSNES_ENGINE.SELF" in main
assert "DENY_TEMP" in test and "DENY_DIRECT" in test
assert 'make_self "$tmp/psnes_edge.elf" "$target"' in p("tools/build_psnes_plain_self.sh")
for path,blob in {
 "src/ps3/sf_cobra_mount.c":"fe9faf92587a9c86ae866502ef5346f98fb76715",
 "src/ps3/sf_ps2_hen.c":"f20a63d83596a9b5079fd056e261656a4f536201",
 "src/ps3/ps3_webman.c":"333f9d3fe75f3ebbdc0840ddb696385306919eca",
}.items():
    assert subprocess.check_output(["git","hash-object",path],text=True).strip()==blob
print("PASS V15.49 exact HEN error repair, readback, rollback, persistent advanced TXT and unchanged launchers/audio/ROM resources")
