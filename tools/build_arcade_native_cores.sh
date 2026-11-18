#!/usr/bin/env bash
# Only the ARCADE tab receives these signed PS3 APP SELF executables.
# Tested sources: FBNeo Retro Loader Plus v2.30 (CrystalCT, April 2021).
# No copyrighted game ROMs, BIOS ROMs or unrelated emulators are bundled.
set -euo pipefail
tmp="$(mktemp -d /tmp/sf_arcade_eng_XXXXXX)"
trap 'rm -rf "$tmp"' EXIT
pkg="$tmp/upstream.pkg"
source="https://github.com/crystalct/FBNeoRLPlus/releases/download/v2.30/FBNeoRLPlus.New_v2.30_2021-04-27.pkg"
echo "ARCADE: downloading native PS3 FBNeoRLPlus v2.30 upstream assets"
curl --retry 3 --retry-delay 3 -fsSL "$source" -o "$pkg"
test "$(stat -c%s "$pkg")" -eq 488200480
python3 tools/probe_fbneo_pkg.py "$pkg" "$tmp/extracted" | tail -20
root="$tmp/extracted/USRDIR"
test -s "$root/cores/fb_neo.SELF"
test -s "$root/MAME125/mame125.SELF"
test -s "$root/RELOAD.SELF"
test -s "$root/retro.db"
test -s "$root/retroarch.cfg"
python3 tools/build_arcade_engine_map.py \
  --db "$root/retro.db" --output pkgfiles/USRDIR/ARCADE/arcade_engines.tsv
# Exact upstream binaries are verified before decryption or patching.
printf '%s  %s\n' \
  a6430cc93e454fe126f1e011a093759a65c2997ce368afe744116b1ad8a679ce "$root/cores/fb_neo.SELF" \
  00c6ebc086bea423ad4dba244f24029865836c0227346c7db323496b26e113a7 "$root/MAME125/mame125.SELF" \
  b67ff7778a01519d77b8c2d9c85fcbc60d32847e37ee5f5d8ef2d1f9beb255de "$root/RELOAD.SELF" \
  3e347673824b936b2d6314bcae5a5121afdf74d05dd45f50841f9bdcc40fcd4a "$root/retro.db" \
  | sha256sum -c -
echo "ARCADE: building reproducible PS3 app re-signing tools"
sceto="$tmp/scetool"
git clone --depth 1 -q https://github.com/naehrwert/scetool "$sceto"
# Source project already uses this pinned public version for the SNES APP SELF.
test "$(git -C "$sceto" rev-parse HEAD)" = "d78d8986c9ab831f084516f21ecdf87080ac7b78" || {
  # Depth-limited clone may have advanced; fetch the frozen revision.
  git -C "$sceto" fetch -q --depth=1 origin d78d8986c9ab831f084516f21ecdf87080ac7b78
  git -C "$sceto" checkout -q d78d8986c9ab831f084516f21ecdf87080ac7b78
}
make -s -C "$sceto" -j 2 CC=g++ LDFLAGS=-lz >"$tmp/scetool-build.log" 2>&1 || {
  tail -30 "$tmp/scetool-build.log";exit 1;
}
mkdir -p "$sceto/data"
for f in keys ldr_curves vsh_curves;do
  curl -fsSL --retry 3 \
    "https://raw.githubusercontent.com/aldostools/IRISMAN/91ac4e820e76fb9c1bbf9ec0cdd7592deb35f70d/rawseciso/data/$f" \
    -o "$sceto/data/$f"
done
mkdir -p pkgfiles/USRDIR/cores pkgfiles/USRDIR/MAME125/cfg pkgfiles/USRDIR/MAME125/fonts
for rel in "cores/fb_neo.SELF" "MAME125/mame125.SELF" "RELOAD.SELF";do
  input="$root/$rel"
  file="$(basename "$rel" .SELF)"
  elf="$tmp/$file.elf"
  patched="$tmp/$file.patched.elf"
  dest="pkgfiles/USRDIR/$rel"
  echo "ARCADE: decrypt APP SELF $rel"
  (cd "$sceto" && ./scetool -d "$input" "$elf") >"$tmp/decrypt-$file.log" 2>&1 || {
    tail -25 "$tmp/decrypt-$file.log";exit 1;
  }
  python3 - "$elf" "$patched" "$rel" <<'PY'
