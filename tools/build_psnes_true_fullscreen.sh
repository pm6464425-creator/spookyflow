#!/usr/bin/env bash
# V15.43: build pSNES 7.2 from pinned public GPL source with an actual
# edge-to-edge display viewport. This is NOT a configuration-only fix.
# The old pSNES libcross2d Tiny3DRenderer virtual screen was 848x512,
# and Tiny3D's default 720p viewport clips the drawable TV area by 54x24.
set -euo pipefail
out="$(realpath -m "$1")"
mkdir -p "$out"
src="$(realpath -m "$2")"
rev="f7365164e1cfa82f1721fe7ad13f78548b0f1efa"

if [ ! -d "$src/.git" ]; then
    git clone -q --depth=1 https://github.com/theheroGAC/pemu-ps3 "$src"
fi
test "$(git -C "$src" rev-parse HEAD)" = "$rev" || {
    echo "FATAL: upstream pSNES is NOT reviewed source commit $rev";
    exit 7;
}
git -C "$src" -c advice.detachedHead=false submodule update -q --init --depth=1 \
    external/libcross2d external/cores/snes9x
test "$(git -C "$src/external/libcross2d" rev-parse HEAD)" = \
     "9dfe2fcb4ccb2dd7be04718ee7ddb450abd85de1"

python3 - "$src" <<'PY'
import pathlib,sys
root=pathlib.Path(sys.argv[1])
p=root/'external/libcross2d/source/platforms/ps3/tiny3d_renderer.cpp'
s=p.read_text()
needle='    tiny3d_Init(1024 * 1024);\n    TINY3DRenderer::setSize(Vector2f(848, 512));'
replacement="""    tiny3d_Init(1024 * 1024);
    // SpookyFlow v15.43: allow pSNES to draw on the ENTIRE output, 16:9.
    // The original default Tiny3D viewport reserves overscan margins:
    // 720p translates (54,24), instead of using the full framebuffer.
    // The libcross2d UI/skin uses 1280x720 logical coordinates.
    // Explicit Tiny3D viewport is documented to remove such black borders.
    TINY3DRenderer::setSize(Vector2f(1280, 720));
    tiny3d_UserViewport(1, 0.0f, 0.0f,
        (float) Video_Resolution.width / 1280.0f,
        (float) Video_Resolution.height / 720.0f,
        (float) Video_Resolution.width / 1920.0f,
        (float) Video_Resolution.height / 1080.0f);
"""
assert s.count(needle)==1, 'Tiny3DRenderer layout did not match reviewed source'
p.write_text(s.replace(needle,replacement))
assert "TINY3DRenderer::setSize(Vector2f(848, 512))" not in p.read_text()
q=root/'src/cores/main.cpp'
t=q.read_text()
anchor="""        cfg->loadGame(game);
        uiEmu->setExitOnStop(true);"""
fixed="""        cfg->loadGame(game);
#if defined(__PS3__) && defined(__PSNES__)
        // Force FULL AFTER the per-ROM config has loaded. A previous FIT or
        // ASPECT option must not restore letterboxing in SpookyFlow 16:9.
        if (auto *scale = cfg->get(PEMUConfig::OptId::EMU_SCALING, true)) {
            const auto values = scale->getArray();
            for (size_t i = 0; i < values.size(); ++i) {
                if (values[i] == "FULL") {
                    scale->setArrayIndex((int)i);
                    break;
                }
            }
        }
        boot_log("SF_VIDEO_MODE=TRUE_16_9_FULL after per-game config");
#endif
        uiEmu->setExitOnStop(true);"""
assert t.count(anchor)==1, 'Main ROM loading path did not match reviewed pSNES source'
q.write_text(t.replace(anchor,fixed))
assert q.read_text().count('SF_VIDEO_MODE=TRUE_16_9_FULL')==1
print('V15.43 source patches verified: 1280x720 Tiny3D viewport + FULL after ROM config')
PY

cmake -S "$src" -B "$out/compiled" \
    -DPLATFORM_PS3=ON -DOPTION_EMU=psnes -DOPTION_SSCRAP=OFF \
    -DOPTION_MPV_PLAYER=OFF -DOPTION_FREETYPE=ON \
    -DCMAKE_BUILD_TYPE=Release -DCMAKE_TRY_COMPILE_TARGET_TYPE=STATIC_LIBRARY
cmake --build "$out/compiled" --target psnes --parallel 2

# This is an unmodified PPU app entry point: SpookyFlow will sign it as
# standalone type4 APP SELF, NEVER as NPDRM, for sysProcessExitSpawn2.
elf="$out/compiled/src/cores/psnes/psnes"
test -s "$elf"
file "$elf"
ppu-readelf -h "$elf" | grep -E 'Class:|Machine:|Entry point|Type:'
ppu-readelf -h "$elf" | grep -q 'Machine:.*PowerPC64'
printf '%s\n' "Native pSNES 7.2 PowerPC64 ELF source build passed."
cp "$elf" "$out/PSNES_TRUE_FULLSCREEN.elf"
sha256sum "$out/PSNES_TRUE_FULLSCREEN.elf" | tee "$out/original_build_sha256.txt"
