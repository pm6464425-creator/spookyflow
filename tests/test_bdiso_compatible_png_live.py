#!/usr/bin/env python3
"""Probe safe PS3 PNG conversion URLs against live Cinemeta poster images."""
import json,urllib.parse,urllib.request,urllib.error
hdr={"User-Agent":"spookyflow/PS3","Accept":"*/*","Accept-Encoding":"identity"}
catalog="https://v3-cinemeta.strem.io/catalog/movie/top/search=Terminator%202.json"
with urllib.request.urlopen(urllib.request.Request(catalog,headers=hdr),timeout=18) as r:
    metas=json.loads(r.read(160000))["metas"]
    film=next(x for x in metas if x.get("name","").lower() in
              ("terminator 2: judgment day","terminator 2"))
poster=film["poster"]
imdb=film["id"]
assert poster.startswith("https://")
print("Poster source:",poster[:145],"IMDb:",imdb)
qfull=urllib.parse.quote(poster,safe="")
qhost=urllib.parse.quote(poster[8:],safe="-_.~")
origins=[
  ("weserv-full", "https://images.weserv.nl/?url="+qfull+"&w=384&h=560&fit=contain&output=png"),
  ("weserv-ssl",  "https://images.weserv.nl/?url=ssl:"+qhost+"&w=384&h=560&fit=contain&output=png"),
  ("wsrv-full",   "https://wsrv.nl/?url="+qfull+"&w=384&h=560&fit=contain&output=png"),
  ("wsrv-ssl",    "https://wsrv.nl/?url=ssl:"+qhost+"&w=384&h=560&fit=contain&output=png"),
  ("metahub",     "https://images.metahub.space/poster/medium/"+imdb+"/img"),
  ("metahub-weserv","https://images.weserv.nl/?url=images.metahub.space%2Fposter%2Fmedium%2F"+imdb+"%2Fimg&w=384&output=png"),
]
found=False
for label,url in origins:
    try:
        with urllib.request.urlopen(urllib.request.Request(url,headers=hdr),timeout=12) as r:
            head=r.read(40)
            print(label,r.status,r.headers.get("Content-Type"),head[:12].hex())
            if head.startswith(b"\x89PNG\r\n\x1a\n") or head.startswith(b"\xff\xd8\xff"):
                if head.startswith(b"\x89PNG\r\n\x1a\n"):
                    w=int.from_bytes(head[16:20],"big")
                    h=int.from_bytes(head[20:24],"big")
                    print("PNG OK:",label,w,h)
                    if w<=2048 and h<=2048:found=True
                else:
                    print("JPEG source works:",label)
    except Exception as e:
        print(label,"UNAVAILABLE",str(e)[:140])
print("Compatible PNG found:",found)
assert found,"No compatible PNG proxy reachable; no PS3 compatibility claim"
