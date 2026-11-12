#!/usr/bin/env python3
"""Live external-network diagnostic, not an assertion about PS3 connectivity."""
import urllib.request
urls=[
    "https://images.metahub.space/poster/medium/tt1020876/img",
    "https://images.metahub.space/poster/small/tt1020876/img",
    "https://v3-cinemeta.strem.io/meta/movie/tt1020876.json",
]
for u in urls:
    try:
        req=urllib.request.Request(u,headers={"User-Agent":"Mozilla/5.0 (PLAYSTATION 3; SpookyFlow/1.0)"})
        with urllib.request.urlopen(req,timeout=12) as r:
            sample=r.read(24)
            print("SUCCESS",r.status,r.headers.get("Content-Type"),r.geturl(),sample[:12].hex())
    except Exception as e:
        print("UNAVAILABLE",u,type(e).__name__,str(e)[:200])
