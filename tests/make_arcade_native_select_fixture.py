#!/usr/bin/env python3
"""Generate metadata-only ZIP fixtures from real MAME 0.125 driver. NO ROM DATA.
Test proves router checks canonical chip CRC+size from original MAME source.
These intentionally invalid ZIPs cannot and must not run a game.
"""
from pathlib import Path
import struct,zipfile
root=Path("/tmp/arcade_native_route_test")
root.mkdir(parents=True,exist_ok=True)
mapping=root/"mame0125_neogeo.tsv"
assert mapping.is_file()
rows=[ln.split("\t") for ln in mapping.read_text().splitlines()
      if ln.startswith("kof2002\t") and "\t@\t" not in ln]
assert len(rows)>=13,len(rows)
entries=[(n,int(crc,16),int(size)) for _,crc,size,n in rows]
assert any(n=="265-m1_decrypted.bin" for n,_,_ in entries)
game=[x for x in entries if x[0] not in ("sfix.sfx","sm1.sm1","000-lo.lo")]
legacy=[("sm1.sm1",0x97cf998b,131072),
        ("sfix.sfx",0x354029fc,131072),
        ("000-lo.lo",0xe09e253c,65536)]
modern=[("sm1.sm1",0x94416d67,131072),
        ("sfix.sfix",0xc2ea0cfd,131072),
        ("000-lo.lo",0x5a86cff2,131072)]
def central_only_zip(path,items):
    # Metadata-only fake test archive: do not confuse it with a real ROM!
    with zipfile.ZipFile(path,"w") as z:
        for name,crc,size in items:
            z.writestr(name,b"")
    buf=bytearray(path.read_bytes())
    pos=0
    count=0
    wanted={name:(crc,size) for name,crc,size in items}
    while (pos:=buf.find(b"PK\x01\x02",pos))>=0:
        namelen=struct.unpack_from("<H",buf,pos+28)[0]
        name=buf[pos+46:pos+46+namelen].decode()
        crc,size=wanted[name]
        struct.pack_into("<I",buf,pos+16,crc)
        struct.pack_into("<I",buf,pos+24,size)
        count+=1
        pos+=46+namelen
    assert count==len(items)
    path.write_bytes(buf)
central_only_zip(root/"kof2002.zip",game)
central_only_zip(root/"neogeo.zip",legacy)
central_only_zip(root/"neogeo_modern.zip",modern)
central_only_zip(root/"kof2002_missing.zip",game[:-1])
central_only_zip(root/"kof2002_wrong.zip",[(n,crc^1 if i==0 else crc,s) for i,(n,crc,s) in enumerate(game)])
print("PASS metadata-only test fixtures KOF",len(game),"old BIOS 3; NOT runnable game ROMs")
