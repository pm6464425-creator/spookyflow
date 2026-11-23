#!/usr/bin/env python3
"""MI PS3: create a surgical update containing ONLY the new executable.

The original MI PS3 installation already owns the 127 files, including
Arcade cover art missing from a clean build.  This update must not include
or overwrite unrelated engines, artwork, settings or caches. Install it
over the original SPKF00001 application; never uninstall the original.
"""
from __future__ import annotations
import argparse
import hashlib
from pathlib import Path
import struct
import sys
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/"tests"))
from verify_compiled_pkg import decrypt_debug_pkg, parse_sfo, be32

EXPECTED_FULL_SHA256="24df719e3b4e6ef4425b1175bd61ed9db90433f0e9718d6d8afb76cdc944c5da"
EXPECTED_TITLE="MI PS3"
EXPECTED_ID="SPKF00001"
EXPECTED_FILES={"ICON0.PNG","PARAM.SFO","USRDIR/EBOOT.BIN"}
def items(path:Path):
    pkg=path.read_bytes()
    assert pkg[:4]==b"\x7fPKG" and pkg[4:8]==b"\x00\x00\x00\x01"
    assert struct.unpack_from(">Q",pkg,0x18)[0]==len(pkg)
    assert pkg[0x30:0x60].startswith(b"UP0001-SPKF00001")
    payload=decrypt_debug_pkg(pkg)
    count=be32(pkg,0x14)
    result={}
    for i in range(count):
        no,nl,fo,fl,typ,_=struct.unpack_from(">IIQQII",payload,32*i)
        assert no>=32*count and no+nl<=len(payload)
        assert fo+fl<=len(payload)
        name=bytes(payload[no:no+nl]).decode("ascii")
        assert name not in result
        if fl:result[name]=bytes(payload[fo:fo+fl])
    return result

def extract(source:Path,destination:Path):
    assert hashlib.sha256(source.read_bytes()).hexdigest()==EXPECTED_FULL_SHA256,\
        "Refusing a full PKG which is not the exact previously tested release"
    data=items(source)
    assert len(data)>=90 and EXPECTED_FILES.issubset(data)
    info=parse_sfo(data["PARAM.SFO"])
    assert info["TITLE"]==EXPECTED_TITLE and info["TITLE_ID"]==EXPECTED_ID
    assert data["USRDIR/EBOOT.BIN"][:4]==b"SCE\x00"
    for name in sorted(EXPECTED_FILES):
        path=destination/name
        path.parent.mkdir(parents=True,exist_ok=True)
        path.write_bytes(data[name])
        print("Preserve original over-install; only patch item:",name,len(data[name]),hashlib.sha256(data[name]).hexdigest())
    assert len([p for p in destination.rglob("*") if p.is_file()])==3
    print("PASS: 3 extracted items: original MI PS3 metadata/icon + modified EBOOT ONLY")

def verify(patch:Path,source:Path):
    patch_data=items(patch)
    full_data=items(source)
    assert set(patch_data)==EXPECTED_FILES,\
        ("NOT surgical: unexpected PKG entries", sorted(set(patch_data)^EXPECTED_FILES))
    for name in EXPECTED_FILES:
        assert patch_data[name]==full_data[name], ("Differs from fully tested executable",name)
    assert parse_sfo(patch_data["PARAM.SFO"])["TITLE_ID"]==EXPECTED_ID
    assert parse_sfo(patch_data["PARAM.SFO"])["TITLE"]==EXPECTED_TITLE
    assert len(patch_data["USRDIR/EBOOT.BIN"])>1000000
    assert 2000000<patch.stat().st_size<4000000,\
        ("Update unexpectedly contains extra resources",patch.stat().st_size)
    print("PASS: exactly 3 payload files, original icon + metadata, tested EBOOT")
    print("PASS: absolutely no Arcade, SNES, PS1, PS2, PS3, BDISO assets or settings in update")
    print("PASS: TitleID SPKF00001 / title MI PS3 unchanged")
    print("PASS: PKG bytes",patch.stat().st_size,"SHA256",hashlib.sha256(patch.read_bytes()).hexdigest())

if __name__=="__main__":
    p=argparse.ArgumentParser()
    p.add_argument("mode",choices=["extract","verify"])
    p.add_argument("source",type=Path)
    p.add_argument("target",type=Path)
    args=p.parse_args()
    if args.mode=="extract":extract(args.source,args.target)
    else:verify(args.target,args.source)
