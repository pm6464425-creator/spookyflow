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
import json
from html.parser import HTMLParser
from html import unescape
from urllib.parse import unquote,urlparse
from urllib.request import Request,urlopen
from pathlib import Path

URL="https://thumbnails.libretro.com/Sony%20-%20PlayStation/Named_Boxarts/"
GITHUB_TREE_URL="https://api.github.com/repos/libretro-thumbnails/Sony_-_PlayStation/git/trees/master?recursive=1"
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

def github_names() -> list[str]:
    """Official Libretro repository fallback when the CDN directory returns 404."""
    req=Request(GITHUB_TREE_URL,headers={
        "User-Agent":"MI-PS3-PS1-boxart-index/1.0",
        "Accept":"application/vnd.github+json"
    })
    with urlopen(req,timeout=60) as response:
        doc=json.load(response)
    assert doc.get("truncated") is False, "GitHub tree truncated: refuse incomplete catalog"
    prefix="Named_Boxarts/"
    names=[]
    for item in doc.get("tree",[]):
        path=item.get("path","")
        if item.get("type")!="blob" or not path.startswith(prefix):
            continue
        name=path[len(prefix):]
        if "/" in name or "\\" in name or not name.lower().endswith(".png"):
            continue
        if len(name.encode("utf-8"))>220 or any(x in name for x in ("\n","\r","\t")):
            continue
        names.append(name)
    result=sorted(set(names),key=str.casefold)
    print(f"PS1 official GitHub thumbnail tree: {len(result)} verified boxart names")
    return result

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
        names=parse(args.fixture.read_bytes())
    else:
        try:
            request=Request(URL,headers={"User-Agent":"SpookyFlow-PS1-boxart-index/1.0"})
            with urlopen(request,timeout=35) as r:
                names=parse(r.read(4*1024*1024))
            if len(names)<args.min_count:
                raise ValueError("CDN listing incomplete; use official repository fallback")
        except Exception as exc:
            print(f"PS1 CDN directory unavailable ({exc}); trying official Libretro GitHub tree")
            names=github_names()
    write(names,args.output,args.min_count)

if __name__=="__main__":main()