from pathlib import Path
import sys,hashlib
src=Path(sys.argv[1]).read_bytes()
assert src[:4]==b'\x7fELF' and len(src)>1024, (sys.argv[3],src[:12])
old=b"FBNE00123";new=b"SPKF00001"
n=src.count(old)
# Patch 9-character content root (same length and no instruction displacement).
patched=src.replace(old,new)
assert len(patched)==len(src)
Path(sys.argv[2]).write_bytes(patched)
print("ARCADE ELF",sys.argv[3],"orig",len(src),"old title-ID strings",n,
      "sha256",hashlib.sha256(patched).hexdigest())
PY
  make_self "$patched" "$dest" >"$tmp/sign-$file.log" 2>&1 || {
    tail -25 "$tmp/sign-$file.log";exit 1;
  }
  (cd "$sceto" && ./scetool -d "$(realpath "$dest")" "$tmp/$file.roundtrip.elf") \
    > "$tmp/roundtrip-$file.log" 2>&1 || {
      tail -25 "$tmp/roundtrip-$file.log";exit 1;
    }
  cmp "$patched" "$tmp/$file.roundtrip.elf"
  python3 - "$dest" <<'PY'
from pathlib import Path
import sys,struct
v=Path(sys.argv[1]).read_bytes()
assert v[:4]==b"SCE\x00"
appinfo=struct.unpack_from(">Q",v,0x28)[0]
assert struct.unpack_from(">I",v,appinfo+12)[0]==4
print("PASS signed APP SELF",sys.argv[1],len(v))
PY
done
cp "$root/MAME125/cfg/default.cfg" pkgfiles/USRDIR/MAME125/cfg/default.cfg
cp "$root/MAME125/fonts/ui000-11.bdc" pkgfiles/USRDIR/MAME125/fonts/ui000-11.bdc
cp "$root/cores/autoconfig/SixAxis Controller.cfg" "pkgfiles/USRDIR/cores/SixAxis Controller.cfg"
# Use the original RetroArch config with the application identifier changed.
# Keep every original control binding but force 16:9 full-frame output.
python3 - "$root/retroarch.cfg" "$root/MAME125/init.ini" <<'PY'
from pathlib import Path
import re,sys
out=Path("pkgfiles/USRDIR/retroarch.cfg")
cfg=Path(sys.argv[1]).read_text()
assert cfg.count("FBNE00123")>6
cfg=cfg.replace("FBNE00123","SPKF00001")
updates={
 "video_fullscreen":"true",
 "video_windowed_fullscreen":"true",
 "video_aspect_ratio_auto":"false",
 "video_aspect_ratio":"1.777800",
 "video_scale_integer":"false",
 "video_shader_enable":"false",
 "input_overlay_enable":"false",
 "video_force_aspect":"true",
 "video_crop_overscan":"false"
}
for key,value in updates.items():
 pattern=rf'(?m)^{key}\s*=\s*"[^"]*"'
 new=f'{key} = "{value}"'
 assert re.search(pattern,cfg),key
 cfg=re.sub(pattern,new,cfg)
out.write_text(cfg)
init=Path(sys.argv[2]).read_text()
assert "/dev_hdd0/MAME" in init
init=init.replace("/dev_hdd0/MAME","/dev_hdd0/ARCADE")
Path("pkgfiles/USRDIR/MAME125/init.ini").write_text(init)
print("PASS fullscreen 16:9 config, all original bindings, isolated ARCADE paths")
PY
mkdir -p pkgfiles/USRDIR/cores/savefiles pkgfiles/USRDIR/cores/savestates
cat > pkgfiles/USRDIR/ARCADE/SOURCE_ENGINES.txt <<'NOTE'
ARCADE ONLY: FBNeo Retro Loader Plus v2.30 PS3 (CrystalCT, April 2021).
Original release:
https://github.com/crystalct/FBNeoRLPlus/releases/tag/v2.30
Sources and license details:
https://github.com/crystalct/FBNeoRLPlus
The original FBNeo and MAME 0.125 engine PS3 APP SELF files are extracted,
their original app ID is remapped to SPKF00001 in the decrypted ELF, then
re-signed as native type-4 APP SELF with the public PSL1GHT toolchain.
This installation includes NO game ROMs or BIOS archives.
Only the ARCADE launcher uses these resources.
NOTE
du -ah pkgfiles/USRDIR/cores/fb_neo.SELF pkgfiles/USRDIR/MAME125/mame125.SELF pkgfiles/USRDIR/RELOAD.SELF pkgfiles/USRDIR/retroarch.cfg pkgfiles/USRDIR/ARCADE/arcade_engines.tsv
echo "PASS: bundled PS3 dual arcade engines and compact compatibility map"
