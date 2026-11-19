#!/usr/bin/env python3
"""Data-only test, no real ROM/BIOS bytes. Exercising 100+4+4+4 structs."""
from pathlib import Path
import importlib.util,struct
modpath=Path("tools/patch_fbneo_legacy.py")
spec=importlib.util.spec_from_file_location("legacy",modpath)
mod=importlib.util.module_from_spec(spec)
import sys
sys.modules[spec.name]=mod
spec.loader.exec_module(mod)
rows=mod.mapping()
original=bytearray(b"\x7fELF"+b"\0"*500)
for item in rows:
    name=item.modern.encode()
    original+=name+b"\0"*(100-len(name))
    original+=struct.pack(">III",item.size_modern,item.crc_modern,0x1234)
original+=b"UNTOUCHED_SENTINEL"
base=bytes(original)
result,stats=mod.transform(base,verbose=False)
assert len(result)==len(base)
assert base.endswith(b"UNTOUCHED_SENTINEL")
assert result.endswith(b"UNTOUCHED_SENTINEL")
assert sum(stats.values())==len(rows)
start=504
for row in rows:
    assert result[start:start+100].split(b"\0",1)[0].decode()==row.legacy
    assert struct.unpack_from(">III",result,start+100)==(row.size_legacy,row.crc_legacy,0x1234)
    start+=112
assert original==bytearray(base)
assert result!=base
print("PASS 19 modified descriptor names/sizes/CRCs; no code or ROM/BIOS data touched")
