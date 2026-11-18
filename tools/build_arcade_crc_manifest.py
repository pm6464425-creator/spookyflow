#!/usr/bin/env python3
"""Freeze the EXACT FBNeo fccba95f2 ROM manifest corresponding to
FBNeoRLPlus v2.30 native PS3 core. Report-only: does not modify any ZIP.
Format: code<TAB>parent<TAB>bios<TAB>member<TAB>CRC32<TAB>unpacked_size
Only core-specific files are used; latest 2026 FBNeo DAT is NOT compatible.
"""
import argparse
import io
import re
import urllib.parse
import urllib.request
import xml.etree.ElementTree as ET
from pathlib import Path

REF="fccba95f2"
DAT="FinalBurn Neo (ClrMame Pro XML, Arcade only).dat"
URL=("https://raw.githubusercontent.com/libretro/FBNeo/"+REF+"/dats/"+
     urllib.parse.quote(DAT))
HEADERS={"User-Agent":"SpookyFlow-PS3-Arcade-Diagnostic/1.0"}
def main():
    p=argparse.ArgumentParser()
    p.add_argument("--output",required=True)
    args=p.parse_args()
    req=urllib.request.Request(URL,headers=HEADERS)
    with urllib.request.urlopen(req,timeout=120) as r:
        data=r.read(16*1024*1024)
    if len(data)<100000 or len(data)>14*1024*1024:
        raise RuntimeError("Wrong frozen FBNeo DAT size")
    out=[]
    games=0
    for _,node in ET.iterparse(io.BytesIO(data),events=("end",)):
        if node.tag not in ("game","machine"):continue
        code=(node.attrib.get("name") or "").lower()
        parent=(node.attrib.get("cloneof") or "").lower()
        bios=(node.attrib.get("romof") or "").lower()
        if not re.fullmatch(r"[a-z0-9_.+-]{1,90}",code):
            node.clear();continue
        if parent and not re.fullmatch(r"[a-z0-9_.+-]{1,90}",parent):parent=""
        if bios and not re.fullmatch(r"[a-z0-9_.+-]{1,90}",bios):bios=""
        games+=1
        # H header always emitted, even if it contains no ROM entries.
        out.append(f"{code}\t{parent}\t{bios}\t@\t-\t-\n")
        for rom in node.findall("rom"):
            name=(rom.attrib.get("name") or "")
            crc=(rom.attrib.get("crc") or "").lower()
            size=(rom.attrib.get("size") or "")
            status=rom.attrib.get("status","")
            # No confidence in nodump chips. Avoid presenting these as
            # missing; emulator can legitimately run without some.
            if status=="nodump" or not re.fullmatch(r"[0-9a-f]{8}",crc):continue
            if not size.isdecimal() or len(name)>125 or any(c in name for c in "\r\n\t"):continue
            out.append(f"{code}\t{parent}\t{bios}\t{name}\t{crc}\t{size}\n")
        node.clear()
    if games<4000 or len(out)<30000:
        raise RuntimeError(f"Incomplete FBNeo DAT: games={games} records={len(out)}")
    path=Path(args.output)
    path.parent.mkdir(parents=True,exist_ok=True)
    path.write_text("# FROZEN FBNeo fccba95f2 arcade DAT, not modern MAME; H header '@'\n"+
                    "".join(sorted(out,key=lambda v:v.split("\t",1)[0])),
                    encoding="utf8",newline="\n")
    print("PASS: old PS3 FBNeo fccba95f2 CRC audit manifest games",games,
          "rows",len(out),"bytes",path.stat().st_size)
if __name__=="__main__":main()
