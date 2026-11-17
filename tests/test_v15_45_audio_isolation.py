#!/usr/bin/env python3
"""V15.45 explicit PS3 audio isolation experiment, never a silent 'fix'.

When XMB-launched content has audio and ALL content launched from SpookyFlow
is silent, do a discriminating A/B test: SpookyFlow itself does not open
LV2 audio devices at all. Compare the SAME title in XMB vs in SpookyFlow.

All launch/game code and game audio output are left untouched.
"""
from pathlib import Path
import subprocess
src=Path("src/ps3/main.c").read_text()
log=Path("src/core/log.c").read_text()
audio=Path("src/ps3/ps3_audio.c").read_text()
app=Path("src/core/app.c").read_text()
test=Path("tests/test_snes_diag_log.c").read_text()
checks=[]
def test_case(name,cond):
    print(("PASS" if cond else "FAIL")+" V15.45 "+name)
    checks.append(bool(cond))
test_case("1 diagnostic build explicitly enables NO SpookyFlow UI audio",
          "#define SF_V15_45_AUDIO_ISOLATION_TEST 1" in src)
main_start=src.split("    sound_init();",1)[1].split("    if (ps3_render_init(",1)[0]
test_case("2 no audioInit, audioPortOpen or decoder thread in active startup branch",
          "#if SF_V15_45_AUDIO_ISOLATION_TEST" in main_start and
          "DIAG_NO_AUDIO_INIT=YES" in main_start and
          "ps3_audio_start();" in main_start.split("#else",1)[1] and
          "ps3_audio_start();" not in main_start.split("#else",1)[0])
launch=src.split("static int launch_snes_psnes(",1)[1].split("\nint main(",1)[0]
test_case("3 signed SNES APP SELF and existing ROM args completely untouched",
          "sf_psnes_plain_ready();" in launch and
          "PSNES_ENGINE.SELF" in launch and "args[0] = rom_path;" in launch and
          "args[1] = NULL;" in launch and "sysProcessExitSpawn2(exe, args," in launch)
test_case("4 no sound restart following an unsuccessful SNES direct spawn",
          "SF_V15_45_AUDIO_ISOLATION_TEST" in launch and
          "DIAG_SNES_SPAWN_REJECTED" in launch and
          "ps3_audio_start();" not in launch.split("#if SF_V15_45_AUDIO_ISOLATION_TEST",1)[1].split("#else",1)[0])
test_case("5 disc mount source remains exactly as V15.44",
          "DIAG_DISC_HANDOFF_SELECTED" in src and
          "if (g_app.launched && !g_audio_released_for_launch)" in src)
test_case("6 PS3/PS2/PS1/BDISO original mounts are byte-for-byte unchanged",
          all(subprocess.check_output(["git","hash-object",p],text=True).strip()==sha
              for p,sha in {
                "src/ps3/sf_cobra_mount.c":"fe9faf92587a9c86ae866502ef5346f98fb76715",
                "src/ps3/sf_ps2_hen.c":"f20a63d83596a9b5079fd056e261656a4f536201",
                "src/ps3/ps3_webman.c":"333f9d3fe75f3ebbdc0840ddb696385306919eca"}.items()))
test_case("7 preserve user's actual SNES custom video corrections",
          "sf_snes_force_widescreen(rom_path)" in launch and
          "tools/psnes_patch_true_fullscreen.py" in Path("tools/build_psnes_plain_self.sh").read_text())
test_case("8 all-platform audio lines now allowed into one diagnostic",
          'strncmp(msg, "audio: ", 7) == 0' in log and
          '"HANDOFF_"' in log and '"DIAG_"' in log)
test_case("9 no ROM/cache/cover noise dumped into diagnostic",
          'strncmp(msg, "snes: ", 6)' in log and
          'strncmp(s, "DIAG_", 5)' in log and
          '"COVER_GPU"' not in log)
test_case("10 log test covers actual audio errors and handoff",
          "HANDOFF_STOP_RESULT" in test and
          "HANDOFF_STOP_WARNING" in test and
          "DIAG_ISOLATION_TEST=ACTIVE" in test)
test_case("11 memory/resource closure remains idempotent",
          "ps3_audio_stop();" in src and
          "if (!g_state)" in audio and
          "HANDOFF_STOP_ALREADY_CLOSED" in audio)
test_case("12 deliberate no-UI-audio test visible in the report",
          'audio: DIAG_ISOLATION_TEST=ACTIVE V15.45' in src and
          'audio: DIAG_NO_AUDIO_INIT=YES' in src)
test_case("13 no intrusive PS3 global audio-system reconfiguration",
          all(w not in src for w in ("audioOutConfigure(", "audioOutSetState(","sysFsUnlink(")))
test_case("14 app still selects every original platform",
          all('host.launch_'+p+' =' in src for p in ("snes","ps1","ps2","ps3","bdiso")))
test_case("15 no wiping user music, settings, covers or game save data",
          all(x not in launch for x in ("remove(rom_path", "unlink(rom_path",
                                      'fopen(rom_path,"wb")')))
print(f"V15.45 isolating SpookyFlow audio and persisting sound diagnostics: {sum(checks)}/{len(checks)}")
if not all(checks): raise SystemExit(1)
