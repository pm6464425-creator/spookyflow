#!/usr/bin/env bash
# Convert the original, SHA256-pinned FREE pSNES homebrew release from
# NPDRM EBOOT to a standalone PS3 APP SELF. No ROMs are modified.
set -euo pipefail
eboot="$(realpath "$1")"
target="$(realpath -m "$2")"
tmp="$(mktemp -d /tmp/sf_psnes_plain_XXXXXX)"
trap 'rm -rf "$tmp"' EXIT
sceto="$tmp/scetool"

# Reproducible public tools used only during compilation, never on the PS3.
SCETOOL_REV=d78d8986c9ab831f084516f21ecdf87080ac7b78
IRISMAN_REV=91ac4e820e76fb9c1bbf9ec0cdd7592deb35f70d
EB_SHA=6a0c3f62c9b4c45715e0041de9b7688652614e9933d0e0a4d9c6f6ccb0ebd707
ELF_SHA=6f621a419fc682af878b350e821b7906364c2f3712504bc5d96fe6b0cf5ebadf
FULL_ELF_SHA=4815e9632f10f5f405e43d284b4d2857bb8415e7a71985afa85016d5863f516e

printf '%s  %s\n' "$EB_SHA" "$eboot" | sha256sum -c -
command -v make_self >/dev/null
command -v ppu-readelf >/dev/null

git clone -q --depth 1 https://github.com/naehrwert/scetool "$sceto"
test "$(git -C "$sceto" rev-parse HEAD)" = "$SCETOOL_REV"
make -s -C "$sceto" -j 2 CC=g++ LDFLAGS=-lz > "$tmp/compile.log" 2>&1 || {
  echo "ERROR: scetool build failed"; tail -35 "$tmp/compile.log"; exit 1;
}
test -x "$sceto/scetool"
mkdir -p "$sceto/data"
for x in keys ldr_curves vsh_curves; do
  curl -fLsS --retry 3 --connect-timeout 15 \
    "https://raw.githubusercontent.com/aldostools/IRISMAN/$IRISMAN_REV/rawseciso/data/$x" \
    -o "$sceto/data/$x"
  test -s "$sceto/data/$x"
done

( cd "$sceto"; ./scetool -d "$eboot" "$tmp/psnes.elf" ) > "$tmp/decrypt.log" 2>&1 || {
  echo "ERROR: official pSNES 7.2 FREE NPDRM decryption failed"; tail -25 "$tmp/decrypt.log"; exit 1;
}
printf '%s  %s\n' "$ELF_SHA" "$tmp/psnes.elf" | sha256sum -c -
ppu-readelf -h "$tmp/psnes.elf" | grep -q 'Machine: *PowerPC64'
ppu-readelf -h "$tmp/psnes.elf" | grep -q 'Class: *ELF64'

# V15.43: patch ONLY 36 verified bytes inside original pSNES 7.2 ELF.
# These PowerPC64 instructions and constant slots cover Tiny3D viewport
# margins plus C2DUIVideo's FILT/FULL fallback. We retain original SNES9x
# emulation/audio/controllers and launcher (all byte-for-byte unchanged).
# Every original instruction, target, float and full SHA must match.
python3 tools/psnes_patch_true_fullscreen.py \
  "$tmp/psnes.elf" "$tmp/psnes_169.elf"
printf '%s  %s\n' "$FULL_ELF_SHA" "$tmp/psnes_169.elf" | sha256sum -c -
ppu-readelf -h "$tmp/psnes_169.elf" | grep -q 'Machine: *PowerPC64'

mkdir -p "$(dirname "$target")"
make_self "$tmp/psnes_169.elf" "$target" > "$tmp/sign.log" 2>&1
test "$(stat -c%s "$target")" -ge 2000000
test "$(stat -c%s "$target")" -le 8000000

# make_self randomizes cryptographic metadata, so signed SELF SHA256 is
# NOT stable across runs. Prove equivalence by decrypting back to the
# pinned PowerPC64 ELF before generating the per-build runtime fingerprint.
( cd "$sceto"; ./scetool -d "$(realpath "$target")" "$tmp/roundtrip.elf" ) \
  > "$tmp/roundtrip.log" 2>&1 || {
  echo "ERROR: cannot roundtrip the APP SELF"; tail -25 "$tmp/roundtrip.log"; exit 1;
}
test -s "$tmp/roundtrip.elf" || { echo "ERROR: converter did not produce a roundtrip ELF"; tail -30 "$tmp/roundtrip.log"; exit 1; }
printf '%s  %s\n' "$FULL_ELF_SHA" "$tmp/roundtrip.elf" | sha256sum -c -

python3 - "$target" src/ps3/sf_psnes_plain_hash.h <<'PY'
import hashlib, pathlib, struct, sys
src=pathlib.Path(sys.argv[1])
out=pathlib.Path(sys.argv[2])
p=src.read_bytes()
assert 2_000_000<=len(p)<=8_000_000
assert p[:4]==b'SCE\x00'
appinfo=struct.unpack_from('>Q',p,0x28)[0]
kind=struct.unpack_from('>I',p,appinfo+12)[0]
assert kind==4, ('expected PS3 APP non-NPDRM SELF, got',kind)
digest=hashlib.sha256(p).digest()
sha=digest.hex()
lines = [
    '/* GENERATED from verified pSNES 7.2 + 36-byte source-pinned 16:9 patch */',
    '#ifndef SF_PSNES_PLAIN_HASH_H',
    '#define SF_PSNES_PLAIN_HASH_H',
    '#define PSNES_PLAIN_SIZE '+str(len(p))+'ULL',
    '#define SF_PSNES_NATIVE_169_ELF_SHA256 "4815e9632f10f5f405e43d284b4d2857bb8415e7a71985afa85016d5863f516e"',
    '#define SF_PSNES_APP_SELF_SHA256 "'+sha+'"',
    'static const unsigned char k_plain_sha256[32] = {',
]
for i in range(0, 32, 8):
    lines.append('    '+','.join('0x%02x'%v for v in digest[i:i+8])+',')
lines += ['};', '#endif']
out.write_text('\n'.join(lines)+'\n', encoding='ascii')
src.with_suffix('.sha256').write_text(sha+'\n',encoding='ascii')
print('PASS pSNES APP SELF, roundtrip FULL-16:9 ELF pinned; signed size='+str(len(p))+' SHA256='+sha)
PY
test -s src/ps3/sf_psnes_plain_hash.h
echo "V15.43 pSNES standalone APP SELF from original ELF + verified 36-byte fullscreen patch."
