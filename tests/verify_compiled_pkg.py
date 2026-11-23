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

EXPECTED_TITLE = "MI PS3"
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
    assert 2 <= count <= 180, "Invalid item count"  # 100 original + Arcade-only dual engines and config
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
    # V15.53: the actual launcher MUST contain the signed, directly spawnable
    # native PS1 RSX engine, not only a source or a separate installer.
    px = files.get("USRDIR/PS1ENGINE/PS1_ENGINE.SELF")
    assert px is not None, "Missing built-in independent PS1 16:9 renderer SELF"
    assert 150_000 <= len(px) <= 16_000_000
    assert px[:4] == b"SCE\x00", "PS1 game engine not a PS3 signed SELF"
    assert struct.unpack_from(">I",px,0x7c)[0]==4, "PS1 SELF must be APP type-4; NPDRM would not spawn"
    pxhash=hashlib.sha256(px).hexdigest()
    engheader=Path("src/ps3/sf_ps1_engine_hash.h").read_text()
    assert ('#define SF_PS1_ENGINE_SELF_SHA256 "'+pxhash+'"') in engheader
    assert ('#define SF_PS1_ENGINE_SIZE '+str(len(px))+'ULL') in engheader
    assert 'k_sf_ps1_engine_hash[32]' in engheader
    assert len(files["USRDIR/PS1ENGINE/ps1titles.txt"])>50000
    assert b"GNU GENERAL PUBLIC LICENSE" in files["USRDIR/PS1ENGINE/LICENSE.txt"]
    assert b"Calyps0man" in files["USRDIR/PS1ENGINE/SOURCE.txt"]
    print("PASS V15.53 actual integrated source-built native PS1 FULL_16_9 RSX APP SELF:",len(px),pxhash)

    # V15.31: verify the exact new SNES case exists in the built PKG.
    snes_case = files.get("USRDIR/CASES/snes.png")
    assert snes_case is not None, "SNES case PNG missing from installer"
    assert snes_case == Path("pkgfiles/USRDIR/CASES/snes.png").read_bytes(), (
        "Installer SNES case differs from generated source")
    assert snes_case[:8] == bytes((137, 80, 78, 71, 13, 10, 26, 10)), (
        "Broken packaged SNES PNG signature")
    assert snes_case[16:24] == struct.pack(">II", 550, 500), (
        "Unexpected packaged SNES case resolution")
    print("PASS compiled SNES transparent case", len(snes_case), "bytes")

    # V15.32: the installable PKG MUST contain real verified SNES cover names.
    snes_names=files.get("USRDIR/snes_boxart_names.txt",b"").decode("utf-8").splitlines()
    assert len(snes_names)>=2500, ("SNES verified catalog missing/partial",len(snes_names))
    assert "Super Mario World (USA).png" in snes_names
    assert "Donkey Kong Country (USA).png" in snes_names
    assert "Legend of Zelda, The - A Link to the Past (USA).png" in snes_names
    print("PASS real packaged SNES Libretro index",len(snes_names),"filenames")
    # V15.35: reject the V15.34 gameplay/title-screen catalog sources.
    for forbidden in ("snes_title_names.txt", "snes_logo_names.txt",
                      "snes_snap_names.txt"):
        assert "USRDIR/" + forbidden not in files, ("Wrong SNES art source", forbidden)
    print("PASS SNES cover-only PKG; no gameplay screenshot sources bundled")

    # V15.39: inspect the REAL installable PKG to guarantee the entire
    # native PS3 pSNES engine is present (NOT merely a reference to it).
    engine = "USRDIR/SNESENGINE/"
    psnes = files.get(engine + "USRDIR/EBOOT.BIN")
    assert psnes is not None, "Native pSNES executable missing from SpookyFlow"
    assert len(psnes) == 2594800, "Unexpected native pSNES SELF size"
    assert hashlib.sha256(psnes).hexdigest() == (
        "6a0c3f62c9b4c45715e0041de9b7688652614e9933d0e0a4d9c6f6ccb0ebd707"
    ), "Embedded PS3 pSNES 7.2 binary is not the pinned official release"
    assert psnes[:4] == b"SCE\x00"
    # V15.41: the original PSNE EBOOT is type 8 NPDRM and is NOT usable
    # with sysProcessExitSpawn2. Require the converted, signed type-4 APP
    # PS3 SELF in the SAME SpookyFlow PKG or the build MUST fail.
    assert struct.unpack_from(">I", psnes, 0x7c)[0] == 8, (
        "Expected original pSNES EBOOT to be NPDRM")
    standalone = files.get(engine + "PSNES_ENGINE.SELF")
    assert standalone is not None, "Standalone pSNES APP SELF missing from compiled PKG"
    assert 2_000_000 <= len(standalone) <= 8_000_000, "Standalone PS3 APP SELF implausible size"
    assert standalone[:4] == b"SCE\x00", "Standalone pSNES is not a signed SELF"
    assert struct.unpack_from(">I", standalone, 0x7c)[0] == 4, (
        "Standalone pSNES SELF is NOT a PS3 APP: would return to XMB")
    # The signer randomizes encrypted SELF bytes. Verify the actual
    # release binary against the generated runtime C hash and embedded
    # SHA manifest (and converter checks decoded ELF roundtrip at build).
    signed_hash = hashlib.sha256(standalone).hexdigest()
    manifest = files.get(engine + "PSNES_ENGINE.sha256")
    assert manifest is not None and manifest.decode("ascii").strip() == signed_hash, (
        "Signed APP SELF differs from its embedded release digest")
    runtime_hash = Path("src/ps3/sf_psnes_plain_hash.h").read_text()
    assert ('#define SF_PSNES_APP_SELF_SHA256 "' + signed_hash + '"') in runtime_hash, (
        "Runtime APP SELF fingerprint does not match compiled package")
    assert "k_plain_sha256[32]" in runtime_hash
    assert ('#define PSNES_PLAIN_SIZE ' + str(len(standalone)) + 'ULL') in runtime_hash, (
        "Runtime PS3 APP SELF byte length differs from actual packaged engine")
    assert (
        '#define SF_PSNES_NATIVE_169_ELF_SHA256 '
        '"4815e9632f10f5f405e43d284b4d2857bb8415e7a71985afa85016d5863f516e"'
    ) in runtime_hash, "APP SELF was not built from the verified 16:9-viewport-patched official ELF"
    edge_manifest = files.get(engine + "PSNES_ENGINE.edge.sha256",b"").decode("ascii").strip()
    assert len(edge_manifest)==64 and all(c in "0123456789abcdef" for c in edge_manifest), (
        "V15.47 final decrypted ELF digest absent from real installable PKG")
    assert ('#define SF_PSNES_NATIVE_BORDER_FILL_ELF_SHA256 "' + edge_manifest + '"') in runtime_hash, (
        "V15.47 final ELF SHA manifest differs from the hash compiled into runtime")
    assert edge_manifest != "4815e9632f10f5f405e43d284b4d2857bb8415e7a71985afa85016d5863f516e"
    print("PASS V15.47 signed SNES engine, pinned intermediate, final signed-roundtrip ELF SHA:", signed_hash, edge_manifest)
    assert files[engine + "PARAM.SFO"].startswith(b"\x00PSF")
    assert b"PSNE00001" in files[engine + "PARAM.SFO"]
    assert files[engine + "USRDIR/romfs/skins/default/config.cfg"]
    assert files[engine + "USRDIR/skins/default/config.cfg"]
    assert len([k for k in files if k.startswith(engine)]) >= 57
    assert files[engine + "LICENSE.txt"] and files[engine + "SOURCE.txt"]
    print("PASS standalone native pSNES 7.2: 55 resource files + license/source")

    # Protect the four previously completed platforms' binary resources:
    # these SHA256 values are from the user's supplied V15.37 PKG.
    preserved = {
        "USRDIR/raw_iso.sprx": "da887373cc7010fff7df600da797f360c01b37e744cd3ce6a0d04b52969fbcb2",
        "USRDIR/ps2configs/SLES_501.20.ENC": "f1f39cdc6987c4c1c83f4a14cb688423c78a946c81d39ecd409ea54a2814016f",
        "USRDIR/ps2configs/SLUS_201.74.ENC": "29ab443ae1154f1ecca79417a9f7d369fdbe5c72c5c07dcebf6f11589acbe9bf",
        "USRDIR/TABS/bdiso.png": "783d454d3376500fbc764ac271486574f24c638bca89b0f8a88925ebe17445ea",
        "USRDIR/TABS/snes.png": "e3d3b66db0fd9c26383d89655b8f6cce61ac0699efbbbc0333635fb85e9c67d6",
    }
    for name, digest in preserved.items():
        assert hashlib.sha256(files[name]).hexdigest() == digest, (
            "Original V15.37 resource changed unexpectedly", name)
    print("PASS original PS1/PS2/PS3/BDISO package resources preserved")

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
