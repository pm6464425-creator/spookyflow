#!/usr/bin/env python3
"""Inspect upstream public PS3 debug PKG and extract only two arcade cores.
No ROM files or BIOS will be bundled; executable licensing to be reviewed.
"""
import hashlib,struct,sys,json,os
from pathlib import Path
from dataclasses import dataclass
def key16(k,n):
    return hashlib.sha1(k[:8]*2+k[8:]*2+bytes(24)+struct.pack(">Q",n)).digest()[:16]
def decrypt_chunk(data,qa,offset):
    out=bytearray(len(data))
    for i in range(0,len(data),16):
        n=(offset+i)//16
        key=key16(qa,n)
        for j in range(min(16,len(data)-i)):out[i+j]=data[i+j]^key[j]
    return bytes(out)
def main():
    name=sys.argv[1]
    dest=Path(sys.argv[2]);dest.mkdir(parents=True,exist_ok=True)
    with open(name,"rb") as f:
        h=f.read(0x1A0)
        assert h[:4]==b'\x7fPKG' and int.from_bytes(h[4:8],'big')==1,(h[:16].hex())
        count=int.from_bytes(h[0x14:0x18],'big')
        dataoff=int.from_bytes(h[0x20:0x28],'big')
        datasize=int.from_bytes(h[0x28:0x30],'big')
        qa=h[0x60:0x70]
        assert 20<count<20000 and 0x140<=dataoff<0x1000 and datasize<650_000_000
        print("PKG count",count,"payloadsize",datasize,"offset",dataoff,flush=True)
        f.seek(dataoff)
        table=decrypt_chunk(f.read(32*count),qa,0)
        rows=[]
        for i in range(count):
            fn,flen,off,sz,flags,pad=struct.unpack_from(">IIQQII",table,i*32)
            if flen<1 or flen>4096:raise RuntimeError(f"bad fname len {flen}")
            rows.append((fn,flen,off,sz,flags))
        max_name_off=max(fn+flen for fn,flen,off,sz,flags in rows)
        f.seek(dataoff)
        names=decrypt_chunk(f.read((max_name_off+15)&~15),qa,0)
        manifest=[]
        for fn,flen,off,sz,flags in rows:
            path=names[fn:fn+flen].decode("utf8","replace")
            manifest.append(dict(path=path,size=sz,flags=flags))
            if (".self" in path.lower() or ".bin" in path.lower() or "retroarch.cfg" in path.lower() or "gamelist" in path.lower() or "game" in path.lower() or "license" in path.lower()):
                print("POTENTIAL",path,sz,flags,flush=True)
            if (flags&255)==4 or sz>52*1024*1024:continue
            stem=path.lower()
            core=(
              stem.endswith("/cores/fb_neo.self")
              or stem.endswith("/mame125/mame125.self")
              or ("/mame125/" in stem and stem.endswith(".self"))
              or stem.endswith("/reload.self")
              or stem.endswith("/retroarch.cfg")
              or stem.endswith("/db/games.db")
              or stem.endswith("/games.db")
              or stem.endswith("/retro.db")
              or stem.endswith("/mame125/init.ini")
              or stem.endswith("/mame125/cfg/default.cfg")
              or stem.endswith("/mame125/fonts/ui000-11.bdc")
              or stem.endswith("/cores/autoconfig/sixaxis controller.cfg")
            )
            if not core:continue
            f.seek(dataoff+off)
            content=decrypt_chunk(f.read((sz+15)&~15),qa,off)
            content=content[:sz]
            target=dest/path
            target.parent.mkdir(parents=True,exist_ok=True)
            target.write_bytes(content)
            print("EXTRACTED",path,len(content),hashlib.sha256(content).hexdigest(),flush=True)
    (dest/"manifest.json").write_text(json.dumps(manifest,indent=1))
    print("MANIFEST",len(manifest),"entries, extraction done")
if __name__=="__main__":main()
