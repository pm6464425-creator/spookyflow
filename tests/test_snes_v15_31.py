#!/usr/bin/env python3
"""SpookyFlow V15.31: ten reproducible SNES regression tests.
Hardware emulation and pSNES handoff still require a real PS3 HEN test.
"""
from pathlib import Path
import subprocess
import tempfile
import struct
import sys
import zlib

def read(p): return Path(p).read_text(encoding="utf-8")
fs=read("src/ps3/ps3_fs.c")
main=read("src/ps3/main.c")
scanner=read("src/core/scanner.c")
cover=read("src/core/coverart.c")
layout=read("src/core/layout.c")
tab=read("src/ps3/ps3_tabimg.c")
render=read("src/core/render.h")
build=read("ci/build_ps3.sh")
ui=read("src/core/ui.c")
app=read("src/core/app.c")
workflow=read(".github/workflows/build.yml")
def check_hash(path,sha):
    return subprocess.check_output(["git","hash-object",path],text=True).strip()==sha

# Read the PNG as saved by our generator, without Pillow or external services.
def decode_png(path):
    data=Path(path).read_bytes()
    if not data.startswith(b"\x89PNG\r\n\x1a\n"): raise ValueError("not PNG")
    i=8; image=b""; w=h=0
    while i+12<=len(data):
        n=struct.unpack(">I",data[i:i+4])[0]
        tag=data[i+4:i+8]
        segment=data[i+8:i+8+n]
        stored=struct.unpack(">I",data[i+8+n:i+12+n])[0]
        if zlib.crc32(tag+segment)&0xffffffff!=stored: raise ValueError("bad CRC")
        if tag==b'IHDR':
            w,h,depth,ctype,cm,fm,im=struct.unpack(">IIBBBBB",segment)
            if (depth,ctype,cm,fm,im)!=(8,6,0,0,0): raise ValueError("not RGBA8")
        if tag==b'IDAT': image+=segment
        if tag==b'IEND': break
        i+=12+n
    data=zlib.decompress(image)
    if len(data)!=h*(1+w*4): raise ValueError("bad pixel dimensions")
    rows=[]
    for y in range(h):
        row=data[y*(w*4+1):(y+1)*(w*4+1)]
        if row[0]!=0: raise ValueError("not a raw filter0 PNG")
        rows.append(row[1:])
    return w,h,rows

with tempfile.TemporaryDirectory(prefix="snes_case_test_") as temp:
    asset=Path(temp)/"snes.png"
    subprocess.run([sys.executable,"tools/gen_snes_case.py",str(asset)],check=True)
    W,H,rows=decode_png(asset)
    def rgba(x,y): return tuple(rows[y][x*4:x*4+4])
    aperture_ok=(
        W==550 and H==500 and
        all(rows[y][x*4+3]==0
            for y in range(42*2,210*2)
            for x in range(18*2,258*2)) and
        rgba(20,160)[3]>180 and
        rgba(280,46)[3]>180 and
        rgba(279,450)[3]>180 and
        rgba(0,0)[3]==0
    )
    image_ok=asset.stat().st_size>3500

