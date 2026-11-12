#!/usr/bin/env python3
"""Check public movie-poster conversion into PS3-readable PNG bytes.
An outside service may be temporarily unavailable; report errors honestly.
"""
import json
import urllib.parse
import urllib.request
h={"User-Agent":"spookyflow/PS3","Accept":"*/*","Accept-Encoding":"identity"}
catalog="https://v3-cinemeta.strem.io/catalog/movie/top/search=Terminator%202.json"
with urllib.request.urlopen(urllib.request.Request(catalog,headers=h),timeout=20) as r:
    metas=json.loads(r.read(160000))["metas"]
    match=next(x for x in metas if x.get("name","").lower()=="terminator 2: judgment day"
               or x.get("name","").lower()=="terminator 2")
    poster=match["poster"]
    print("Poster:",poster[:120])
    assert poster.startswith("https://m.media-amazon.com/") or poster.startswith("https://image.tmdb.org/") or poster.startswith("https://images.metahub.space/")
    assert "?" not in poster
    payload=urllib.parse.quote(poster[8:],safe="-_.~")
    url="https://images.weserv.nl/?url="+payload+"&w=384&h=560&fit=contain&output=png"
with urllib.request.urlopen(urllib.request.Request(url,headers=h),timeout=25) as r:
    head=r.read(24)
    print("PS3 PNG:",r.status,r.headers.get("Content-Type"),head[:8].hex(),len(head))
    assert head.startswith(b"\x89PNG\r\n\x1a\n")
    w=int.from_bytes(head[16:20],"big"); ht=int.from_bytes(head[20:24],"big")
    print("Dimensions:",w,"x",ht)
    assert 1 <= w <= 384 and 1 <= ht <= 560
print("PASS PNG conversion: compatible PNG data from real movie poster")
