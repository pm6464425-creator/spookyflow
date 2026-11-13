#!/usr/bin/env python3
"""Inspect the REAL compiled PSL1GHT debug PKG, not just sfo.xml/source.
Runs in CI after make pkg; includes structural, icon and SFO assertions.
This is not a console hardware/runtime compatibility test.
"""
from __future__ import annotations
import hashlib
from pathlib import Path
import struct
import sys

EXPECTED_TITLE = "MIS JUEGOS PS3 PS2 PS1"
EXPECTED_ID = "SPKF00001"

def be32(blob, pos):
    return struct.unpack_from(">I", blob, pos)[0]

def be64(blob, pos):
    return struct.unpack_from(">Q", blob, pos)[0]

def decrypt_debug_pkg(blob):
    """The PSL1GHT pkg.c debug-package SHA1 stream based on QA digest."""
    key = blob[0x60:0x70]
    ctx = bytearray(64)
    ctx[:8] = ctx[8:16] = key[:8]
    ctx[16:24] = ctx[24:32] = key[8:]
    offset, length = be64(blob, 0x20), be64(blob, 0x28)
    if offset < 0x80 or length > len(blob) or offset + length > len(blob):
        raise AssertionError("Invalid encrypted payload bounds")
    out = bytearray(blob[offset:offset+length])
    for i in range(0, len(out), 16):
        if i:
            num = struct.unpack_from(">Q", ctx, 0x38)[0]
            struct.pack_into(">Q", ctx, 0x38, num+1)
        pad = hashlib.sha1(ctx).digest()
        for j in range(min(16, len(out)-i)):
            out[i+j] ^= pad[j]
    return out

def parse_sfo(blob):
    assert blob[:4] == b"\x00PSF", "Wrong PARAM.SFO magic"
    _, version, key_offset, data_offset, count = struct.unpack_from("<4sIIII", blob)
    assert version == 0x101
    assert 0 < count < 200
    out = {}
    for i in range(count):
        entry = 20 + 16*i
        ko, typ, length, maximum, vo = struct.unpack_from("<HHIII", blob, entry)
        kpos = key_offset + ko
        vpos = data_offset + vo
        assert 0 <= kpos < len(blob) and 0 <= vpos <= len(blob)
        key = blob[kpos:blob.index(b"\x00", kpos)].decode("ascii")
        assert vpos + length <= len(blob), key
        if typ == 0x0404:
            out[key] = struct.unpack_from("<I", blob, vpos)[0]
        else:
            out[key] = blob[vpos:vpos+length].split(b"\x00")[0].decode("utf-8")
    return out

def verify(package):
    data = Path(package).read_bytes()
    assert data[:4] == b"\x7fPKG"
    assert data[4:8] == b"\x00\x00\x00\x01", "Expected PSL1GHT debug PS3 PKG"
    assert be64(data, 0x18) == len(data), "Declared PKG size differs from actual file"
    assert data[0x30:0x60].startswith(b"UP0001-SPKF00001"), "Application content ID changed"
    count = be32(data, 0x14)
    assert 2 <= count <= 100, "Invalid item count"
    decrypted = decrypt_debug_pkg(data)
    table_end = count * 32
    files = {}
    for i in range(count):
        name_off, name_len, file_off, file_len, typ, _ = struct.unpack_from(">IIQQII", decrypted, i*32)
        assert name_off >= table_end and name_off + name_len <= len(decrypted)
        assert file_off + file_len <= len(decrypted)
        name = bytes(decrypted[name_off:name_off+name_len]).decode("ascii")
        assert name not in files
        files[name] = bytes(decrypted[file_off:file_off+file_len])
        print("PKG item:", name, "bytes=", file_len, "type=", hex(typ))
    assert "ICON0.PNG" in files and "PARAM.SFO" in files
    assert "USRDIR/EBOOT.BIN" in files
    assert "USRDIR/raw_iso.sprx" in files, "Integrated BDISO plugin missing"
    assert "USRDIR/ps1_boxart_names.txt" in files, "Verified PS1 title catalog missing from installed PKG"
    catalog = files["USRDIR/ps1_boxart_names.txt"].decode("utf-8").splitlines()
    assert len(catalog) >= 7000, ("Only", len(catalog), "PS1 real boxart filenames")
    assert "Disney Tarzan (Spain).png" in catalog, "Tarzan Spanish cover absent"
    assert "Disney's Tarzan (USA).png" in catalog, "Tarzan USA cover absent"
    print("PASS real bundled Libretro PS1 cover names:", len(catalog))
    rawiso = files["USRDIR/raw_iso.sprx"]
    assert len(rawiso) == 7930 and rawiso[:4] == b"SCE\x00", (
        "Invalid embedded raw_iso.sprx",len(rawiso))

    # Exact encrypted PS2 Classics Rumble Racing CONFIG must be in the real
    # installable PKG, not merely mentioned by source code or a CI fixture.
    cfg = files["USRDIR/ps2configs/SLUS_201.74.ENC"]
    assert cfg[:4] == bytes((80,83,50,0)) and len(cfg) == 49152, "Rumble encrypted CONFIG missing or invalid"
    assert cfg[0x10:0x34] == b"2P0001-PS2U10000_00-0000111122223333", "Wrong placeholder CID"
    assert cfg == Path("pkgfiles/USRDIR/ps2configs/SLUS_201.74.ENC").read_bytes()
    print("PASS embedded Rumble Racing encrypted CONFIG, size:", len(cfg))

    icon = files["ICON0.PNG"]
    expected = Path("pkgfiles/ICON0.PNG").read_bytes()
    assert icon == expected, "Embedded XMB icon differs from new source image"
    assert icon.startswith(b"\x89PNG\r\n\x1a\n")
    assert icon[16:24] == struct.pack(">II", 320, 176)
    assert icon[25] == 6, "Need transparent RGBA PNG for XMB icon"
    sfo = parse_sfo(files["PARAM.SFO"])
    assert sfo["TITLE"] == EXPECTED_TITLE, sfo
    assert sfo["TITLE_ID"] == EXPECTED_ID, sfo
    print("PASS compiled TITLE:", sfo["TITLE"])
    print("PASS original Title ID:", sfo["TITLE_ID"])
    print("PASS custom icon matches", len(icon), "bytes, 320x176 RGBA")
    print("PASS compiled Cobra-bearing EBOOT present", len(files["USRDIR/EBOOT.BIN"]), "bytes")
    print("PASS integrity SHA256:", hashlib.sha256(data).hexdigest())

if __name__ == "__main__":
    verify(sys.argv[1] if len(sys.argv) > 1 else "spookyflow.pkg")
