#!/usr/bin/env python3
"""SpookyFlow V15.41: PS3 independent pSNES launch regression (no hardware)."""
from pathlib import Path
import subprocess

read=lambda p: Path(p).read_text(encoding="utf-8")
main=read("src/ps3/main.c")
bundle=read("src/ps3/sf_snes_bundle.c")
app=read("src/core/app.c")
builder=read("ci/build_ps3.sh")
converter=read("tools/build_psnes_plain_self.sh")
verify=read("tests/verify_compiled_pkg.py")
diagnosis=read("src/ps3/sf_snes_diag.c")
launcher=main.split("static int launch_snes_psnes(",1)[1].split("\nint main(",1)[0]
protected={
    "src/ps3/sf_cobra_mount.c":"fe9faf92587a9c86ae866502ef5346f98fb76715",
    "src/ps3/sf_ps2_hen.c":"f20a63d83596a9b5079fd056e261656a4f536201",
    "src/ps3/ps3_webman.c":"333f9d3fe75f3ebbdc0840ddb696385306919eca",
}
def hash_blob(p):
    return subprocess.check_output(["git","hash-object",p],text=True).strip()

checks=[
 ("01 native APP SELF compiled from verified original pSNES release",
  "tools/build_psnes_plain_self.sh" in builder and
  "6a0c3f62c9b4c45715e0041de9b7688652614e9933d0e0a4d9c6f6ccb0ebd707" in converter),
 ("02 verified official PowerPC64 ELF (no foreign binary)",
  "6f621a419fc682af878b350e821b7906364c2f3712504bc5d96fe6b0cf5ebadf" in converter and
  "ppu-readelf -h" in converter),
 ("03 runtime new APP SELF SHA256 check is mandatory",
  "sf_psnes_plain_ready();" in launcher and
  "DIAG_APP_SELF_PRECHECK_FAILED" in launcher and
  "k_plain_sha256" in bundle),
 ("04 original type8 NPDRM EBOOT not used as process target",
  "PSNES_ENGINE.SELF" in launcher and
  'static const char exe[] = "/dev_hdd0/game/PSNE00001/USRDIR/EBOOT.BIN";' not in main and
  "sysProcessExitSpawn2(exe, args, NULL" in launcher),
 ("05 PS3 process argv convention sends ONE ROM argument",
  "args[0] = rom_path;" in launcher and "args[1] = NULL;" in launcher and
  "args[0] = exe;" not in launcher and "args[2] = NULL;" not in launcher),
 ("06 native pSNES resource installation retained and never overwrites others",
  "sf_psnes_prepare();" in launcher and
  "if (ps3_fs_exists(PSNES_HOME)) {" in bundle and
  "return -74;" in bundle),
 ("07 compiled PKG verifies APP vs NPDRM and exact fingerprint",
  'files.get(engine + "PSNES_ENGINE.SELF")' in verify and
  'struct.unpack_from(">I", standalone, 0x7c)[0] == 4' in verify and
  'struct.unpack_from(">I", psnes, 0x7c)[0] == 8' in verify and
  '62c3c4da28a99cfd73ff6924e9baf55c96e2c64ac26acbb9d423a2b8c7d68d06' in verify),
 ("08 no webMAN, RetroArch or PKGLAUNCH in SNES launch",
  all(word not in launcher for word in ["ps3_webman_send(","ps3_webman_get(","PKGLAUNCH","RETROARCH"]) and
  "sysProcessExitSpawn2" in launcher),
 ("09 original ROMs untouched",
  all(word not in launcher for word in ["remove(rom_path","rename(rom_path",'fopen(rom_path, "wb")'])),
 ("10 reliable launch diagnosis retained for failed handoff",
  "sf_snes_diag_before_spawn(rom_path)" in launcher and
  "sf_snes_diag_on_startup();" in main and
  "DIAG_PSNES_BOOTLOG" in diagnosis),
 ("11 PS1 PS2 PS3 BDISO launch backends byte-for-byte preserved",
  all(hash_blob(p)==sha for p,sha in protected.items()) and
  all('host.launch_'+p+' =' in main for p in ("ps1","ps2","ps3","bdiso"))),
 ("12 focused error text for non-verified APP SELF",
  "Motor SNES integrado dañado o no verificado" in app and
  "snes: DIAG_SPAWN_KIND=SIGNED_APP_SELF" in launcher),
]
for label,ok in checks: print(("PASS " if ok else "FAIL ")+label)
passed=sum(bool(ok) for _,ok in checks)
print(f"V15.41 verified native PS3 standalone SNES: {passed}/{len(checks)}")
if passed!=len(checks): raise SystemExit(1)
