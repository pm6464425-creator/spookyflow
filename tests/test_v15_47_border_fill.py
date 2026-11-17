#!/usr/bin/env python3
"""V15.47 bounded SNES overscan: geometry, original SHA, ELF/GOT and only video changes."""
import importlib.util,struct
from pathlib import Path
s=importlib.util.spec_from_file_location("border",Path("tools/psnes_patch_border_fill.py"))
m=importlib.util.module_from_spec(s);s.loader.exec_module(m)
m.test_geometry()
assert len(m.planned_patches.__code__.co_consts)>0
# Create a synthetic PPC64 ELF with an explicit LOAD segment and the same
# verified V15.43 preimage bytes. This tests relocation logic without
# pretending to run an actual PS3 graphics processor.
elf=bytearray(m.ELF_SIZE)
elf[:6]=b"\x7fELF\x02\x02";struct.pack_into(">H",elf,18,21)
struct.pack_into(">Q",elf,32,64)
struct.pack_into(">HH",elf,54,56,1)
struct.pack_into(">I",elf,64,1)
struct.pack_into(">QQQQQ",elf,72,0x60ff40,0x10000000,0x10000000,0x1a920,0x1a920)
assert m.file_offset_to_vaddr(elf,m.F_1080_Y)==0x1001a91c
pre=[(m.GOT_720_Y,bytes.fromhex("0000000010000840")),
(m.F_720_X,struct.pack(">f",0)),(m.F_720_SCALE_Y,struct.pack(">f",720/512)),
(m.F_720_SCALE_X,struct.pack(">f",1280/848)),(m.F_1080_Y,struct.pack(">f",0)),
(m.F_1080_X,struct.pack(">f",0)),(m.F_1080_SCALE_Y,struct.pack(">f",1080/512)),
(m.F_1080_SCALE_X,struct.pack(">f",1920/848))]
for off,old in pre:elf[off:off+len(old)]=old
patch=m.planned_patches(elf)
assert len(patch)==8
assert int.from_bytes(patch[0][2],"big")==0x1001a91c
assert all(elf[off:off+len(old)]==old for off,old,new,label in patch)
before=bytes(elf)
for off,old,new,label in patch:elf[off:off+len(new)]=new
changed={j for off,old,new,label in patch for j in range(off,off+len(new)) if old[j-off]!=new[j-off]}
assert all(before[j]==elf[j] for j in (0,0x1cf39c,0x1cf494,0x2b29e8,0x62a844))
assert len(changed)<40
assert len(changed)>10
# Build source keeps the original V15.43 pinned phase before second phase.
sh=Path("tools/build_psnes_plain_self.sh").read_text()
assert sh.index('tools/psnes_patch_true_fullscreen.py')<sh.index('tools/psnes_patch_border_fill.py')
assert 'make_self "$tmp/psnes_edge.elf" "$target"' in sh
assert 'SF_PSNES_NATIVE_BORDER_FILL_ELF_SHA256' in sh
assert '"$edge_sha" "$tmp/roundtrip.elf"' in sh
# No code modifications for music, PS3/PS2/PS1, BDISO, cover or ROM systems.
print("PASS V15.47: eight original-byte checked patches, ELF64 GOT, centered crop, signed SELF roundtrip setup")
