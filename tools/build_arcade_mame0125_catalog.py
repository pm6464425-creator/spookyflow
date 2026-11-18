#!/usr/bin/env python3
"""Pin original MAME 0.125 NeoGeo driver ROM expectations.
Source: mamedev/mame tag mame0125 (commit 692a5e467470ca26ff06391899aa8f5eecbf5d1b).
Unlike the modern FBNeo list, this exact 2008 driver accepts old NeoGeo
BIOS CRCs 97cf998b/354029fc/e09e253c and old kof2002 filenames.
Map is used ONLY to select original MAME125 core for actual matching ZIPs;
no ROMs or copyrighted BIOS bytes are distributed.
"""
import argparse,re,urllib.request
from pathlib import Path
URL="https://raw.githubusercontent.com/mamedev/mame/692a5e467470ca26ff06391899aa8f5eecbf5d1b/src/mame/drivers/neodrvr.c"
RGAME=re.compile(r"(?m)^\s*ROM_START\s*\(\s*([A-Za-z0-9_]+)\s*\)(.*?)^\s*ROM_END",re.S|re.M)
RROM=re.compile(r'ROM_LOAD[A-Z0-9_]*\s*\(\s*"([^"]+)"\s*,\s*(?:0x[0-9a-fA-F]+|\d+)\s*,\s*(0x[0-9a-fA-F]+|\d+)\s*,\s*CRC\(([0-9a-fA-F]{8})\)',re.I)
def main():
    ap=argparse.ArgumentParser()
    ap.add_argument("--output",required=True)
    opts=ap.parse_args()
    req=urllib.request.Request(URL,headers={"User-Agent":"SpookyFlow-MAME0125-compat-catalog/1.0"})
    with urllib.request.urlopen(req,timeout=100) as resp:
        src=resp.read(1_700_000).decode("latin1")
    assert 'ROM_START( kof2002 )' in src
    assert 'CRC(97cf998b)' in src
    assert 'CRC(354029fc)' in src
    assert 'CRC(e09e253c)' in src
    sets={}
    for name,body in RGAME.findall(src):
        roms=[]
        for filename,size,crc in RROM.findall(body):
            value=int(size,0)
            if not 1<=value<=64*1024*1024:continue
            roms.append((crc.lower(),value,filename))
        if len(roms)>=2:sets[name.lower()]=roms
    assert 150<len(sets)<1000,len(sets)
    required={
      "265-p1.bin":("9ede7323",1048576),
      "265-p2.bin":("327266b8",4194304),
      "265-m1_decrypted.bin":("1c661a4b",131072),
      "265-c1.bin":("2b65a656",8388608),
    }
    got={fn:(crc,sz) for crc,sz,fn in sets["kof2002"]}
    assert all(got.get(fn)==pair for fn,pair in required.items()),got
    lines=["# frozen original MAME0125 NeoGeo source ROM_START shortname CRC size name\n"]
    for name in sorted(sets):
        lines.append(f"{name}\t@\t0\t-\n")
        for crc,sz,fn in sets[name]:
            if any(c in fn for c in "\r\n\t"):continue
            lines.append(f"{name}\t{crc}\t{sz}\t{fn}\n")
    path=Path(opts.output)
    path.parent.mkdir(parents=True,exist_ok=True)
    path.write_text("".join(lines),encoding="utf8")
    print("PASS MAME0125 exact NeoGeo driver",len(sets),"sets",
          len(lines)-1,"entries",path.stat().st_size,"bytes")
    print("PASS KOF2002 source has",len(got),"mandatory member definitions")
if __name__=="__main__":main()
