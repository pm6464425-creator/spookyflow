#!/usr/bin/env python3
"""Live metadata + real production C parser + JPEG, from GitHub runner only."""
import json, subprocess, tempfile, urllib.parse, urllib.request
from pathlib import Path
from urllib.parse import urlparse

h={"User-Agent":"spookyflow/MI-PKG (PS3)",
   "Accept":"*/*","Accept-Encoding":"identity"}
exe=Path("build-host/movie_catalog_cli")
subprocess.run(["cc","-std=c99","-O2","-Wall","-Wextra","-Werror",
                "-Isrc/core","src/core/movie_catalog.c",
                "tests/movie_catalog_cli.c","-o",str(exe)],check=True)

for movie,year in [("Avatar",2009),("Thor",2011),("Shrek",2001),("Dredd",2012)]:
    url="https://v3-cinemeta.strem.io/catalog/movie/top/search="+urllib.parse.quote(movie)+".json"
    with urllib.request.urlopen(urllib.request.Request(url,headers=h),timeout=20) as resp:
        raw=resp.read(600_000)
        assert resp.status==200
        print("Live catalog:",movie,"bytes:",len(raw),
              "encoding:",resp.headers.get("Transfer-Encoding"))
    parsed=json.loads(raw)
    assert parsed.get("metas"),"No results from public catalog"
    with tempfile.NamedTemporaryFile(suffix=".json") as f:
        f.write(raw);f.flush()
        subprocess.run([str(exe),f.name,movie,str(year)],check=True,timeout=8)
    if movie=="Avatar":
        item=next(m for m in parsed["metas"]
                  if m.get("name","").lower()=="avatar" and m.get("poster"))
        poster=item["poster"]
        with urllib.request.urlopen(urllib.request.Request(poster,headers=h),timeout=20) as resp:
            head=resp.read(64)
            print("Live poster:",resp.status,"host:",urlparse(resp.geturl()).hostname,
                  "first bytes:",head[:8].hex())
            assert head.startswith(b"\xff\xd8\xff")
print("PASS: shipping C parser matched 4 real public API responses and a real JPEG")
