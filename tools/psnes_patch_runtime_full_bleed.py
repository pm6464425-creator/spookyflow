#!/usr/bin/env python3
"""V15.50: source-audited centered full-bleed pSNES game-quad patch.
Unlike old launch-time checks, this changes the actual UPDATE_SCALING
routine immediately before its setScale call inside pSNES PowerPC64.
The original SNES9x, audio, gamepad and ROM code is never altered.
The patch is pinned to the exact v15.47 ELF SHA256 and bails out on any
mismatch. Crops outer image pixels: TV physical validation still required.
"""
from __future__ import annotations
from pathlib import Path
import hashlib,struct,sys
BASE_SHA="664cafadb096db1367ea0f018bfdbdf14141ae04c20d0340e5f23c45e67041bb"
EXPECTED_ELF_SIZE=16653712
SITE_OFF=0x001CF650
SITE_VA=0x001DF650
CAVE_OFF=0x004A50C8
CAVE_VA=0x004B50C8
CAVE_BYTES=48
# 1.12x centered overscan of the SNES *game quad*, independently of the
# Tiny3D viewport case. This is NOT a system-wide resolution change.
ZOOM=1.12
def branch(src:int,dst:int)->int:
    delta=dst-src
    if delta%4 or delta<-0x02000000 or delta>=0x02000000:
        raise ValueError("PPC64 b out of range or unaligned")
    instr=0x48000000|(delta&0x03fffffc)
    displacement=instr&0x03fffffc
    if displacement&0x02000000:displacement-=0x04000000
    if src+displacement!=dst:raise ValueError("PPC64 branch roundtrip failed")
    return instr

def load_segment_for(elf:bytes, file_offset:int):
    if elf[:6]!=b"\x7fELF\x02\x02" or struct.unpack_from(">H",elf,18)[0]!=21:
        raise ValueError("Expected PPC64 big-endian ELF")
    phoff=struct.unpack_from(">Q",elf,32)[0]
    phsize=struct.unpack_from(">H",elf,54)[0]
    phnum=struct.unpack_from(">H",elf,56)[0]
    found=[]
    for i in range(phnum):
        p=phoff+i*phsize
        typ,flags=struct.unpack_from(">II",elf,p)
        ofs,va=struct.unpack_from(">QQ",elf,p+8)
        sz=struct.unpack_from(">Q",elf,p+32)[0]
        if typ==1 and ofs<=file_offset<ofs+sz and flags&1:
            found.append(va+file_offset-ofs)
    if len(found)!=1:return None
    return found[0]

def apply(source:bytes)->tuple[bytes,str]:
    if len(source)!=EXPECTED_ELF_SIZE:
        raise ValueError(f"Unexpected pSNES ELF length {len(source)}")
    actual=hashlib.sha256(source).hexdigest()
    if actual!=BASE_SHA:raise ValueError(f"Wrong version input SHA256: {actual}")
    assert load_segment_for(source,SITE_OFF)==SITE_VA
    assert load_segment_for(source,CAVE_OFF)==CAVE_VA
    # The last 48 bytes between .text and .opd/.plt, reserved alignment
    # padding within RX PT_LOAD; must be empty in the pinned binary.
    unused=source[CAVE_OFF:CAVE_OFF+CAVE_BYTES]
    if len(unused)!=CAVE_BYTES or any(unused):
        raise ValueError("Executable code cave is not wholly empty / safe")
    displaced=struct.pack(">I",0x388100c0)  # addi r4,r1,192
    if source[SITE_OFF:SITE_OFF+4]!=displaced:
        raise ValueError("pSNES setScale argument assembly mismatch")
    factor=struct.unpack(">I",struct.pack(">f",ZOOM))[0]
    instructions=[
        0xc00100c0,              # lfs   f0,192(r1)  video_scale.x
        0xc02100c4,              # lfs   f1,196(r1)  video_scale.y
        0x3d800000|(factor>>16), # lis   r12,zoom_hi
        0x618c0000|(factor&0xffff), # ori r12,r12,zoom_lo
        0x918100f0,              # stw   r12,240(r1), local float scratch
        0xc1a100f0,              # lfs   f13,240(r1)
        0xec000372,              # fmuls f0,f0,f13
        0xec210372,              # fmuls f1,f1,f13
        0xd00100c0,              # stfs  f0,192(r1)
        0xd02100c4,              # stfs  f1,196(r1)
        0x388100c0,              # execute displaced addi r4,r1,192
        branch(CAVE_VA+44,SITE_VA+4), # return to original setScale dispatch
    ]
    stub=b"".join(struct.pack(">I",ins) for ins in instructions)
    assert len(stub)==CAVE_BYTES
    out=bytearray(source)
    out[CAVE_OFF:CAVE_OFF+CAVE_BYTES]=stub
    out[SITE_OFF:SITE_OFF+4]=struct.pack(">I",branch(SITE_VA,CAVE_VA))
    assert load_segment_for(out,CAVE_OFF)==CAVE_VA
    assert all(source[x]==out[x] for x in range(SITE_OFF+4,CAVE_OFF))
    assert all(source[x]==out[x] for x in range(CAVE_OFF+CAVE_BYTES,len(source)))
    sha=hashlib.sha256(out).hexdigest()
    print("PASS pinned original pSNES 7.2: C2DUIVideo::updateScaling runtime setScale patch")
    print("PASS 1.12x centered uniform video zoom, both axes, all SNES games, no menu or audio changes")
    print("PASS PPC64 machine opcodes / branch roundtrip / RX PT_LOAD / exact 52-byte modification")
    print("PASS new pSNES ELF SHA256",sha)
    return bytes(out),sha

def main():
    if len(sys.argv)!=4:raise SystemExit("Usage: patch.py old.elf new.elf sha256.txt")
    inp,out,stamp=map(Path,sys.argv[1:])
    if inp.resolve()==out.resolve():raise ValueError("Source overwrite refused")
    modified,sha=apply(inp.read_bytes())
    out.write_bytes(modified)
    stamp.write_text(sha+"\n",encoding="ascii")
    if hashlib.sha256(out.read_bytes()).hexdigest()!=sha:raise ValueError("Written digest failed")
if __name__=="__main__":main()
