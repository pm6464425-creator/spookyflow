#!/usr/bin/env python3
"""Make an ISOLATED FBNeo PS3 core variant for original-era Neo Geo chips.

Input: authenticated, already title-ID-remapped ELF from original FBNeoRLPlus
v2.30 core. Output: same-sized ELF with only validated BurnRomInfo entries
changed. Both the real original PS3 FBNeo core and all user ROM/BIOS files
remain UNTOUCHED.

Important: the original BIOS revisions have been used by old MAME, but this
patch has NOT been validated on physical PS3; ROM correctness does not prove
that vintage BIOS revisions are runtime-compatible with FBNeo.
"""
from __future__ import annotations
import argparse, hashlib, struct
from pathlib import Path
from dataclasses import dataclass

NAME_MAX=100
STRUCT_SIZE=112
@dataclass(frozen=True)
class Descriptor:
    modern: str
    legacy: str
    size_modern: int
    size_legacy: int
    crc_modern: int
    crc_legacy: int

def mapping() -> list[Descriptor]:
    rows=[
      # 3 mandatory modern Neo Geo BIOS chips replaced ONLY in legacy core.
      ("sm1.sm1","sm1.sm1",131072,131072,0x94416d67,0x97cf998b),
      ("sfix.sfix","sfix.sfx",131072,131072,0xc2ea0cfd,0x354029fc),
      ("000-lo.lo","000-lo.lo",131072,65536,0x5a86cff2,0xe09e253c),
      # Legacy MVS BIOS filenames (same chip CRC, different name).
      ("sp-s3.sp1","asia-s3.rom",131072,131072,0x91b64be3,0x91b64be3),
      ("sp-u2.sp1","usa_2slt.bin",131072,131072,0xe72943de,0xe72943de),
      ("sp-j2.sp1","sp-j2.rom",131072,131072,0xacede59c,0xacede59c),
      # Actual 13 KOF2002 game chips -- CRCs same in MAME125 and PS3 FBNeo.
      ("265-p1.p1","265-p1.bin",1048576,1048576,0x9ede7323,0x9ede7323),
      ("265-p2.sp2","265-p2.bin",4194304,4194304,0x327266b8,0x327266b8),
      ("265-m1.m1","265-m1.bin",131072,131072,0x85aaa632,0x85aaa632),
      ("265-v1.v1","265-v1.bin",8388608,8388608,0x15e8f3f5,0x15e8f3f5),
      ("265-v2.v2","265-v2.bin",8388608,8388608,0xda41d6f9,0xda41d6f9),
    ]
    for n,crc in enumerate((0x2b65a656,0xadf18983,0x875e9fd7,
                            0x2da13947,0x61bd165d,0x03fdd1eb,
                            0x1a2749d8,0xab0bb549),1):
        rows.append((f"265-c{n}.c{n}",f"265-c{n}.bin",
                     8388608,8388608,crc,crc))
    assert len(rows)==19
    return [Descriptor(*x) for x in rows]

def load_aliases(path:Path)->list[Descriptor]:
    """Only permit exact name+CRC+size aliases derived from the frozen
    ORIGINAL PS3 FBNeo 2021 source and authentic MAME 0.125 driver."""
    rows=[]
    seen={}
    for ln in path.read_text(encoding="ascii").splitlines():
        if not ln or ln.startswith("#"):continue
        p=ln.split("\t")
        assert len(p)==4,ln
        modern,crc,size,legacy=p
        crc=int(crc,16);size=int(size)
        assert size>0 and len(modern)<NAME_MAX and len(legacy)<NAME_MAX
        key=(modern,crc,size)
        assert key not in seen or seen[key]==legacy,key
        seen[key]=legacy
    for (modern,crc,size),legacy in sorted(seen.items()):
        rows.append(Descriptor(modern,legacy,size,size,crc,crc))
    assert len(rows)>=700,len(rows)
    return rows

def transform(raw: bytes, verbose:bool=True, aliases_path:Path|None=None)->tuple[bytes,dict[str,int]]:
    assert raw.startswith(b"\x7fELF"),"Input must be decrypted ELF, NOT SELF"
    out=bytearray(raw)
    applied={}
    rows=mapping()
    if aliases_path is not None:
        old={(x.modern,x.crc_modern,x.size_modern):x for x in rows}
        for x in load_aliases(aliases_path):
            key=(x.modern,x.crc_modern,x.size_modern)
            if key in old:
                assert old[key].legacy==x.legacy,key
            else:
                old[key]=x;rows.append(x)
        print("FBNEO_LEGACY_ALIASES",len(rows),"unique descriptors expected")
    for row in rows:
        name=row.modern.encode("ascii")
        target=row.legacy.encode("ascii")
        assert len(target)<NAME_MAX and len(name)<NAME_MAX
        # Scan exact NUL-terminated descriptor with 100-byte name array;
        # all 4 numeric fields are big-endian on PS3 PPU.
        needle=name+b"\0"
        pos=-1
        count=0
        while True:
            pos=raw.find(needle,pos+1)
            if pos<0:break
            if pos+STRUCT_SIZE>len(raw):continue
            if raw[pos+len(needle):pos+NAME_MAX].strip(b"\0"):continue
            length,crc,kind=struct.unpack_from(">III",raw,pos+NAME_MAX)
            if length!=row.size_modern or crc!=row.crc_modern:continue
            # The original core uses very large ROM tables and the same
            # descriptor may occur multiple times (clones/variants).
            # Patching only the exact struct is safer than CRC byte replace.
            assert out[pos:pos+len(name)+1]==needle
            out[pos:pos+NAME_MAX]=target+b"\0"*(NAME_MAX-len(target))
            struct.pack_into(">II",out,pos+NAME_MAX,row.size_legacy,row.crc_legacy)
            assert struct.unpack_from(">I",out,pos+NAME_MAX+8)[0]==kind
            count+=1
        if count==0:
            raise AssertionError(f"ELF incompatible; expected descriptor absent: {row.modern} CRC {row.crc_modern:08x}")
        applied[f"{row.modern}@{row.crc_modern:08x}"]=count
        if verbose:print(f"PATCH_LEGACY_NEOGEO {row.modern}->{row.legacy} descriptor_count={count} oldCRC={row.crc_legacy:08x}")
    assert len(out)==len(raw)
    # Make sure only descriptor payloads changed -- no PPC code, functions,
    # unrelated PS1/PS2/PS3/SNES/BDISO assets, or base FBNeo core changed.
    if verbose:
        print("PASS isolated FBNeo legacy table-only patch",sum(applied.values()),
              "descriptor entries; ELF bytes",len(raw),
              "SHA256",hashlib.sha256(out).hexdigest())
    return bytes(out),applied

def main():
    p=argparse.ArgumentParser()
    p.add_argument("source")
    p.add_argument("dest")
    p.add_argument("--aliases",type=Path,default=None)
    a=p.parse_args()
    src=Path(a.source).read_bytes()
    target,stats=transform(src,aliases_path=a.aliases)
    assert stats["sm1.sm1@94416d67"]>=1 and stats["sfix.sfix@c2ea0cfd"]>=1 and stats["000-lo.lo@5a86cff2"]>=1
    assert stats["265-p1.p1@9ede7323"]>=1 and stats["265-c1.c1@2b65a656"]>=1
    Path(a.dest).write_bytes(target)
if __name__=="__main__":
    main()
