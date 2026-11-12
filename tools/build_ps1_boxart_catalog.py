#!/usr/bin/env python3
"""Build a small, *real* PlayStation cover-name catalog from Libretro.

Do not synthesize filenames from arbitrary CUE names.  The live Libretro
thumbnails listing contains actual image filenames including edition/region.
Bundle those filenames (NOT the copyrighted images) as UTF-8 text in the PKG,
then query verified image URLs by the best name/region match on the console.

Source: https://thumbnails.libretro.com/Sony%20-%20PlayStation/Named_Boxarts/
Only public image names are indexed; no ROM files, accounts, or private data.
"""
from __future__ import annotations
import argparse
from html.parser import HTMLParser
from html import unescape
from urllib.parse import unquote,urlparse
from urllib.request import Request,urlopen
from pathlib import Path

URL="https://thumbnails.libretro.com/Sony%20-%20PlayStation/Named_Boxarts/"
OUTPUT=Path("pkgfiles/USRDIR/ps1_boxart_names.txt")

class CoverNames(HTMLParser):
    def __init__(self):
        super().__init__(convert_charrefs=True)
        self.names=set()
    def handle_starttag(self,tag,attrs):
        if tag!="a": return
        target=dict(attrs).get("href","")
        if not target.lower().endswith(".png"): return
        name=unescape(unquote(target.split("/")[-1]))
        if not name or name.startswith(".") or "/" in name or "\\" in name: return
        if "\n" in name or "\r" in name or "\t" in name: return
        if len(name.encode("utf-8"))>220: return
        self.names.add(name)

def parse(html:bytes)->list[str]:
    p=CoverNames()
    p.feed(html.decode("utf-8","replace"))
    return sorted(p.names,key=str.casefold)

def write(names:list[str],path:Path,min_count:int)->None:
    assert len(names)>=min_count, f"Only {len(names)} boxart names; aborting partial index"
    assert "Disney Tarzan (Spain).png" in names, "Tarzan must be present in catalog"
    assert "Disney's Tarzan (USA).png" in names, "Disney's Tarzan must be present"
    path.parent.mkdir(parents=True,exist_ok=True)
    path.write_text("".join(n+"\n" for n in names),encoding="utf-8",newline="\n")
    assert path.stat().st_size<1500000, "index too large for bounded PS3 loader"
    print(f"PS1 verified Libretro names: {len(names)} entries, {path.stat().st_size} bytes -> {path}")

def main():
    ap=argparse.ArgumentParser()
    ap.add_argument("--output",type=Path,default=OUTPUT)
    ap.add_argument("--fixture",type=Path,help="Offline unit fixture, avoid network")
    ap.add_argument("--min-count",type=int,default=7000)
    args=ap.parse_args()
    if args.fixture:
        data=args.fixture.read_bytes()
    else:
        request=Request(URL,headers={"User-Agent":"SpookyFlow-PS1-boxart-index/1.0"})
        with urlopen(request,timeout=35) as r:
            data=r.read(4*1024*1024)
    write(parse(data),args.output,args.min_count)

if __name__=="__main__":main()
