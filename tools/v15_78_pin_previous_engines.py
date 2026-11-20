#!/usr/bin/env python3
"""V15.78 build-only pinning to the original V15.77 PS1/SNES signed assets.

The user's existing PKG contains older, unmodified signed APP SELF files.
New signing is nondeterministic. Before rebuilding SpookyFlow's EBOOT,
update only its GENERATED hash headers to accept those exact original files.
The engine executables and source code are never modified.
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
     "65a3462a4bbfccf2ec1c089d99f7dede1c2336430ca78d29876ebc25e0671701"),
)

if sys.argv[1:] not in ([], ["--check"]):
    raise SystemExit("usage: v15_78_pin_previous_engines.py [--check]")
check = sys.argv[1:] == ["--check"]

for path, size_key, sha_key, array_key, size, sha in OLD_ASSETS:
    text = path.read_text(encoding="utf-8")
    if not check:
        lines = text.splitlines(keepends=True)
        count_size = count_sha = 0
        for index, line in enumerate(lines):
            if line.startswith("#define " + size_key + " "):
                count_size += 1
                lines[index] = "#define " + size_key + " " + str(size) + "ULL\n"
            if line.startswith("#define " + sha_key + " "):
                count_sha += 1
                lines[index] = '#define ' + sha_key + ' "' + sha + '"\n'
        if count_size != 1 or count_sha != 1:
            raise RuntimeError("missing generated size/hash macros in " + str(path))
        text = "".join(lines)
        array_pattern = (r"static const unsigned char " + array_key +
                         r"\[32\]\s*=\s*\{[^}]*\};")
        val = bytes.fromhex(sha)
        chunks = [",".join("0x%02x" % b for b in val[i:i+8])
                  for i in range(0, 32, 8)]
        new_array = ("static const unsigned char " + array_key +
                     "[32]={\n    " + ",\n    ".join(chunks) + "\n};")
        text, n = re.subn(array_pattern, lambda _m: new_array, text,
                          count=1, flags=re.S)
        if n != 1:
            raise RuntimeError("missing generated hash array in " + str(path))
        path.write_text(text, encoding="utf-8")
    # Validate the exact constants and byte arrays, for both modes.
    if "#define " + size_key + " " + str(size) + "ULL" not in text:
        raise RuntimeError("wrong signed engine size: " + str(path))
    if '#define ' + sha_key + ' "' + sha + '"' not in text:
        raise RuntimeError("wrong signed engine SHA: " + str(path))
    m = re.search(r"static const unsigned char " + array_key +
                  r"\[32\]\s*=\s*\{([^}]*)\}", text, re.S)
    if not m:
        raise RuntimeError("hash array missing: " + str(path))
    parsed = bytes(int(x, 16) for x in re.findall(r"0x[0-9a-fA-F]{2}",
                                                   m.group(1)))
    if parsed != bytes.fromhex(sha):
        raise RuntimeError("hash array mismatch: " + str(path))
    print("VERIFIED" if check else "PINNED", path.name, size, sha)
print("PASS: original PS1/SNES engines match V15.78 launcher fingerprints")
