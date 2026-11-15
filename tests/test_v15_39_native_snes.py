#!/usr/bin/env python3
"""V15.39 native PS3 SNES launch regression checks; never touches user's PS3."""
import hashlib
import subprocess
from pathlib import Path

main=Path("src/ps3/main.c").read_text()
app=Path("src/core/app.c").read_text()
bundle=Path("src/ps3/sf_snes_bundle.c").read_text()
build=Path("ci/build_ps3.sh").read_text()
extract=Path("tools/bundle_psnes.py").read_text()
f=main.split("static int launch_snes_psnes(",1)[1].split("\nint main(",1)[0]
snes=app.split("if (a->tab == PLAT_SNES) {",1)[1].split("\n    /* V15.13:",1)[0]
def digest(path):
    return subprocess.check_output(["git","hash-object",path],text=True).strip()
protected={
    "src/ps3/sf_cobra_mount.c":"fe9faf92587a9c86ae866502ef5346f98fb76715",
    "src/ps3/sf_ps2_hen.c":"f20a63d83596a9b5079fd056e261656a4f536201",
    "src/ps3/ps3_webman.c":"333f9d3fe75f3ebbdc0840ddb696385306919eca",
}
# The existing V15.37 host regressions independently check all 4 backends.
checks=[
    ("A: only SNES launch enters internal pSNES installer",
     "host.launch_snes = launch_snes_psnes;" in main and
     "sf_psnes_prepare();" in f and
     "sf_psnes_prepare();" not in main.split("static int launch_snes_psnes(",1)[0]),
    ("B: pSNES arguments honor the real PS3 main(argc, argv[1]) API",
     'args[0] = exe;' in f and 'args[1] = rom_path;' in f and
     'args[2] = NULL;' in f and 'sysProcessExitSpawn2(exe, args,' in f),
    ("C: SNES is independent of webMAN/RetroArch",
     'ps3_webman_send(' not in f and 'launch_build(' not in f and
     'ps3_webman_get(' not in f and
     'sysProcessExitSpawn2' in f),
    ("D: ROM roots and automatic boxart untouched",
     '/dev_hdd0/SNES' in Path("src/core/scanner.c").read_text() and
     'SNES_BOXART_INDEX' in Path("src/ps3/main.c").read_text()),
    ("E: release and executable pinned by SHA256",
     '9c7a5f3eae36b08ef6fa0715b61daee343160eac33c3d03fa30ccbe41fc8f7fc' in build and
     '6a0c3f62c9b4c45715e0041de9b7688652614e9933d0e0a4d9c6f6ccb0ebd707' in extract),
    ("F: stage is atomic and never overwrites existing PSNE00001",
     'rename(PSNES_STAGE, PSNES_HOME)' in bundle and
     'if (ps3_fs_exists(PSNES_HOME)) {' in bundle and
     'return -74;' in bundle and
     'PSNES_MARKER' in bundle),
    ("G: original ROM files are never moved, deleted or rewritten",
     'remove(rom_path)' not in f and
     'rename(rom_path' not in f and
     'fopen(rom_path, "wb")' not in f and
     'rom_path' not in bundle),
    ("H: error messages are truthful, no request to install pSNES manually",
     'Instala pSNES 7.2' not in app and
     'PS3 no pudo ejecutar pSNES' in app and
     'pSNES existente incompatible' in app),
    ("I: separated PS3 launchers are not edited",
     all(digest(p)==h for p,h in protected.items())),
    ("J: compiled PKG checker insists on real bundled pSNES EBOOT",
     'USRDIR/EBOOT.BIN")' in Path("tests/verify_compiled_pkg.py").read_text() and
     '"Embedded PS3 pSNES 7.2 binary is not the pinned official release"' in
         Path("tests/verify_compiled_pkg.py").read_text()),
]
failed=[name for name,ok in checks if not ok]
for name,ok in checks:
    print(("PASS " if ok else "FAIL ")+name)
print("V15.39 standalone PS3 pSNES checks:",len(checks)-len(failed),"/",len(checks))
if failed: raise SystemExit(1)
