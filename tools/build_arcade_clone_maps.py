#!/usr/bin/env python3
"""Derive STRICT PS3 FBNeo legacy clones and descriptor aliases from REAL
original MAME 0.125 and frozen original PS3 FBNeo metadata.

Source fields:
 - FBNeo PS3 2021: shortname / parent / expected chip NAME, CRC and size
 - original MAME 0.125 NeoGeo driver: expected game chip CRC, size and name
 - verified parent set (generated independently by build_arcade_legacy_aliases)

Only enable a clone if:
  (1) its parent is a verified Legacy NeoGeo game;
  (2) every non-inherited MAME 0.125 chip has an *unambiguous* CRC/size
      match in the SAME clone driver on the real PS3 FBNeo core;
  (3) no descriptor collides with an existing parent or other clone mapping.
Do not modify, create, redistribute or rename ROMs or BIOS.
"""
from __future__ import annotations
import argparse
from collections import defaultdict
from pathlib import Path
import re

NAME_OK = re.compile(r"^[a-z0-9_.+-]{1,90}$")

def load_existing_aliases(path: Path) -> dict[tuple[str,int,int],str]:
    known={}
    for line in path.read_text(encoding="ascii").splitlines():
        if not line or line.startswith("#"):continue
        p=line.split("\t")
        assert len(p)==4,p
        modern,crc_txt,size_txt,old=p
        crc,size=int(crc_txt,16),int(size_txt)
        assert modern and old and len(modern)<100 and len(old)<100
        key=(modern,crc,size)
        assert key not in known or known[key]==old,(key,old)
        known[key]=old
    assert len(known)>=700,len(known)
    return known

def generate(fbneo:Path,mame:Path,legacy_games:Path,
             base_aliases:Path)->tuple[dict[str,str],dict[str,tuple[str,int,int]],
                                       dict[tuple[str,int,int],str],
                                       dict[tuple[str,int,int],str]]:
    parent_map={}
    modern=defaultdict(list)
    old=defaultdict(list)
    for raw in fbneo.read_text(encoding="utf-8").splitlines():
        p=raw.split("\t")
        if len(p)!=6 or raw.startswith("#"):continue
        code,parent,romof,name,crc_txt,size_txt=p
        if not NAME_OK.fullmatch(code):continue
        if name=="@":
            if parent and NAME_OK.fullmatch(parent) and parent!=code:
                if code in parent_map and parent_map[code]!=parent:
                    raise ValueError(f"ambiguous parent {code}")
                parent_map[code]=parent
            continue
        try:crc,size=int(crc_txt,16),int(size_txt)
        except ValueError:continue
        if name and size>0:modern[code].append((name,crc,size))
    for raw in mame.read_text(encoding="utf-8").splitlines():
        p=raw.split("\t")
        if len(p)!=4 or p[1]=="@" or raw.startswith("#"):continue
        try:crc,size=int(p[1],16),int(p[2])
        except ValueError:continue
        if p[3] and size>0:old[p[0]].append((p[3],crc,size))
    parents_ok=set()
    for line in legacy_games.read_text(encoding="ascii").splitlines():
        if line and not line.startswith("#"):parents_ok.add(line.split("\t")[0])
    aliases=load_existing_aliases(base_aliases)
    original=dict(aliases)
    clones={}
    for code,parent in sorted(parent_map.items()):
        if parent not in parents_ok or code not in old or parent not in old:
            continue
        parent_chips={(c,s) for _,c,s in old[parent]}
        unique={(n,c,s) for n,c,s in old[code] if (c,s) not in parent_chips}
        if not unique:continue
        by_crc=defaultdict(set)
        for n,c,s in modern[code]:by_crc[(c,s)].add(n)
        local={}
        for oldname,crc,size in sorted(unique):
            options=by_crc[(crc,size)]
            # A CRC has to identify exactly one descriptor in this driver.
            # MAME and FBNeo may use different filenames for same chips.
            if len(options)!=1:break
            name=next(iter(options))
            if len(oldname)>=100 or len(name)>=100:break
            key=(name,crc,size)
            if (key in aliases and aliases[key]!=oldname) or (
                key in local and local[key]!=oldname
            ):break
            local[key]=oldname
        else:
            clones[code]=(parent,len(unique),len(old[code]))
            aliases.update(local)
    new_aliases={k:v for k,v in aliases.items() if k not in original}
    assert parent_map.get("kf2k2mp")=="kof2002"
    assert clones.get("kf2k2mp")==("kof2002",3,14)
    assert len(clones)>=30,len(clones)
    assert len(new_aliases)>=100,len(new_aliases)
    return parent_map,clones,new_aliases,aliases

def put(path:Path,head:str,body:str):
    path.parent.mkdir(parents=True,exist_ok=True)
    path.write_text("# "+head+"\n"+body,encoding="ascii")

def print_aliases(d):
    return "".join(f"{name}\t{crc:08x}\t{size}\t{d[name,crc,size]}\n"
                   for name,crc,size in sorted(d))

def main():
    p=argparse.ArgumentParser()
    for arg in ("fbneo","mame","legacy_games","parents","clones",
                "base_aliases","clone_aliases","all_aliases"):
        p.add_argument("--"+arg.replace("_","-"),required=True,type=Path)
    a=p.parse_args()
    parents,clones,new,all_=generate(a.fbneo,a.mame,a.legacy_games,
                                      a.base_aliases)
    put(a.parents,"original frozen PS3 FBNeo code<TAB>parent",
        "".join(f"{c}\t{p}\n" for c,p in sorted(parents.items())))
    put(a.clones,
        "exactly matched original MAME125 + PS3 FBNeo clone<TAB>parent<TAB>unique<TAB>total",
        "".join(f"{c}\t{p}\t{u}\t{t}\n"
                for c,(p,u,t) in sorted(clones.items())))
    put(a.clone_aliases,"new clone-specific PS3 FBNeo descriptor rename metadata",
        print_aliases(new))
    put(a.all_aliases,"parent and clone descriptor aliases for isolated FBNeo Legacy",
        print_aliases(all_))
    print("PASS complete PS3 frozen parent map",len(parents))
    print("PASS original MAME125 ROM CRC+size and PS3 FBNeo clone names",
          len(clones),"verified clones; new aliases",len(new),
          "total aliases",len(all_))
    print("PASS kf2k2mp 3 clone chips + 11 inherited MAME125 game chips")

if __name__=="__main__":
    main()