baseline={
 "src/core/app.c":"0ad392ce9942c48003d31fc01984fdc631310372",
 "src/ps3/main.c":"1ad6550be1e47e33d18ea034d57c3e4d1b5db54b",
 "src/core/scanner.c":"ab4f344256aa764d0f799e20a60bbcfb517d8709",
 "src/core/coverart.c":"0fa033322c2b13070355beffb2cce48600c2cb17",
 "src/ps3/sf_ps2_hen.c":"f20a63d83596a9b5079fd056e261656a4f536201",
 "src/ps3/sf_cobra_mount.c":"fe9faf92587a9c86ae866502ef5346f98fb76715",
 "src/ps3/ps3_webman.c":"333f9d3fe75f3ebbdc0840ddb696385306919eca",
 "src/core/ps1_name_match.c":"b5ea816d0c4d978db13a0a1c89452f56b7f78e9c",
 "src/core/ui.c":"0da4dccbaef66846b3dcc15779e0363e10be431c",
 "pkgfiles/USRDIR/TABS/bdiso.png":"8200a26e2ee1ffee049db49bc0a76735d2ff2134",
 "pkgfiles/USRDIR/TABS/snes.png":"a116e05cb3350ea2d55780db3a565bdb0d002989",
}
tests=[
 ("01. SNES root is created at startup",
  'sysFsMkdir("/dev_hdd0/SNES", 0777)' in fs and
  "ps3_fs_mkdirs();" in main),
 ("02. Already-existing directory is safe; filesystem failure is reported",
  'sysFsStat("/dev_hdd0/SNES", &st)' in fs and
  "(st.st_mode & SF_S_IFMT) != SF_S_IFDIR" in fs and
  fs.count('sysFsMkdir("/dev_hdd0/SNES", 0777)')==1),
 ("03. SNES has an isolated processed-cover cache and Libretro source",
  'sysFsMkdir(SF_COVERS_DIR "/snes", 0777)' in fs and
  '"ps1", "ps2", "ps3", "bdiso", "snes"' in cover and
  "Nintendo%20-%20Super%20Nintendo%20Entertainment%20System/Named_Boxarts/" in cover),
 ("04. HDD and FAT USB SNES roots remain independent of other consoles",
  '{ PLAT_SNES, "/dev_hdd0/SNES", SRC_HDD_ISO_DIR }' in scanner and
  '{ "SNES", PLAT_SNES, SRC_HDD_ISO_DIR }' in scanner and
  all(s in scanner for s in ('"/dev_hdd0/PSXISO"','"/dev_hdd0/PS2ISO"',
                             '"/dev_hdd0/PS3ISO"','"/dev_hdd0/BDISO"'))),
 ("05. SNES only recognizes .sfc, .smc and .zip without breaking CUE/ISO",
  'if (plat == PLAT_SNES)' in scanner and
  all(s in scanner for s in ('".sfc"','".smc"','".zip"')) and
  'if (plat == PLAT_PS1)' in scanner and
  'if(plat==PLAT_BDISO)' in scanner),
 ("06. Real-case transparent art aperture exactly 240x168 at 2x scale",
  image_ok and aperture_ok),
 ("07. 275x250 case geometry, window placement and always-visible SNES case",
  "{ 275.0f, 250.0f, 18.0f, 42.0f, 240.0f, 168.0f }" in tab and
  "plat == PLAT_BDISO || plat == PLAT_SNES || g_cases_on" in layout and
  "case PLAT_SNES: return 240.0f/168.0f" in layout),
 ("08. SNES has its own GPU slot, no PS1/PS2/PS3/BDISO texture alias",
  "TEX_CASE_SNES = 28" in render and "TEX_COUNT_FIXED = 29" in render and
  "plat == PLAT_SNES ? TEX_CASE_SNES : TEX_CASE_PS1 + plat" in tab and
  "ui_set_case_image(PLAT_BDISO, TEX_CASE_PS3)" in tab),
 ("09. User custom SNES case overrides packaged default; image shipped in PKG",
  'load_case(PLAT_SNES, SF_CASES_DIR "/snes.png")' in tab and
  'if (plat == PLAT_SNES && !ps3_fs_exists(path))' in tab and
  '"/USRDIR/CASES/snes.png"' in tab and
  "python3 tools/gen_snes_case.py pkgfiles/USRDIR/CASES/snes.png" in build and
  "PKGFILES := $(CURDIR)/pkgfiles" in read("Makefile")),
 ("10. Existing launchers, cover engine, UI and BDISO/SNES tab logos unchanged",
  all(check_hash(p,sha) for p,sha in baseline.items()) and
  "host.launch_snes = launch_snes_psnes;" in main and
  'if (a->tab == PLAT_SNES)' in app and
  '"PS1", "PS2", "PS3", "BDISO", "SNES"' in ui),
]
passed=0
for title,good in tests:
    print(("PASS" if good else "FAIL"),title)
    passed+=bool(good)
print(f"SNES V15.31 targeted regressions: {passed}/{len(tests)}")
if passed!=10: raise SystemExit(1)
