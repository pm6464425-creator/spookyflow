#!/usr/bin/env bash
set -euo pipefail
out="$(realpath -m "$1")"
src="$(realpath -m "$2")"
repo='https://github.com/Calyps0man/pcsx_rearmed_for_ps3'
rev='cce9fc67f46a495d4746aa49cff8e3367a9a315e'
mkdir -p "$out"
if [ ! -e "$src/.git" ]; then git clone -q --depth 1 --branch ps3 "$repo" "$src"; fi
# Always compile the ORIGINAL PINNED upstream commit, even if ps3 branch moved.
# No PS1 game engine source is changed compared with V15.53.
git -C "$src" fetch -q --depth 1 origin "$rev"
git -C "$src" checkout -q --detach "$rev"
test "$(git -C "$src" rev-parse HEAD)" = "$rev"
python3 tools/patch_ps1_fullbleed_source.py "$src"
test -n "$PS3DEV"
if [ -z "$PSL1GHT" ]; then export PSL1GHT="$PS3DEV"; fi
export PATH="$PS3DEV/bin:$PS3DEV/ppu/bin:$PS3DEV/spu/bin:$PATH"
command -v ppu-gcc
command -v spu-gcc
command -v cgc
command -v cgcomp
command -v make_self
if ! grep -q '"memory"' "$PSL1GHT/ppu/include/rsx/rsx_function_macros.h"; then
  patch -d "$PSL1GHT" -p1 --forward < "$src/psl1ght-patches/0001-rsx-context-callback-clobbers.patch"
fi
grep -q '"memory"' "$PSL1GHT/ppu/include/rsx/rsx_function_macros.h"
# The already linked ELF is our needed artifact, not the upstream NPDRM PKG.
# The old toolchain lacks upstream sfo, so skip ONLY its final pkg metadata.
sed -i '/^sfo --title /,$d' "$src/ps3/build.sh"
(
 cd "$src"
 JOBS=2 PS3DEV="$PS3DEV" PSL1GHT="$PSL1GHT" bash ps3/build.sh sf_ps1_native
)
elf="$src/ps3/build/sf_ps1_native_stripped.elf"
test -s "$elf"
ppu-readelf -h "$elf" | grep -q 'Machine: *PowerPC64'
make_self "$elf" "$out/PS1_ENGINE.SELF"
test -s "$out/PS1_ENGINE.SELF"
cp "$src/ps3/data/ps1titles.txt" "$out/ps1titles.txt"
cp "$src/COPYING" "$out/LICENSE.txt"
cat > "$out/SOURCE.txt" <<EOF
Native PS1 engine PSXonPS3 (PCSX-ReARMed), GPLv2+, source:
https://github.com/Calyps0man/pcsx_rearmed_for_ps3
Original source revision: $rev
SpookyFlow changes: tools/patch_ps1_fullbleed_source.py
Compiled with tools/build_ps1_fullbleed_engine.sh
Uses RSX 16:9 stretched image with 110 percent centered overscan.
Sony's native PS1 emulator remains available as fallback; its save cards
are not automatically imported into this alternate engine.
EOF
python3 - "$out/PS1_ENGINE.SELF" "$out/ps1titles.txt" src/ps3/sf_ps1_engine_hash.h <<'PY'
from pathlib import Path
import hashlib,struct,sys
exe=Path(sys.argv[1]); titles=Path(sys.argv[2]); h=Path(sys.argv[3])
data=exe.read_bytes()
assert data[:4]==b'SCE\x00'
meta=struct.unpack_from('>Q',data,0x28)[0]
assert struct.unpack_from('>I',data,meta+12)[0]==4
assert 150000<len(data)<16000000 and titles.stat().st_size>50000
hash_=hashlib.sha256(data).digest()
h.write_text(
  "/* SHA-verified PS1 native APP SELF */\n"
  "#ifndef SF_PS1_ENGINE_HASH_H\n#define SF_PS1_ENGINE_HASH_H\n"
  "#define SF_PS1_ENGINE_SIZE "+str(len(data))+"ULL\n"
  '#define SF_PS1_ENGINE_SELF_SHA256 "'+hash_.hex()+'"\n'
  'static const unsigned char k_sf_ps1_engine_hash[32]={'+','.join('0x%02x'%v for v in hash_)+'};\n'
  "#endif\n")
print("PASS standalone PowerPC64 PSXonPS3 APP SELF",len(data),hash_.hex())
PY
