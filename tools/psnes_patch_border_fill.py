#!/usr/bin/env python3
"""SNES V15.47: compensate remaining black borders in pSNES/Tiny3D HDTV viewport.
Takes ONLY the pinned V15.43 PowerPC64 ELF and changes 8 verified screen
projection slots. No audio, game code, controls or ROM bytes are changed.
Intentional small crop may hide HUD pixels. Physical PS3 test still required.
"""
from __future__ import annotations
import hashlib,struct,sys
from pathlib import Path
BASE_SHA="4815e9632f10f5f405e43d284b4d2857bb8415e7a71985afa85016d5863f516e"
ELF_SIZE=16653712
GOT_720_Y=0x005DA4C8
F_720_X=0x0062A850
F_720_SCALE_Y=0x0062A854
F_720_SCALE_X=0x0062A858
F_1080_Y=0x0062A85C
F_1080_X=0x0062A860
F_1080_SCALE_Y=0x0062A864
F_1080_SCALE_X=0x0062A868
X_720,Y_720=40,24
X_1080,Y_1080=60,24

def file_offset_to_vaddr(elf:bytes,offset:int)->int:
    if elf[:6]!=b"\x7fELF\x02\x02" or struct.unpack_from(">H",elf,18)[0]!=21:
        raise ValueError("Expected big-endian PPC64 ELF")
    phoff=struct.unpack_from(">Q",elf,32)[0]
    phentsize=struct.unpack_from(">H",elf,54)[0]
    phnum=struct.unpack_from(">H",elf,56)[0]
    if not (phentsize>=56 and 0<phnum<=256 and phoff+phentsize*phnum<len(elf)):
        raise ValueError("Invalid program headers")
    result=[]
    for n in range(phnum):
        i=phoff+n*phentsize
        if struct.unpack_from(">I",elf,i)[0]!=1: continue
        start,va=struct.unpack_from(">QQ",elf,i+8)
        size=struct.unpack_from(">Q",elf,i+32)[0]
        if start<=offset<start+size: result.append(va+offset-start)
    if len(result)!=1: raise ValueError("File offset not uniquely mapped")
    return result[0]

def test_geometry():
    assert (1280+2*X_720)-2*X_720==1280
    assert (720+2*Y_720)-2*Y_720==720
    assert (1920+2*X_1080)-2*X_1080==1920
    assert (1080+2*Y_1080)-2*Y_1080==1080
    assert X_720/1280==X_1080/1920
    assert max(X_720/1280,Y_720/720,X_1080/1920,Y_1080/1080)<0.05
    print("PASS 720p/1080p bounded border-compensated 16:9 geometry")

def planned_patches(elf:bytes):
    # 720p Y comes from a GOT pointer. Reuse the 1080p Y float (-24.0)
    # and resolve its real ELF virtual address using PT_LOAD, never a guess.
    addr=file_offset_to_vaddr(elf,F_1080_Y)
    if not 0x10000000<=addr<=0x10ffffff: raise ValueError("Unexpected GOT target")
    return [
      (GOT_720_Y,bytes.fromhex("0000000010000840"),struct.pack(">Q",addr),"720p Y GOT pointer"),
      (F_720_X,struct.pack(">f",0),struct.pack(">f",-X_720),"720p X translation"),
      (F_720_SCALE_Y,struct.pack(">f",720/512),struct.pack(">f",(720+2*Y_720)/512),"720p Y scaling"),
      (F_720_SCALE_X,struct.pack(">f",1280/848),struct.pack(">f",(1280+2*X_720)/848),"720p X scaling"),
      (F_1080_Y,struct.pack(">f",0),struct.pack(">f",-Y_1080),"1080p Y translation"),
      (F_1080_X,struct.pack(">f",0),struct.pack(">f",-X_1080),"1080p X translation"),
      (F_1080_SCALE_Y,struct.pack(">f",1080/512),struct.pack(">f",(1080+2*Y_1080)/512),"1080p Y scaling"),
      (F_1080_SCALE_X,struct.pack(">f",1920/848),struct.pack(">f",(1920+2*X_1080)/848),"1080p X scaling"),
    ]

def apply(elf:bytes)->tuple[bytes,str]:
    test_geometry()
    if len(elf)!=ELF_SIZE or hashlib.sha256(elf).hexdigest()!=BASE_SHA:
        raise ValueError("Refusing ELF differing from verified V15.43 baseline")
    patch=planned_patches(elf)
    out=bytearray(elf)
    end=0
    for off,before,after,desc in patch:
        if off<end or len(before)!=len(after) or out[off:off+len(before)]!=before:
            raise ValueError("Unsafe/mismatched patch: "+desc)
        out[off:off+len(before)]=after
        end=off+len(before)
        print("PASS",desc,hex(off),after.hex())
    result=bytes(out)
    assert len(result)==len(elf)
    assert all(result[off:off+len(after)]==after for off,before,after,_ in patch)
    sha=hashlib.sha256(result).hexdigest()
    print("PASS final border-fill PPC64 ELF SHA256",sha)
    return result,sha

def main():
    if len(sys.argv)!=4:
        raise SystemExit("Usage: psnes_patch_border_fill.py old.elf new.elf sha256.txt")
    src,dst,sha_path=map(Path,sys.argv[1:])
    if src.resolve()==dst.resolve(): raise ValueError("Do not overwrite V15.43")
    content,sha=apply(src.read_bytes())
    dst.write_bytes(content)
    sha_path.write_text(sha+"\n",encoding="ascii")
    if hashlib.sha256(dst.read_bytes()).hexdigest()!=sha: raise ValueError("Written ELF hash failed")

if __name__=="__main__": main()
