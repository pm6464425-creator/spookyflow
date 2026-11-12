#!/usr/bin/env python3
"""Non-blocking live source smoke test. Runs on GitHub (NOT on PS3 hardware)."""
import json,urllib.parse,urllib.request,urllib.error
from urllib.parse import urlparse

movie="Avatar"
url="https://v3-cinemeta.strem.io/catalog/movie/top/search="+urllib.parse.quote(movie)+".json"
h={"User-Agent":"spookyflow/MI-PKG (PS3)","Accept":"*/*","Accept-Encoding":"identity"}
try:
    req=urllib.request.Request(url,headers=h)
    with urllib.request.urlopen(req,timeout=18) as resp:
        body=resp.read(600_000)
        print("Catalog:",resp.status,"final host:",urlparse(resp.geturl()).hostname,
              "type:",resp.headers.get("Content-Type"),
              "encoding:",resp.headers.get("Transfer-Encoding"),
              "bytes:",len(body))
    d=json.loads(body)
    metas=d.get("metas",[])
    assert metas, "no movie results returned"
    matches=[x for x in metas if x.get("name","").lower()=="avatar"]
    assert matches, f"search returned {len(metas)} but no Avatar"
    poster=matches[0].get("poster")
    assert poster and poster.startswith("https://")
    print("Movie:",matches[0].get("name"),"artwork host:",urlparse(poster).hostname)
    with urllib.request.urlopen(urllib.request.Request(poster,headers=h),timeout=18) as resp:
        head=resp.read(64)
        print("Poster:",resp.status,"final host:",urlparse(resp.geturl()).hostname,
              "content type:",resp.headers.get("Content-Type"),
              "first bytes:",head[:8].hex())
    assert head.startswith(b"\xff\xd8\xff"),"poster endpoint did not return JPEG"
    print("PASS: real public catalog and JPEG poster available from GitHub runner")
except Exception as exc:
    print("LIVE SOURCE UNAVAILABLE:",type(exc).__name__,str(exc))
    raise
