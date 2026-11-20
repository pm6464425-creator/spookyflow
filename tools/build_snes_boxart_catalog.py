#!/usr/bin/env python3
"""Build a verified, filenames-only Super Nintendo boxart index.

Source is the actual public Libretro Named_Boxarts directory. Never bundles
ROMs, copyrighted images, or unverified synthetic cover filenames.
"""
from __future__ import annotations
import argparse
from html.parser import HTMLParser
from html import unescape
from urllib.parse import unquote
from urllib.request import Request, urlopen
from pathlib import Path
from tempfile import TemporaryDirectory
from subprocess import run, check_output
from urllib.error import HTTPError, URLError

ROOT = "https://thumbnails.libretro.com/Nintendo%20-%20Super%20Nintendo%20Entertainment%20System/"
KINDS = ("Named_Boxarts", "Named_Titles", "Named_Logos", "Named_Snaps")
URL = ROOT + KINDS[0] + "/"
OUTPUT = Path("pkgfiles/USRDIR/snes_boxart_names.txt")

class CoverNames(HTMLParser):
    def __init__(self):
        super().__init__(convert_charrefs=True)
        self.names = set()
    def handle_starttag(self, tag, attrs):
        if tag != "a":
            return
        link = dict(attrs).get("href", "")
        if not link.lower().endswith(".png"):
            return
        name = unescape(unquote(link.rsplit("/", 1)[-1]))
        if not name or name.startswith(".") or any(ch in name for ch in ("/", "\\", "\n", "\r", "\t")):
            return
        if len(name.encode("utf-8")) > 220:
            return
        self.names.add(name)

def parse(html: bytes) -> list[str]:
    p = CoverNames()
    p.feed(html.decode("utf-8", "replace"))
    return sorted(p.names, key=str.casefold)

def write(names: list[str], path: Path, min_count: int, kind: str = "Named_Boxarts") -> None:
    assert len(names) >= min_count, "Partial SNES catalog: only %s covers" % len(names)
    assert "Super Mario World (USA).png" in names, "SNES Mario missing"
    if kind == "Named_Boxarts":
        assert "Donkey Kong Country (USA).png" in names, "SNES Donkey Kong Country missing"
        assert "Legend of Zelda, The - A Link to the Past (USA).png" in names, "SNES Zelda missing"
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text("".join(n + "\n" for n in names), encoding="utf-8", newline="\n")
    assert path.stat().st_size < 512000, "SNES catalog exceeds PS3 memory budget"
    print("VERIFIED SNES BOXART NAMES: %d entries, %d bytes -> %s" %
          (len(names), path.stat().st_size, path))

def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--output", type=Path, default=OUTPUT)
    ap.add_argument("--fixture", type=Path)
    ap.add_argument("--min-count", type=int, default=2500)
    ap.add_argument("--kind", choices=KINDS, default="Named_Boxarts")
    args = ap.parse_args()
    if args.fixture:
        html = args.fixture.read_bytes()
    else:
        req = Request(ROOT + args.kind + "/", headers={"User-Agent": "SpookyFlow-SNES-cover-catalog/1.0"})
        try:
            with urlopen(req, timeout=45) as response:
                html = response.read(3 * 1024 * 1024)
            names=parse(html)
        except (HTTPError,URLError) as e:
            print(f"SNES listing unavailable ({e}); indexing official Git tree names")
            with TemporaryDirectory(prefix="sf_snes_list_") as tmp:
                run(["git","clone","--filter=blob:none","--no-checkout",
                     "--single-branch","--depth","1",
                     "https://github.com/libretro-thumbnails/Nintendo_-_Super_Nintendo_Entertainment_System.git",
                     tmp],check=True,timeout=180)
                ls=check_output(["git","-C",tmp,"ls-tree","-r","--name-only",
                                 "HEAD"],timeout=90).decode("utf-8")
                names=sorted({
                    p[len(args.kind)+1:]
                    for p in ls.splitlines()
                    if p.startswith(args.kind+"/") and p.endswith(".png")
                    and "/" not in p[len(args.kind)+1:]
                },key=str.casefold)
    if args.fixture:
        names=parse(html)
    write(names, args.output, args.min_count, args.kind)

if __name__ == "__main__":
    main()
