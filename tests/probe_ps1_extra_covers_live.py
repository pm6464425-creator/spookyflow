#!/usr/bin/env python3
"""Live external-source smoke test for two REAL PS1 game covers.
No ROM downloads. This is a network sanity check, not PS3 TLS validation.
"""
from urllib.request import Request,urlopen
from urllib.error import URLError
sources={
"Ace Combat 3 - Electrosphere (USA)":
 "https://thumbnails.libretro.com/Sony%20-%20PlayStation/Named_Boxarts/Ace%20Combat%203%20-%20Electrosphere%20(USA).png",
"Tekken 3 OT Golden Leon (LaunchBox Russian front)":
 "https://images.launchbox-app.com/817f6c51-f659-4f8f-b0f2-40a486b39b60.png",
}
fails=[]
for label,url in sources.items():
    try:
        req=Request(url,headers={"User-Agent":"Mozilla/5.0 (compatible; SpookyFlow-cover-source-check/1.0)"})
        with urlopen(req,timeout=20) as r:
            blob=r.read(3*1024*1024+1)
            status=r.status
            kind=r.headers.get("Content-Type","")
        ok=status==200 and len(blob)>64 and len(blob)<=3*1024*1024 and blob.startswith(b"\x89PNG\r\n\x1a\n")
        print(("PASS" if ok else "FAIL"),label,"status",status,"bytes",len(blob),"type",kind)
        if not ok:fails.append(label)
    except Exception as e:
        print("FAIL",label,type(e).__name__,str(e)[:180])
        fails.append(label)
print("Live PS1 cover sources: %d/%d"%(len(sources)-len(fails),len(sources)))
raise SystemExit(bool(fails))
