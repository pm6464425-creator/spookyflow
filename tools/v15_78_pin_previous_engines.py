#!/usr/bin/env python3
"""V15.78 packaging-only compatibility step.

The distributed V15.77 pkg retains PS1 and SNES SELF assets signed in an
earlier build. make_self generates nondeterministic metadata, so a newly
compiled main EBOOT must pin the HASHES OF THOSE EXISTING ASSETS, not those
of newly re-signed copies staged by a CI run.

This script changes ONLY generated .h constants in the build workspace.
It never edits emulators, ROMs, packages, or the engine verification code.
"""
from pathlib import Path
import re
import sys

OLD_ASSETS = (
    (Path("src/ps3/sf_ps1_engine_hash.h"),
     "SF_PS1_ENGINE_SIZE", "SF_PS1_ENGINE_SELF_SHA256",
     "k_sf_ps1_engine_hash", 903456,
     "d77c24b182acd8bb1dc2771fc562b13b0f85811fbe5bc59fed52c831da29b720"),
    (Path("src/ps3/sf_psnes_plain_hash.h"),
     "PSNES_PLAIN_SIZE", "SF_PSNES_APP_SELF_SHA256",
     "k_plain_sha256", 2614992,
     "65a3462a4bbfccf2ec1c089d99f7dede1c2336430ca78d29876ebc25e0671701")
)
check_only = sys.argv[1:] == ["--check"]
if sys.argv[1:] and not check_only:
    raise SystemExit("usage: v15_78_pin_previous_engines.py [--check]")

def replace_exact(text,pattern,replacement,ident):
    text2, n = re.subn(pattern, lambda _m: replacement, text, flags=re.M|re.S)
    if n != 1:
        raise RuntimeError(f"{ident}: expected exactly 1 match, got {n}")
    return text2

for path,size_key,sha_key,array_name,size,hexsha in OLD_ASSETS:
    text=path.read_text(encoding="utf-8")
    if check_only:
        assert re.search(r"(?m)^#define "+size_key+rf"\s+{size}ULL\s*$",text),path
        assert re.search(r'(?m)^#define '+sha_key+r' "'+hexsha+r'"\s*$',text),path
        arr=re.search(r'static const unsigned char '+array_name+r'\[32\]\s*=\s*\{([^}]*)\}',text,re.S)
        assert arr and bytes(int(x,16) for x in re.findall(r'0x[0-9a-fA-F]{2}',arr.group(1))) == bytes.fromhex(hexsha),path
    else:
        text=replace_exact(text,rf"^#define {size_key} .+$",
                           f"#define {size_key} {size}ULL",size_key)
        text=replace_exact(text,rf'^#define {sha_key} .+$',
                           f'#define {sha_key} "{hexsha}"',sha_key)
        values=bytes.fromhex(hexsha)
        rows=[",".join("0x%02x"%b for b in values[i:i+8]) for i in range(0,32,8)]
        line="static const unsigned char "+array_name+"[32]={\n    "+",\n    ".join(rows)+"\n};"
        text=replace_exact(text,
            r"static const unsigned char "+array_name+r"\[32\]\s*=\s*\{[^}]*\};",
            line,array_name)
        path.write_text(text,encoding="utf-8")
    print(("VERIFIED" if check_only else "PINNED"),
          path.name,"bytes",size,"sha256",hexsha)
print("PASS: old V15.77 PS1/SNES signed engine signatures match new launcher pins")
