#!/usr/bin/env python3
"""V15.46 sound-only PS3 forensic integration: 18 independent checks.

No test can measure PS3 title PCM after SpookyFlow has exited. Instead
record exactly which stage SpookyFlow actually reached, the zero/nonzero
status of its own LV2 audio port, and PS3 backend return codes. A/B
comparison of title sound from XMB is a human-performed hardware check.
"""
from pathlib import Path
import subprocess

text=lambda x: Path(x).read_text(encoding="utf-8")
log=text("src/core/log.c")
main=text("src/ps3/main.c")
app=text("src/core/app.c")
aud=text("src/ps3/ps3_audio.c")
hdr=text("src/ps3/ps3_platform.h")
sn=main.split("static int launch_snes_psnes(",1)[1].split("\nint main(",1)[0]
checks=[]
def chk(label,ok):
    print(("PASS" if ok else "FAIL")+" V15.46 "+label)
    checks.append(bool(ok))
chk("01 a genuinely separate audio-only report file", 
    '"/dev_hdd0/tmp/spookyflow/diagnostico_audio.txt"' in log and
    "static FILE *s_audio_diag;" in log)
chk("02 deliberately no game or non-audio file noise",
    'strncmp(msg,"audio: ",7)==0' in log and
    'strncmp(msg,"music: ",7)==0' in log and
    'fputs(line,s_audio_diag)' in log)
chk("03 audio record is flushed to disk BEFORE process exit",
    "fflush(s_audio_diag)==0" in log and
    "log_printf" in sn and "sysProcessExitSpawn2(" in sn)
chk("04 original SNES diagnostic preserved independently",
    'SPOOKYFLOW - DIAGNOSTICO DE ARRANQUE SNES V15.40' in log and
    's_audio_diag' in log and
    's_file_bytes' in log)
chk("05 one file only with bounded 64KB size and 56KB rotation",
    "SF_AUDIO_DIAG_MAX (64u * 1024u)" in log and
    "SF_AUDIO_DIAG_ROTATE (56u * 1024u)" in log and
    'append?"ab":"wb"' in log)
chk("06 each app launch has a separate audio-diagnosis record",
    "NUEVO INICIO DE SPOOKYFLOW - AUDIO V15.46" in log)
chk("07 startup tells actual PS3 audio mode, not assumed sound",
    "DIAG_SYSTEM=V15.46" in main and
    "DIAG_UI_AUDIO_MODE=ISOLATED" in main and
    "ps3_audio_diag_state()" in main)
chk("08 no PS3 own sound device/audio port created in A/B build",
    "#define SF_V15_45_AUDIO_ISOLATION_TEST 1" in main and
    "#if SF_V15_45_AUDIO_ISOLATION_TEST" in main)
chk("09 read-only PS3 resource status is real g_state",
    "int ps3_audio_diag_state(void) { return g_state; }" in aud and
    "int  ps3_audio_diag_state(void);" in hdr)
chk("10 SNES records state both before and after stop",
    "DIAG_SNES_PHASE=PRE_SPAWN ui_port_state=%d" in sn and
    "DIAG_SNES_PHASE=AFTER_STOP ui_port_state=%d" in sn and
    sn.index("DIAG_SNES_PHASE=PRE_SPAWN") <
    sn.index("ps3_audio_stop();") <
    sn.index("DIAG_SNES_PHASE=AFTER_STOP") <
    sn.index("sysProcessExitSpawn2("))
chk("11 SNES original external process explicitly not observable",
    "DIAG_SNES_OWN_AUDIO=UNOBSERVABLE_AFTER_PROCESS_REPLACEMENT" in sn)
chk("12 disc launches log before/after cleanup and uncertain game audio",
    "DIAG_DISC_PHASE=PRE_RELEASE" in main and
    "DIAG_DISC_PHASE=AFTER_RELEASE" in main and
    "DIAG_DISC_NEXT_STEP=RETURN_TO_XMB_GAME_AUDIO_NOT_ACCESSIBLE" in main)
chk("13 PS1/PS2/PS3/BDISO/SNES selection is captured",
    "DIAG_LAUNCH_REQUEST platform=%d" in app and
    "audio: DIAG_AUDIO_OBSERVATION=PRE_HANDOFF_ONLY" in app)
chk("14 each platform has separate backend return code",
    all(("platform="+p) in app for p in ("PS1","PS2","PS3","BDISO","SNES")) and
    "stage=WEBMAN_REQUEST rc=%d" in app and
    "stage=PREFLIGHT rc=%d" in app)
chk("15 failed mounting never mislabeled as audible game",
    "DIAG_GAME_AUDIO=HEARD" not in main+app and
    "GAME_AUDIO_NOT_ACCESSIBLE" in main)
chk("16 PS3/PS2/PS1/BDISO native Cobra/webMAN modules unchanged",
    all(subprocess.check_output(["git","hash-object",f],text=True).strip()==digest
        for f,digest in {
          "src/ps3/sf_cobra_mount.c":"fe9faf92587a9c86ae866502ef5346f98fb76715",
          "src/ps3/sf_ps2_hen.c":"f20a63d83596a9b5079fd056e261656a4f536201",
          "src/ps3/ps3_webman.c":"333f9d3fe75f3ebbdc0840ddb696385306919eca"}.items()))
chk("17 same signed pSNES APP SELF/ROM and 16:9 source retained",
    "PSNES_ENGINE.SELF" in sn and
    "args[0] = rom_path;" in sn and
    "sf_snes_force_widescreen(rom_path)" in sn and
    "tools/psnes_patch_true_fullscreen.py" in text("tools/build_psnes_plain_self.sh"))
chk("18 report gives explicit XMB A/B test instruction",
    "COMPARE_SAME_TITLE_DIRECT_XMB_VS_SPOOKYFLOW" in main and
    "Registra solo la actividad de SpookyFlow" in log)
print(f"V15.46 audio-only trace audit: {sum(checks)}/{len(checks)}")
if not all(checks): raise SystemExit(1)
