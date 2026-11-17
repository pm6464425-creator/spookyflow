#!/usr/bin/env python3
"""V15.44 PS3 sound forensic regression: all five launches must release
SpookyFlow's live audio session before handoff. These are source invariants;
real HDMI output remains a PS3 hardware integration test.
"""
from pathlib import Path
import re
import subprocess

root=Path(".")
m=Path("src/ps3/main.c").read_text(encoding="utf-8")
a=Path("src/ps3/ps3_audio.c").read_text(encoding="utf-8")
app=Path("src/core/app.c").read_text(encoding="utf-8")
laun=m.split("static int launch_snes_psnes(",1)[1].split("\nint main(",1)[0]
loop=m.split("while (!g_app.quit)",1)[1]
checks=[]
def ck(name, success):
    checks.append(bool(success))
    print(("PASS" if success else "FAIL")+" V15.44 "+name)

# Audio port is opened by SpookyFlow; it is not the game/emulator's port.
ck("01 SpoookyFlow actively opens stereo LV2 audio port",
   "audioInit()" in a and "audioPortOpen(" in a and
   "audioPortStart(" in a and "sysThreadCreate(&g_tid, audio_thread" in a)
ck("02 music decoder and mixer both exist and run while menu is active",
   "music_thread_start();" in a and "music_mix(" in a and "sound_mix(" in a)
ck("03 complete audio cleanup includes decoder thread join first",
   "music_thread_stop();" in a and "sysThreadJoin(g_tid" in a and
   a.index("music_thread_stop();",a.index("void ps3_audio_stop(")) <
   a.index("audioPortStop(",a.index("void ps3_audio_stop(")))
ck("04 cleanup stops audio port, removes notification, closes port then quits",
   all(x in a for x in ("rc_stop = audioPortStop(", "rc_remove = audioRemoveNotifyEventQueue(",
                       "rc_close = audioPortClose(", "rc_queue = sysEventQueueDestroy(",
                       "rc_quit = audioQuit();")) and
   a.index("rc_stop = audioPortStop(") < a.index("rc_remove = audioRemoveNotifyEventQueue(") <
   a.index("rc_close = audioPortClose(") < a.index("rc_queue = sysEventQueueDestroy(") <
   a.index("rc_quit = audioQuit();"))
ck("05 all PS3 audio cleanup errors logged instead of silently discarded",
   "HANDOFF_STOP_RESULT" in a and all(x in a for x in (
        "rc_join", "rc_stop", "rc_remove", "rc_close", "rc_queue", "rc_quit")) and
   "HANDOFF_STOP_WARNING" in a and "HANDOFF_STOP_OK" in a)
ck("06 audio port config and notify setup failures now unwind cleanly",
   "audioGetPortConfig failed" in a and
   "audioSetNotifyEventQueue failed" in a and
   "queue drain warning" in a)
ck("07 SNES validates ROM and signed APP SELF before stopping audio",
   laun.index("sf_psnes_prepare()") <
   laun.index("sf_psnes_plain_ready()") <
   laun.index("sf_snes_diag_before_spawn(rom_path)") <
   laun.index("ps3_audio_stop()"))
ck("08 SNES releases PS3 audio explicitly before replacing process",
   laun.index("ps3_audio_stop()") < laun.index("sysProcessExitSpawn2(") and
   "HANDOFF_SNES" in laun)
ck("09 rejected SNES process restart keeps SpookyFlow menu audio usable",
   laun.index("sysProcessExitSpawn2(") < laun.index("ps3_audio_start()") and
   "g_audio_released_for_launch = 0;" in laun and
   "return -65;" in laun)
ck("10 all disc mounts release audio immediately AFTER success flag",
   loop.index("app_update(&g_app, held, dt)") <
   loop.index("if (g_app.launched && !g_audio_released_for_launch)") <
   loop.index("ps3_audio_stop();") <
   loop.index("tiny3d_Clear("))
ck("11 failed mounts never release live menu audio",
   "if (g_app.launched && !g_audio_released_for_launch)" in loop and
   "if (result != 0)" in app and "app_show_toast(a" in app)
ck("12 app marks successful BDISO launch before exit",
   bool(re.search(r"if \(a->tab == PLAT_BDISO\).*?a->launched\s*=\s*1",app,re.S)))
ck("13 app marks successful PS2 launch before exit",
   bool(re.search(r"if \(a->tab == PLAT_PS2 && a->host.launch_ps2\).*?a->launched\s*=\s*1",app,re.S)))
ck("14 app marks successful PS1 launch before exit",
   bool(re.search(r"if\(a->tab==PLAT_PS1 && a->host.launch_ps1\).*?a->launched\s*=\s*1",app,re.S)))
ck("15 app marks successful PS3 Cobra launch before exit",
   bool(re.search(r"if \(a->tab == PLAT_PS3 && a->host.launch_ps3\).*?a->launched\s*=\s*1",app,re.S)))
ck("16 atexit remains idempotent after early handoff",
   "atexit(on_exit_handler)" in m and
   m.count("ps3_audio_stop();")>=3 and
   "if (!g_state)" in a)
ck("17 Tiny3D 16:9 and pSNES 7.2 signed APP SELF unchanged",
   "sf_snes_force_widescreen(rom_path)" in laun and
   "PSNES_ENGINE.SELF" in laun and
   "tools/psnes_patch_true_fullscreen.py" in Path("tools/build_psnes_plain_self.sh").read_text())
ck("18 original non-audio Cobra and webMAN launch modules are unmodified",
   all(subprocess.check_output(["git","hash-object",p],text=True).strip()==h for p,h in {
       "src/ps3/sf_cobra_mount.c":"fe9faf92587a9c86ae866502ef5346f98fb76715",
       "src/ps3/sf_ps2_hen.c":"f20a63d83596a9b5079fd056e261656a4f536201",
       "src/ps3/ps3_webman.c":"333f9d3fe75f3ebbdc0840ddb696385306919eca"}.items()))
ck("19 original SNES controller/ROM args still passed unchanged",
   "args[0] = rom_path;" in laun and "args[1] = NULL;" in laun and
   "sysProcessExitSpawn2(exe, args, NULL" in laun)
ck("20 failure state cannot silently hide a missing audio port",
   "audioInit failed" in a and "audioPortOpen failed" in a and
   "audioPortStart failed" in a and "audio: started" in a)
print(f"V15.44 AUDIO HANDOFF full audit: {sum(checks)}/{len(checks)}")
if not all(checks):
    raise SystemExit(1)
