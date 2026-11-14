#!/usr/bin/env python3
"""Offline SNES catalog builder test with real-world file names."""
import importlib.util
from pathlib import Path
from tempfile import TemporaryDirectory
p=Path("tools/build_snes_boxart_catalog.py")
spec=importlib.util.spec_from_file_location("snes_catalog_build",p)
module=importlib.util.module_from_spec(spec)
spec.loader.exec_module(module)
src=b'''<html><a href="Super%20Mario%20World%20%28USA%29.png">Mario</a>
<a href="Donkey%20Kong%20Country%20%28USA%29.png">DKC</a>
<a href="Legend%20of%20Zelda%2C%20The%20-%20A%20Link%20to%20the%20Past%20%28USA%29.png">Zelda</a>
<a href="rom.zip">bad</a></html>'''
names=module.parse(src)
assert names==sorted([
 "Super Mario World (USA).png",
 "Donkey Kong Country (USA).png",
 "Legend of Zelda, The - A Link to the Past (USA).png"
],key=str.casefold)
with TemporaryDirectory() as d:
    target=Path(d)/"snes_boxart_names.txt"
    module.write(names,target,min_count=3)
    assert target.read_text(encoding="utf-8").splitlines()==names
print("PASS SNES official indexed filenames, no ROM payloads, no guessed names")
