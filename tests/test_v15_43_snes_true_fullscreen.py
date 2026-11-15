#!/usr/bin/env python3
"""V15.43 SNES: verify 16:9 native PS3 ELF patch invariants on the build host.

The real 16MB ELF is verified/modified during ps3-pkg build using SHA256
of the official upstream homebrew. This fast check protects source/offset
intent and all unrelated PlayStation platforms before building that ELF.
"""
from __future__ import annotations
import importlib.util
import struct
import subprocess
from pathlib import Path

src=Path("tools/psnes_patch_true_fullscreen.py")
spec=importlib.util.spec_from_file_location("snv1543patch",src)
assert spec and spec.loader
p=importlib.util.module_from_spec(spec)
spec.loader.exec_module(p)
checks=[]

def check(label, result):
    checks.append((label, bool(result)))
    print(("PASS" if result else "FAIL")+" "+label)

p.check_geometry()
check("01 official PowerPC64 7.2 ELF SHA pinned",
      p.ORIGINAL_SHA=="6f621a419fc682af878b350e821b7906364c2f3712504bc5d96fe6b0cf5ebadf")
check("02 screen-fixed ELF derived digest is exact",
      p.PATCHED_SHA=="4815e9632f10f5f405e43d284b4d2857bb8415e7a71985afa85016d5863f516e")
check("03 patch has exactly 15 isolated known original offsets",
      len(p.PATCHES)==15 and
      all(p.PATCHES[i][0]+len(bytes.fromhex(p.PATCHES[i][1]))<=p.PATCHES[i+1][0]
          for i in range(14)))
check("04 exactly 36 bytes changed, no unreviewed edits",
      sum(sum(a!=b for a,b in zip(bytes.fromhex(prev),bytes.fromhex(new)))
          for _,prev,new,_ in p.PATCHES)==36)
d={off:(old,new) for off,old,new,_ in p.PATCHES}
check("05 force physical game scaling FULL, independent of per-game FIT",
      d[0x1cf39c]==("418200e4","480000e4") and
      d[0x1cf494]==("4082ff24","60000000"))
check("06 720p zero margins and exact 1280x720 projection",
      d[0x5da4c8][1]=="0000000010000840" and
      d[0x62a850][1]=="00000000" and
      bytes.fromhex(d[0x62a854][1])==struct.pack(">f",720/512) and
      bytes.fromhex(d[0x62a858][1])==struct.pack(">f",1280/848))
check("07 1080 line modes have zero offsets and 1920x1080 projection",
      d[0x62a85c][1]=="00000000" and
      d[0x62a860][1]=="00000000" and
      bytes.fromhex(d[0x62a864][1])==struct.pack(">f",1080/512) and
      bytes.fromhex(d[0x62a868][1])==struct.pack(">f",1920/848))
check("08 SD output modes still mapped edge-to-edge",
      d[0x2b29e8][1]=="394a0000" and
      d[0x5da4b8][1]=="0000000010000840" and
      bytes.fromhex(d[0x62a838][1])==struct.pack(">f",480/512) and
      bytes.fromhex(d[0x62a83c][1])==struct.pack(">f",576/512))
converter=Path("tools/build_psnes_plain_self.sh").read_text()
pkg=Path("ci/build_ps3.sh").read_text()
verifier=Path("tests/verify_compiled_pkg.py").read_text()
launch=Path("src/ps3/main.c").read_text()
check("09 stage only from original NPDRM EBOOT and original ELF",
      "tools/bundle_psnes.py" in pkg and
      "6a0c3f62c9b4c45715e0041de9b7688652614e9933d0e0a4d9c6f6ccb0ebd707" in converter and
      'printf \'%s  %s\\n\' "$ELF_SHA" "$tmp/psnes.elf"' in converter)
check("10 real ELF patched before self signing, SHA and roundtrip checked",
      converter.index("tools/psnes_patch_true_fullscreen.py") <
      converter.index('make_self "$tmp/psnes_169.elf"') and
      '$FULL_ELF_SHA' in converter and
      'sha256sum -c -' in converter and
      'roundtrip.elf' in converter)
check("11 runtime hash, dynamic size and type-4 APP self preserved",
      "PSNES_PLAIN_SIZE" in converter and
      "SF_PSNES_NATIVE_169_ELF_SHA256" in converter and
      "SF_PSNES_NATIVE_169_ELF_SHA256" in verifier and
      "sf_psnes_plain_ready()" in launch and
      "sysProcessExitSpawn2(exe, args," in launch)
check("12 none of PS1, PS2, PS3, BDISO native launch modules changed",
      all(subprocess.check_output(["git","hash-object",name],text=True).strip()==sha
          for name,sha in {
              "src/ps3/sf_cobra_mount.c":"fe9faf92587a9c86ae866502ef5346f98fb76715",
              "src/ps3/sf_ps2_hen.c":"f20a63d83596a9b5079fd056e261656a4f536201",
              "src/ps3/ps3_webman.c":"333f9d3fe75f3ebbdc0840ddb696385306919eca"}.items()))
check("13 existing SNES preview, cover art, diagnostics and ROM path unchanged",
      'sf_snes_diag_before_spawn(rom_path)' in launch and
      'sf_snes_force_widescreen(rom_path)' in launch and
      "SNES_BOXART_INDEX" in Path("src/core/snes_name_match.h").read_text() and
      "ps3_webman_send(" not in launch.split("static int launch_snes_psnes(",1)[1].split("\nint main(",1)[0])
check("14 no unsafe core library update: only pinned executable bytes patched",
      'git clone -q --depth 1 https://github.com/naehrwert/scetool' in converter and
      'ppu-readelf' in converter and
      'make_self "$tmp/psnes_169.elf"' in converter)
check("15 source attribution lists actual changed ELF and original", 
      "4815e9632f10f5f405e43d284b4d2857bb8415e7a71985afa85016d5863f516e" in pkg and
      "6f621a419fc682af878b350e821b7906364c2f3712504bc5d96fe6b0cf5ebadf" in pkg)
print(f"V15.43 SNES 16:9 true TV viewport: {sum(ok for _,ok in checks)}/{len(checks)}")
if not all(ok for _,ok in checks): raise SystemExit(1)
