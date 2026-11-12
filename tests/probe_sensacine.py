#!/usr/bin/env python3
"""Non-invasive availability probe: SensaCine theatrical poster markup.
Do not download poster image files; inspect only public-page metadata."""
from urllib.request import Request,urlopen
from urllib.error import HTTPError,URLError
import re,html
sites=[
 "https://www.sensacine.com/buscar/?q=Avatar",
 "https://www.sensacine.com/peliculas/pelicula-61282/",
 "https://www.sensacine.com/peliculas/pelicula-61282/fotos/detalle/?cmediafile=19150275",
 "https://www.sensacine.com/buscar/?q=47%20Ronin",
]
for url in sites:
 print("CHECK",url,flush=True)
 try:
  with urlopen(Request(url,headers={"User-Agent":"Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 Chrome/120.0 Safari/537.36","Accept":"text/html,*/*","Accept-Language":"es-ES,es;q=0.9","Accept-Encoding":"identity"}),timeout=20) as r:
   body=r.read(600000).decode("utf-8","replace")
   print("HTTP",r.status,"length",len(body),"final",r.url)
   for m in list(re.finditer(r'<img[^>]+alt="Cartel de [^"]+"[^>]*>',body,re.I|re.S))[:2]:
    print("POSTER_CONTEXT",re.sub(r"\s+"," ",body[max(0,m.start()-800):m.end()+400])[:1150])
   for regex in [r'<meta[^>]+(?:og:image|twitter:image)[^>]*>',r'<meta[^>]+(?:og:title|twitter:title)[^>]*>',r'<img[^>]+(?:Cartel|poster|avatar)[^>]*>',r'<a[^>]+href="[^"]*pelicula-[0-9]+[^"]*"[^>]*>',r'(?:https?:)?//[^" ]*acsta[^" ]*']:
    vals=re.findall(regex,body,re.I|re.S)
    print("PATTERN",regex[:33],"MATCHES",len(vals))
    for val in vals[:3]: print("  ",html.unescape(re.sub(r"\s+"," ",val))[:400])
 except Exception as e: print("ERROR",type(e).__name__,str(e)[:200])
