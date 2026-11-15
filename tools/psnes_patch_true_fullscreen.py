#!/usr/bin/env python3
"""Minimal, SHA-pinned pSNES 7.2 PS3 video fix (not a new emulator build).

The official free-homebrew pSNES 7.2 ELF cannot presently be reproduced
from the public repository because the PS3 graphics headers and base
interfaces have diverged. Instead, surgically correct exactly 36 bytes
in the verified original ELF, then re-sign as a PS3 type-4 APP SELF.

This only adjusts C2DUIVideo::updateScaling() and Tiny3D_Project2D().
Other executable code, SNES9x, control handling, audio and ROMs remain
byte-for-byte identical. All instruction/data locations were independently
identified by PowerPC64 disassembly, TOC references and matching upstream
Tiny3D source; every before byte and full ELF SHA256 must match.

Run: python3 tools/psnes_patch_true_fullscreen.py old.elf new.elf
"""
from __future__ import annotations

import hashlib
import struct
import sys
from pathlib import Path

ORIGINAL_SHA = "6f621a419fc682af878b350e821b7906364c2f3712504bc5d96fe6b0cf5ebadf"
PATCHED_SHA = "4815e9632f10f5f405e43d284b4d2857bb8415e7a71985afa85016d5863f516e"
ELF_SIZE = 16_653_712
# Source-based disassembly identifies the FULL branch:
#   C2DUIVideo::updateScaling()
#   0x1df39c: bt eq,1df480   (only if scaling string is 4 bytes long)
#   0x1df494: bf ne,1df3b8   (if the 4-byte value is not "FULL")
# Force the existing FULL branch for every SNES game independently of old
# global or per-ROM FIT/ASPECT/INTEGER settings.
# Tiny3D_Project2D() is at PPC64 vaddr 0x2c28e8. Its 2D viewport
# by default adds 24px top and 54px left at 720p and keeps a smaller
# virtual 848x512 canvas even on HDTV. The following patches remove
# only these offsets and set exact physical output ratios.
# For original source: pSNES v7.2 libcross2d tiny3d.c:
#  rsxtiny_ViewportTranslate(context,54,24,0,0);
#  rsxtiny_ViewportScale(context,848/611,674/512,...)
# Correct: translate(0,0) and scale(1280/848,720/512) at 720p.
# Likewise 1080x, and SD widths/heights. PS3 output 1080p is selected
# by system HDMI settings, not generated from native SNES pixels.
PATCHES = (
    (0x001CF39C, "418200e4", "480000e4", "C2DUIVideo FULL: always enter existing FULL test"),
    (0x001CF494, "4082ff24", "60000000", "C2DUIVideo FULL: bypass FIT/ASPECT fallback"),
    (0x002B29E8, "394affb8", "394a0000", "Tiny3D SD: width-72 overscan -> width"),
    (0x005DA4B8, "000000001001a910", "0000000010000840", "Tiny3D SD: y 16 -> 0 GOT pointer"),
    (0x005DA4C8, "000000001001a90c", "0000000010000840", "Tiny3D 720p: y 24 -> 0 GOT pointer"),
    (0x0062A838, "3f638e39", "3f700000", "Tiny3D 480p scaleY 480/512"),
    (0x0062A83C, "3f890000", "3f900000", "Tiny3D 576p scaleY 576/512"),
    (0x0062A84C, "42180000", "00000000", "Tiny3D SD translation X 38 -> 0"),
    (0x0062A850, "42580000", "00000000", "Tiny3D 720p translation X 54 -> 0"),
    (0x0062A854, "3fa88000", "3fb40000", "Tiny3D 720p scaleY 720/512"),
    (0x0062A858, "3fb1a656", "3fc13522", "Tiny3D 720p scaleX 1280/848"),
    (0x0062A85C, "42200000", "00000000", "Tiny3D 1080p translation Y 40 -> 0"),
    (0x0062A860, "427c0000", "00000000", "Tiny3D 1080p translation X 63 -> 0"),
    (0x0062A864, "3fee0000", "40070000", "Tiny3D 1080p scaleY 1080/512"),
    (0x0062A868, "4007ae14", "4010e7d9", "Tiny3D 1080p scaleX 1920/848"),
)

def check_geometry():
    """Verify mathematical basis independently from the original byte hex."""
    values = {
        0x0062A838: 480.0 / 512.0,
        0x0062A83C: 576.0 / 512.0,
        0x0062A84C: 0.0,
        0x0062A850: 0.0,
        0x0062A854: 720.0 / 512.0,
        0x0062A858: 1280.0 / 848.0,
        0x0062A85C: 0.0,
        0x0062A860: 0.0,
        0x0062A864: 1080.0 / 512.0,
        0x0062A868: 1920.0 / 848.0,
    }
    for off,before,after,note in PATCHES:
        if off in values:
            assert struct.pack(">f",values[off]).hex() == after, (hex(off),note)
    assert 1280/720 == 1920/1080 == 16/9
    assert len(PATCHES)==15

def apply_patch(original: bytes) -> bytes:
    if len(original)!=ELF_SIZE or hashlib.sha256(original).hexdigest()!=ORIGINAL_SHA:
        raise ValueError("Refusing to patch non-official pSNES 7.2 ELF")
    if original[:4] != b"\x7fELF" or original[4:6]!=b"\x02\x02":
        raise ValueError("Not a big-endian PowerPC64 ELF")
    # e_machine == EM_PPC64 (21)
    if struct.unpack_from(">H",original,18)[0] != 21:
        raise ValueError("Wrong CPU: expected PS3 PPU PowerPC64")
    check_geometry()
    output = bytearray(original)
    modifications=0
    previous=-1
    for off,before,after,label in PATCHES:
        src=bytes.fromhex(before)
        dst=bytes.fromhex(after)
        assert len(src)==len(dst)
        if off <= previous:
            raise ValueError("Patches overlap or are out of file order")
        previous=off+len(src)-1
        if output[off:off+len(src)]!=src:
            raise ValueError(f"Unexpected bytes at {off:#x}: {label}")
        output[off:off+len(src)]=dst
        modifications += sum(x!=y for x,y in zip(src,dst))
    actual=hashlib.sha256(output).hexdigest()
    if actual!=PATCHED_SHA or modifications!=36:
        raise ValueError(f"Unexpected signed-source change! sha={actual}, bytes={modifications}")
    assert len(output)==len(original)
    print(f"PASS: original pSNES 7.2 ELF -> edge-to-edge 16:9 PS3 viewport; {modifications} changed bytes")
    print("PASS: verified 480p/576p/720p/1080 output scaling + mandatory FULL")
    print(f"PASS: byte-for-byte deterministic patched ELF SHA256: {actual}")
    return bytes(output)

def main() -> int:
    if len(sys.argv)!=3:
        print("Usage: psnes_patch_true_fullscreen.py original.elf patched.elf", file=sys.stderr)
        return 2
    original=Path(sys.argv[1]).read_bytes()
    patched=apply_patch(original)
    target=Path(sys.argv[2])
    if target.resolve()==Path(sys.argv[1]).resolve():
        raise ValueError("Never overwrite upstream original ELF")
    target.write_bytes(patched)
    return 0

if __name__ == "__main__":
    sys.exit(main())
