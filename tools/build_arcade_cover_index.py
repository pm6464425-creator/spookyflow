#!/usr/bin/env python3
"""Compile verified Arcade ROM -> existing Libretro flyer/boxart filenames.

Official arcade flyer/boxart sources are community-managed by Libretro:
 https://thumbnails.libretro.com/FBNeo%20-%20Arcade%20Games/Named_Boxarts/
 https://thumbnails.libretro.com/MAME/Named_Boxarts/

Never synthesizes a remote filename and never searches a different system.
The list is an index of actual HTTP directory entries, not a collection of
unverified guesses. ROM DAT names and cloneof associations come from FBNeo.
"""
import argparse
import collections
import json
import html
from html.parser import HTMLParser
import io
import re
import time
import unicodedata
import urllib.parse
import urllib.request
import xml.etree.ElementTree as ET
from pathlib import Path

FBNEO_DAT="FinalBurn Neo (ClrMame Pro XML, Arcade only).dat"
DAT_URL="https://raw.githubusercontent.com/libretro/FBNeo/master/dats/"+urllib.parse.quote(FBNEO_DAT)
SOURCE_URLS=(
 ("FBNeo","https://thumbnails.libretro.com/FBNeo%20-%20Arcade%20Games/Named_Boxarts/"),
 ("MAME","https://thumbnails.libretro.com/MAME/Named_Boxarts/"),
)
UA={"User-Agent":"SpookyFlow-Arcade-Index/1.0 (open-source)"}
GITHUB_TREES={
    "FBNeo":"https://api.github.com/repos/libretro-thumbnails/FBNeo_-_Arcade_Games/git/trees/master?recursive=1",
    "MAME":"https://api.github.com/repos/libretro-thumbnails/MAME/git/trees/master?recursive=1",
}

def retrieve(url, cap):
    req=urllib.request.Request(url,headers=UA)
    with urllib.request.urlopen(req,timeout=120) as r:
        data=r.read(cap+1)
    if len(data)>cap:raise RuntimeError("Source unexpectedly large: "+url)
    return data

class IndexParser(HTMLParser):
    def __init__(self):
        super().__init__()
        self.names=set()
    def handle_starttag(self,tag,attrs):
        if tag!="a":return
        src=dict(attrs).get("href","")
        src=html.unescape(src)
        name=urllib.parse.unquote(urllib.parse.urlsplit(src).path).rsplit("/",1)[-1]
        if name.lower().endswith(".png") and "\t" not in name and "\n" not in name:
            self.names.add(name)

def official_github_names(source):
    """Actual public Libretro boxart filenames. Reject truncated trees."""
    req=urllib.request.Request(GITHUB_TREES[source],headers={
        "User-Agent":"MI-PS3-Arcade-Index/1.0",
        "Accept":"application/vnd.github+json"
    })
    payload=None
    for attempt in range(4):
        try:
            with urllib.request.urlopen(req,timeout=120) as response:
                payload=json.load(response)
            break
        except Exception as exc:
            if attempt==3:
                raise RuntimeError(f"{source}: official catalog unavailable after 4 attempts") from exc
            time.sleep(2*(attempt+1))
    if payload.get("truncated") is not False:
        raise RuntimeError(f"{source}: official Libretro GitHub tree truncated")
    prefix="Named_Boxarts/"
    names=set()
    for entry in payload.get("tree",[]):
        p=entry.get("path","")
        if entry.get("type")!="blob" or not p.startswith(prefix):
            continue
        name=p[len(prefix):]
        if ("/" not in name and "\\" not in name and name.lower().endswith(".png")
                and not any(ch in name for ch in ("\n","\r","\t"))):
            names.add(name)
    return names

def normalized(s):
    s=unicodedata.normalize("NFKD",s).casefold()
    s="".join(c for c in s if not unicodedata.combining(c))
    return "".join(c for c in s if c.isalnum())

def base_title(s):
    s=s.strip()
    for i in range(6):
        cut=re.sub(r"\s*\([^()]{2,130}\)\s*$","",s).strip()
        if cut==s:break
        s=cut
    return s

def catalog(source_names):
    exact=collections.defaultdict(list)
    base=collections.defaultdict(list)
    for name in source_names:
        title=name[:-4].strip()
        exact[normalized(title)].append(name)
        base[normalized(base_title(title))].append(name)
    return exact,base

def select_entry(title, indexes):
    if not title:return ""
    exact,base=indexes
    k=normalized(title)
    matches=exact.get(k,())
    if not matches:
        k=normalized(base_title(title))
        if len(k)<3:return ""
        matches=base.get(k,())
    if not matches:return ""
    def rank(n):
        l=n.casefold()
        preferred=0 if "(world" in l or "(usa" in l else 1 if "(euro" in l else 2
        return (preferred,len(n),n.casefold())
    return sorted(matches,key=rank)[0]

def read_games(dat):
    items={}
    for _,node in ET.iterparse(io.BytesIO(dat),events=("end",)):
        if node.tag not in ("game","machine"):continue
        code=(node.get("name") or "").strip().lower()
        title=(node.findtext("description") or "").strip()
        clone=(node.get("cloneof") or "").strip().lower()
        if re.fullmatch(r"[a-z0-9_.+-]{1,90}",code) and title:
            items[code]=(title,clone)
        node.clear()
    return items

def main():
    ap=argparse.ArgumentParser()
    ap.add_argument("--output",required=True)
    ap.add_argument("--min-count",type=int,default=2000)
    o=ap.parse_args()
    games=read_games(retrieve(DAT_URL,24*1024*1024))
    indexes=[]
    for name,url in SOURCE_URLS:
        try:
            parser=IndexParser()
            parser.feed(retrieve(url,12*1024*1024).decode("utf-8","replace"))
            names=parser.names
            if len(names)<1000:
                raise RuntimeError(f"{name}: only {len(names)} source names")
        except Exception as exc:
            print(f"ARCADE {name} CDN unavailable ({exc}); using verified official GitHub tree")
            names=official_github_names(name)
        if len(names)<1000:
            raise RuntimeError(f"{name}: official thumbnail index unexpectedly small")
        print("THUMBNAIL DIRECTORY",name,len(names))
        indexes.append(catalog(names))
    lines=["# rom<TAB>verified FBNeo filename<TAB>verified MAME/community filename; UTF8; no guessed URLs\n"]
    exact=parent=0
    examples={}
    for code,(title,clone) in sorted(games.items()):
        by=[]
        own=False
        for index in indexes:
            name=select_entry(title,index)
            by.append(name)
            own=own or bool(name)
        if not own and clone in games:
            parent_title=games[clone][0]
            by=[select_entry(parent_title,index) for index in indexes]
            if any(by):parent+=1
        elif own:exact+=1
        if not any(by):continue
        lines.append("%s\t%s\t%s\n"%(code,by[0],by[1]))
        if code in ("1942","gng","ghouls","mslug3","pacman","sf2","wh1","galaga"):
            examples[code]=(title,by)
    if exact+parent<o.min_count:
        raise RuntimeError(f"Only {exact+parent} mapped games, need {o.min_count}")
    path=Path(o.output)
    path.parent.mkdir(parents=True,exist_ok=True)
    path.write_text("".join(lines),encoding="utf8",newline="\n")
    print("ARCADE COVERS INDEX games",len(games),"matched",exact,"parent",parent,
          "total",exact+parent,"size_bytes",path.stat().st_size)
    print("Examples:",examples)

if __name__=="__main__":main()
