#!/usr/bin/env python3
"""Generate trusted arcade parent/clone dependencies for the PS3 launcher.

Use frozen 2021 PS3 FBNeo DAT (not a modern broad title list), original MAME
0.125 Neo Geo chip CRCs and the independently verified FBNeo Legacy parents.
Both outputs contain only metadata; no game or BIOS data is extracted.

parents.tsv: all exact FBNeo clone->parent mappings.
legacy_clones.tsv: the subset whose parent is verified for PS3 FBNeo Legacy
and whose *unique* MAME clone ROMs match SAME NAME/CRC/SIZE in the frozen
2021 FBNeo driver. A launcher must still check actual game+parent+BIOS ZIPs.
"""
import argparse
from collections import defaultdict
from pathlib import Path
import re

SHORT = re.compile(r"^[a-z0-9_.+-]{1,90}$")
def generate(fbneo:Path, mame:Path, legacy_games:Path):
    parents = {}
    modern = defaultdict(set)
    old = defaultdict(set)
    for raw in fbneo.read_text(encoding="utf-8").splitlines():
        row=raw.split("\t")
        if len(row)!=6 or raw.startswith("#"):continue
        code,parent,bios,name,crc,size=row
        if not SHORT.fullmatch(code):continue
        if name=="@":
            if parent and SHORT.fullmatch(parent) and parent!=code:
                if code in parents and parents[code]!=parent:
                    raise ValueError(f"ambiguous parent for {code}")
                parents[code]=parent
            continue
        try: modern[code].add((name,int(crc,16),int(size)))
        except ValueError:pass
    for raw in mame.read_text(encoding="utf-8").splitlines():
        row=raw.split("\t")
        if len(row)!=4 or row[1]=="@" or raw.startswith("#"):continue
        try:old[row[0]].add((row[3],int(row[1],16),int(row[2])))
        except ValueError:pass
    accepted=set()
    for ln in legacy_games.read_text(encoding="utf-8").splitlines():
        if ln and not ln.startswith("#"):
            accepted.add(ln.split("\t")[0])
    verified={}
    for clone,parent in sorted(parents.items()):
        if parent not in accepted or clone not in old or parent not in old:
            continue
        pchips={(crc,size) for _,crc,size in old[parent]}
        unique={(name,crc,size) for name,crc,size in old[clone]
                if (crc,size) not in pchips}
        if not unique:continue
        # Unlike CRC-based aliases for the parent, clone-specific filenames
        # must be byte-identical to the names expected by the PS3 FBNeo core.
        if not unique.issubset(modern[clone]):continue
        verified[clone]=(parent,len(unique),len(old[clone]))
    assert parents.get("kf2k2mp")=="kof2002"
    assert "kf2k2mp" in verified and verified["kf2k2mp"][1:]==(3,14)
    assert len(verified)>=5,(len(verified),verified)
    return parents,verified

def main():
    p=argparse.ArgumentParser()
    for x in ("fbneo","mame","legacy_games","parents","clones"):
        p.add_argument("--"+x.replace("_","-"),required=True,type=Path)
    a=p.parse_args()
    parents,clones=generate(a.fbneo,a.mame,a.legacy_games)
    a.parents.parent.mkdir(parents=True,exist_ok=True)
    a.clones.parent.mkdir(parents=True,exist_ok=True)
    a.parents.write_text("# Frozen PS3 FBNeo: clone<TAB>parent\n"+
        "".join(f"{k}\t{v}\n" for k,v in sorted(parents.items())),
        encoding="ascii")
    a.clones.write_text("# MAME0.125 plus PS3 FBNeo verified: clone<TAB>parent<TAB>unique<TAB>total\n"+
        "".join(f"{k}\t{v[0]}\t{v[1]}\t{v[2]}\n"
                for k,v in sorted(clones.items())),
        encoding="ascii")
    print("PASS frozen PS3 parent map",len(parents),"clones; verified legacy clone set",len(clones))
    print("PASS kf2k2mp=>kof2002 3 unique chip CRCs + 11 inherited MAME entries")

if __name__=="__main__":
    main()
