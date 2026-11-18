#!/usr/bin/env python3
"""Generate arcade-only ROM shortname -> official FBNeo description catalogue.

Original authoritative source (project update at each build):
https://github.com/libretro/FBNeo/blob/master/dats/
    FinalBurn Neo (ClrMame Pro XML, Arcade only).dat
No ROMs, samples, BIOS files or executable cores are ever downloaded.
"""
import argparse
import io
import re
import urllib.request
import xml.etree.ElementTree as ET
from pathlib import Path
from urllib.parse import quote

DAT = "FinalBurn Neo (ClrMame Pro XML, Arcade only).dat"
URL = "https://raw.githubusercontent.com/libretro/FBNeo/master/dats/" + quote(DAT)
VALID = re.compile(r"^[a-z0-9_+.-]{1,90}$")
SAMPLES = {"gng", "ghouls", "1942", "pacman", "galaga", "wh1", "commando", "frogger"}

def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--output", required=True)
    ap.add_argument("--min-count", type=int, default=4000)
    ns = ap.parse_args()
    req = urllib.request.Request(URL, headers={"User-Agent": "SpookyFlow-Arcade-Catalog/1.0"})
    with urllib.request.urlopen(req, timeout=90) as response:
        data = response.read(24 * 1024 * 1024)
    if b"<game" not in data and b"<machine" not in data:
        raise RuntimeError("Unexpected FBNeo DAT file format")
    names = {}
    for _, item in ET.iterparse(io.BytesIO(data), events=("end",)):
        if item.tag not in ("game", "machine"):
            continue
        rom = (item.attrib.get("name") or "").lower().strip()
        title = (item.findtext("description") or "").strip()
        title = " ".join(title.replace("\t", " ").replace("\r", " ").replace("\n", " ").split())
        if VALID.fullmatch(rom) and title:
            # Keep the authentic name as published by FBNeo; do not infer a title.
            names[rom] = title[:180]
        item.clear()
    if len(names) < ns.min_count:
        raise RuntimeError(f"Incomplete FBNeo catalogue: only {len(names)} games, need {ns.min_count}")
    # Certain old MAME archives use aliases not in the latest FBNeo DAT.
    # The tiny fallback built into arcade_catalog.c covers those names.
    out = Path(ns.output)
    out.parent.mkdir(parents=True, exist_ok=True)
    out.write_text(
        "# Original: libretro/FBNeo arcade-only DAT; UTF-8, sorted rom_set<TAB>title\n" +
        "".join(f"{rom}\t{names[rom]}\n" for rom in sorted(names)),
        encoding="utf-8", newline="\n"
    )
    missing = sorted(SAMPLES - names.keys())
    print("ARCADE CATALOG:", len(names), "official descriptions", out, out.stat().st_size, "bytes")
    print("Legacy MAME fallbacks (when absent in FBNeo):", missing)

if __name__ == "__main__":
    main()
