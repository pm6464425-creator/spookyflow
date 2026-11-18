#!/usr/bin/env python3
"""V15.53 safe alternate PS1 fullscreen renderer integration invariants."""
from pathlib import Path
import subprocess
read=lambda p:Path(p).read_text()
main=read("src/ps3/main.c")
native=read("src/ps3/sf_ps1_mount.c")
head=read("src/ps3/sf_ps1_mount.h")
engine=read("src/ps3/sf_ps1_engine.c")
builder=read("tools/build_ps1_fullbleed_engine.sh")
patch=read("tools/patch_ps1_fullbleed_source.py")
ci=read("ci/build_ps3.sh")
assert "SF_PS1_ENGINE_EXEC" in engine
assert "mbedtls_sha256_update_ret" in engine
assert "SF_PS1_ENGINE_SIZE" in engine
assert "k_sf_ps1_engine_hash" in engine
assert '"SCE\\x00"' in engine
assert "SF_PS1_ENGINE_SELF_SHA256" in engine
assert "sf_ps1_engine_ready()" in main
assert "sf_ps1_fullbleed_cue_alias(path,cue_alias,sizeof cue_alias)" in main
assert "sysProcessExitSpawn2(sf_ps1_engine_path(),args,NULL,NULL" in main
assert "PS1_DIAG STAGE=NEW_NATIVE_RENDERER" in main
assert "emulator!=1" in main # deliberate fallback choice for Sony native
assert "rc=sf_ps1_mount(path,emulator)" in main
assert "host.launch_ps1 = launch_ps1_forensic" in main
assert "sf_ps1_join_mount()" in main
assert "sysProcessExitSpawn2(exe, args" in main # preserved working SNES
assert "sf_snes_force_widescreen(rom_path)" in main
assert "ps3_audio_stop();" in main
assert "sf_ps1_fullbleed_cue_alias" in head
assert "read_cue(cue)" in native and "s_plan.image" in native
assert "FULLBLEED_CUE_ALIAS" in native
assert 'fopen(cue,"wb")' not in native
assert "fopen(target,\"wb\")" in native
assert 'root[]=SF_DATA_DIR "/ps1_fullbleed_cues"' in native
assert 'SF_PS1_ENGINE.SELF' not in main # correct actual path constant in engine
assert "cfg.aspect=ASPECT_FULL" in patch
assert "cfg.screen_w=110" in patch and "cfg.screen_h=110" in patch
assert "config_game_begin(game_name)" in patch
assert "run_game(sf_ps1_rom_arg)" in patch
assert "PS3DEV" in builder and "make_self" in builder
assert "cce9fc67f46a495d4746aa49cff8e3367a9a315e" in builder
assert "pkgfiles/USRDIR/PS1ENGINE" in ci
assert 'make pkg SF_TLS=1' in ci
for path,sha in {
  "src/ps3/sf_cobra_mount.c":"fe9faf92587a9c86ae866502ef5346f98fb76715",
  "src/ps3/sf_ps2_hen.c":"f20a63d83596a9b5079fd056e261656a4f536201",
  "src/ps3/ps3_webman.c":"333f9d3fe75f3ebbdc0840ddb696385306919eca",
  "tools/psnes_patch_runtime_full_bleed.py":"1b6793938b0d2fc5b7d1a03bd8590d2137478c0c"
}.items():
  assert subprocess.check_output(["git","hash-object",path],text=True).strip()==sha, path
print("PASS V15.53: source pinned real-native PS1 RSX 16:9 and 110% overscan, validated CUE alias, SONY/SNES fallback preservation")
