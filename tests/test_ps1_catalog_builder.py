#!/usr/bin/env python3
"""Offline PS1 Libretro index scraper test (no HTTP in regression build)."""
import importlib.util
from pathlib import Path
from tempfile import TemporaryDirectory
path=Path("tools/build_ps1_boxart_catalog.py")
spec=importlib.util.spec_from_file_location("ps1_builder",path)
m=importlib.util.module_from_spec(spec)
spec.loader.exec_module(m)
html=b"""<html><body>
<a href="Disney%20Tarzan%20%28Spain%29.png">Disney Tarzan (Spain)</a>
<a href="Disney%27s%20Tarzan%20%28USA%29.png">Disney's Tarzan (USA)</a>
<a href="Gran%20Turismo%202%20%28USA%29.png">Gran Turismo 2 (USA)</a>
<a href="../">Parent</a>
<a href="game.zip">No ROM imports</a>
</body></html>"""
names=m.parse(html)
assert names==sorted(["Disney Tarzan (Spain).png","Disney's Tarzan (USA).png",
                      "Gran Turismo 2 (USA).png"],key=str.casefold),names
with TemporaryDirectory() as d:
    out=Path(d)/"PS1 names.txt"
    m.write(names,out,min_count=3)
    assert out.read_text(encoding="utf-8").splitlines()==names
print("PASS PS1 catalog source names parsed, URL decoded, verified, no ROMs bundled")
