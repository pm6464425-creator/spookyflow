#!/usr/bin/env bash
# Convert the original, SHA256-pinned FREE pSNES homebrew release from
# NPDRM EBOOT to a standalone PS3 APP SELF. No ROMs are modified.
set -euo pipefail
eboot="$(realpath "$1")"
target="$2"
tmp="$(mktemp -d /tmp/sf_psnes_plain_XXXXXX)"
trap 'rm -rf "$tmp"' EXIT
sceto="$tmp/scetool"

# Reproducible public tools used only during compilation, never on the PS3.
SCETOOL_REV=d78d8986c9ab831f084516f21ecdf87080ac7b78
IRISMAN_REV=91ac4e820e76fb9c1bbf9ec0cdd7592deb35f70d
EB_SHA=6a0c3f62c9b4c45715e0041de9b7688652614e9933d0e0a4d9c6f6ccb0ebd707
ELF_SHA=6f621a419fc682af878b350e821b7906364c2f3712504bc5d96fe6b0cf5ebadf

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

mkdir -p "$(dirname "$target")"
make_self "$tmp/psnes.elf" "$target" > "$tmp/sign.log" 2>&1
test "$(stat -c%s "$target")" -eq 2614944

# make_self randomizes cryptographic metadata, so signed SELF SHA256 is
# NOT stable across runs. Prove equivalence by decrypting back to the
# pinned PowerPC64 ELF before generating the per-build runtime fingerprint.
( cd "$sceto"; ./scetool -d "$(realpath "$target")" "$tmp/roundtrip.elf" ) \
  > "$tmp/roundtrip.log" 2>&1 || {
  echo "ERROR: cannot roundtrip the APP SELF"; tail -25 "$tmp/roundtrip.log"; exit 1;
}
printf '%s  %s\n' "$ELF_SHA" "$tmp/roundtrip.elf" | sha256sum -c -

python3 - "$target" src/ps3/sf_psnes_plain_hash.h <<'PY'
import hashlib, pathlib, struct, sys
src=pathlib.Path(sys.argv[1])
out=pathlib.Path(sys.argv[2])
p=src.read_bytes()
assert len(p)==2614944
assert p[:4]==b'SCE\x00'
appinfo=struct.unpack_from('>Q',p,0x28)[0]
kind=struct.unpack_from('>I',p,appinfo+12)[0]
assert kind==4, ('expected PS3 APP non-NPDRM SELF, got',kind)
digest=hashlib.sha256(p).digest()
sha=digest.hex()
out.write_text(
    '/* GENERATED from verified pSNES 7.2 original ELF; DO NOT EDIT */\n'
    '#ifndef SF_PSNES_PLAIN_HASH_H\n#define SF_PSNES_PLAIN_HASH_H\n'
    '#define PSNES_PLAIN_SIZE 2614944ULL\n'
    '#define SF_PSNES_APP_SELF_SHA256 "'+sha+'"\n'
    'static const unsigned char k_plain_sha256[32] = {\\n'
    + ''.join('    '+','.join('0x%02x'%v for v in digest[i:i+8])+',\\n' for i in range(0,32,8))
    + '};\\n#endif\\n'.replace('\\\\n','\\n'),
    encoding='ascii'
)
src.with_suffix('.sha256').write_text(sha+'\\n',encoding='ascii')
print('PASS pSNES APP SELF, roundtrip ELF pinned; actual signed SHA256='+sha)
PY
test -s src/ps3/sf_psnes_plain_hash.h
echo "pSNES standalone APP SELF verified and packaged from official release."
