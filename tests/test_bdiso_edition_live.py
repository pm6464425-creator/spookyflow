#!/usr/bin/env python3
"""Live smoke test (GitHub CI, NOT an emulated PS3) of genuine 3D art.
No copyright images are persisted or included in the application package.
"""
import re,urllib.request
h={"User-Agent":"Mozilla/5.0 (PLAYSTATION 3; SpookyFlow)","Accept":"*/*","Accept-Encoding":"identity"}
samples=[
 ("es-avatar","https://www.mubis.es/titulos/avatar-blu-ray-3d"),
 ("es-thor","https://www.mubis.es/titulos/thor-blu-ray-3d"),
]
for name,url in samples:
 with urllib.request.urlopen(urllib.request.Request(url,headers=h),timeout=15) as r:
  html=r.read(160000).decode("utf-8","replace")
  print(name,"page",r.status,len(html))
 title=re.search(r'<meta property="og:title" content="([^"]+)"',html)
 image=re.search(r'<meta property="og:image" content="([^"]+)"',html)
 assert title and "Blu-ray 3D" in title.group(1)
 assert image and image.group(1).startswith("https://www.mubis.es/media/releases/")
 assert "-3d-" in image.group(1).lower()
 with urllib.request.urlopen(urllib.request.Request(image.group(1),headers=h),timeout=18) as r:
  sig=r.read(32)
  print(name,"JPG",r.status,r.headers.get("Content-Type"),sig[:6].hex())
  assert sig[:3]==b"\xff\xd8\xff"
# Verified release IDs for actual 3D Blu-ray editions. Some publishers
# expose _front.jpg, others only _large.jpg. Report both; do not invent.
for ident in [82808,30402,33406,24347,210144]:
 for suffix in ["front","large"]:
  url=f"https://images.static-bluray.com/movies/covers/{ident}_{suffix}.jpg"
  try:
   with urllib.request.urlopen(urllib.request.Request(url,headers=h),timeout=12) as r:
    sig=r.read(8)
    print("blu-ray.com",ident,suffix,r.status,r.headers.get("Content-Type"),sig.hex())
  except Exception as e: print("blu-ray.com",ident,suffix,"UNAVAILABLE",type(e).__name__,str(e)[:70])
print("PASS: real Spanish 3D cover images validated")
