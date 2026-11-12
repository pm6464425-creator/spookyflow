#!/usr/bin/env python3
"""Exploratory live source availability report; never fetches images at CI."""
from urllib.request import Request,urlopen
from urllib.error import HTTPError,URLError
import re,html
sources={
 "mubis-avatar":"https://www.mubis.es/titulos/avatar-blu-ray-3d",
 "mubis-avatar-art":"https://www.mubis.es/titulos/avatar-blu-ray-3d/caratula",
 "mubis-thor":"https://www.mubis.es/titulos/thor-blu-ray-3d",
 "bluray-avatar":"https://www.blu-ray.com/movies/Avatar-3D-Blu-ray/210144/",
 "bluray-search":"https://www.blu-ray.com/search/?quicksearch=1&quicksearch_keyword=avatar+3d&quicksearch_country=US",
 "mubis-search":"https://www.mubis.es/buscar?search=avatar+3d",
}
for name,url in sources.items():
 print("SOURCE",name,url,flush=True)
 try:
  req=Request(url,headers={"User-Agent":"Mozilla/5.0 (compatible; SpookyFlow/1.0)","Accept":"text/html"})
  with urlopen(req,timeout=12) as h:
   data=h.read(300000)
   st=h.status
   ct=h.headers.get("Content-Type")
  text=data.decode("utf-8","replace")
  print("STATUS",st,"BYTES",len(data),"TYPE",ct)
  for p in [r'<meta[^>]+(?:og:image|twitter:image)[^>]+>',r'<meta[^>]+(?:og:title|twitter:title)[^>]+>',r'<img[^>]+(?:cover|avatar)[^>]+>',r'<a[^>]+href="[^"]+(?:[0-9]+/|blu-ray-3d)[^"]+"[^>]*>']:
   found=re.findall(p,text,re.I|re.S)
   print("MATCH",p[:25],":",len(found))
   for x in found[:2]: print(" TAG",re.sub(r"\s+"," ",html.unescape(x))[:380])
 except Exception as e:
  print("ERROR",type(e).__name__,str(e)[:160])
