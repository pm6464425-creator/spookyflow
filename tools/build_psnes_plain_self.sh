#!/usr/bin/env bash
# Convert the original, SHA256-pinned FREE pSNES homebrew release from
# NPDRM EBOOT to a standalone PS3 APP SELF. No ROMs are modified.
set -euo pipefail
eboot="$1"
target="$2"
tmp="$(mktemp -d /tmp/sf_psnes_plain_XXXXXX)"
trap 'rm -rf "$tmp"' EXIT
sceto="$tmp/scetool"

# Reproducible public tools used only during compilation, never on the PS3.
SCETOOL_REV=d78d8986c9ab831f084516f21ecdf87080ac7b78
IRISMAN_REV=91ac4e820e76fb9c1bbf9ec0cdd7592deb35f70d
EB_SHA=6a0c3f62c9b4c45715e0041de9b7688652614e9933d0e0a4d9c6f6ccb0ebd707
ELF_SHA=6f621a419fc682af878b350e821b7906364c2f3712504bc5d96fe6b0cf5ebadf
SELF_SHA=62c3c4da28a99cfd73ff6924e9baf55c96e2c64ac26acbb9d423a2b8c7d68d06

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
printf '%s  %s\n' "$SELF_SHA" "$target" | sha256sum -c -
test "$(stat -c%s "$target")" -eq 2614944
python3 - "$target" <<'PY'
import pathlib, struct, sys
p=pathlib.Path(sys.argv[1]).read_bytes()
assert p[:4]==b'SCE\x00'
# PS3 SELF extended header app-info offset (0x28) + 12 = self_type.
appinfo=struct.unpack_from('>Q',p,0x28)[0]
kind=struct.unpack_from('>I',p,appinfo+12)[0]
assert kind==4, ('expected PS3 APP non-NPDRM SELF, got',kind)
print('PASS verified PS3 standalone APP SELF (type 4), no webMAN.')
PY
echo "pSNES standalone APP SELF verified and packaged from official release."
