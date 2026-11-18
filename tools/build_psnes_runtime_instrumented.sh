#!/usr/bin/env bash
set -euo pipefail
out="$(realpath -m "$1")"
src="$(realpath -m "$2")"
mkdir -p "$out"
if [ ! -d "$src/.git" ]; then git clone -q https://github.com/theheroGAC/pemu-ps3 "$src"; fi
git -C "$src" checkout -q f7365164e1cfa82f1721fe7ad13f78548b0f1efa
test "$(git -C "$src" rev-parse HEAD)" = f7365164e1cfa82f1721fe7ad13f78548b0f1efa
git -C "$src" -c advice.detachedHead=false submodule update --init --depth=1 external/libcross2d external/cores/snes9x
test "$(git -C "$src/external/libcross2d" rev-parse HEAD)" = 9dfe2fcb4ccb2dd7be04718ee7ddb450abd85de1
python3 tools/psnes_inject_runtime_video_probe.py "$src"
cmake -S "$src" -B "$out/compiled" \
  -DPLATFORM_PS3=ON -DOPTION_EMU=psnes -DOPTION_SSCRAP=OFF \
  -DOPTION_MPV_PLAYER=OFF -DOPTION_FREETYPE=OFF \
  -DCMAKE_BUILD_TYPE=Release -DCMAKE_TRY_COMPILE_TARGET_TYPE=STATIC_LIBRARY
cmake --build "$out/compiled" --target psnes --parallel 2
elf="$out/compiled/src/cores/psnes/psnes"
test -s "$elf"
ppu-readelf -h "$elf" | grep -q 'Machine:.*PowerPC64'
ppu-nm -C "$elf" | grep -F sf_runtime_video_telemetry
cp "$elf" "$out/PSNES_RUNTIME_GRAPHICS.elf"
sha256sum "$out/PSNES_RUNTIME_GRAPHICS.elf"
echo 'PASS compiled native PS3 pSNES runtime renderer telemetry ELF'
