#!/usr/bin/env python3
"""Generate legacy NeoGeo alias metadata from TWO authentic PS3-era catalogs.

Use the original MAME 0.125 neodrvr.c-derived TSV and the exact 2021
FBNeo-PS3 core's frozen manifest. Games qualify ONLY if every actual MAME
game chip has a matching CRC+size in the PS3 FBNeo core's same game driver.

Output 1: FBNeo descriptor name/CRC/size -> MAME-era descriptor name TSV.
Output 2: strict list of fully matching game shortnames (NOT a guarantee
that every variant can actually boot; hardware test still required).
No copyrighted ROM/BIOS files are downloaded or created.
"""
from __future__ import annotations
from collections import defaultdict
from pathlib import Path
import argparse

OLD_BIOS_NAMES = {"sfix.sfx", "sm1.sm1", "000-lo.lo"}

def collect(mame:Path,fbneo:Path):
    old=defaultdict(list)
    modern=defaultdict(list)
    for ln in mame.read_text(encoding="utf-8").splitlines():
        p=ln.split("\t")
        if len(p)!=4 or p[1]=="@" or ln.startswith("#"):continue
        try:
            crc=int(p[1],16); size=int(p[2]); name=p[3]
        except ValueError:continue
        if name in OLD_BIOS_NAMES or name.endswith("_decrypted.bin"):continue
        if not size or not name:continue
        old[p[0]].append((name,crc,size))
    for ln in fbneo.read_text(encoding="utf-8").splitlines():
        p=ln.split("\t")
        if len(p)!=6 or p[3]=="@" or ln.startswith("#") or p[2]!="neogeo":continue
        try:
            crc=int(p[4],16); size=int(p[5]); name=p[3]
        except ValueError:continue
        if name and size:modern[p[0]].append((name,crc,size))
    eligible={}
    aliases={}
    for code in sorted(old.keys() & modern.keys()):
        indexed=defaultdict(set)
        for name,crc,size in modern[code]:indexed[(crc,size)].add(name)
        items=old[code]
        if len(items)<4 or any((crc,size) not in indexed for _,crc,size in items):
            continue
        local={}
        for old_name,crc,size in items:
            possible=indexed[(crc,size)]
            if len(possible)!=1:
                break
            current=next(iter(possible))
            key=(current,crc,size)
            if key in local and local[key]!=old_name:
                break
            local[key]=old_name
        else:
            if any(key in aliases and aliases[key]!=val for key,val in local.items()):
                continue
            eligible[code]=len(items)
            aliases.update(local)
    assert "kof2002" in eligible and "kof97" in eligible and "kof98" in eligible
    assert "neobombe" in eligible and "mslug3" in eligible
    assert len(eligible)>=65 and len(aliases)>=700,(len(eligible),len(aliases))
    return eligible,aliases

def main():
    p=argparse.ArgumentParser()
    p.add_argument("--mame",required=True,type=Path)
    p.add_argument("--fbneo",required=True,type=Path)
    p.add_argument("--aliases",required=True,type=Path)
    p.add_argument("--games",required=True,type=Path)
    a=p.parse_args()
    eligible,aliases=collect(a.mame,a.fbneo)
    a.aliases.parent.mkdir(parents=True,exist_ok=True)
    a.games.parent.mkdir(parents=True,exist_ok=True)
    a.aliases.write_text("# name-modern<TAB>crc<TAB>size<TAB>name-mame0125\n"+
        "".join(f"{name}\t{crc:08x}\t{size}\t{aliases[name,crc,size]}\n"
                for name,crc,size in sorted(aliases)),
        encoding="ascii")
    a.games.write_text("# Verified 2021 FBNeo PS3 NeoGeo/MAME0.125 chip CRC overlap; not hardware-tested\n"+
        "".join(f"{code}\t{eligible[code]}\n" for code in sorted(eligible)),
        encoding="ascii")
    print("PASS PS3-era legacy NeoGeo overlap",
          len(eligible),"complete game sets;",len(aliases),"unique descriptors")
if __name__=="__main__":main()
