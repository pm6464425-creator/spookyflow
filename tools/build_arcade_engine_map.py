#!/usr/bin/env python3
"""Convert public FBNeo Retro Loader Plus v2.30 PS3 SQLite database into
a small, sorted, authoritative ROM shortname -> original PS3 core mapping.
Do not substitute the newer FBNeo general DAT (it overstates PS3 support).
"""
import argparse
import collections
import sqlite3
from pathlib import Path
import re
def main():
    a=argparse.ArgumentParser()
    a.add_argument("--db",required=True);a.add_argument("--output",required=True)
    ns=a.parse_args()
    con=sqlite3.connect("file:"+ns.db+"?mode=ro",uri=True)
    rows=con.execute("select name,def_core_id from games where system='mame'").fetchall()
    out={}
    for name,core in rows:
        if not name:continue
        code=name.strip().lower()
        if not re.fullmatch(r"[a-z0-9_.+-]{1,90}",code):continue
        engine="M" if core==3 else "F"
        if code not in out or engine=="M":out[code]=engine
    assert len(rows)>=6500, len(rows)
    assert len(out)>=6500, len(out)
    assert sum(x=="M" for x in out.values())>=50
    assert out["gng"]=="F" and out["mslug3"]=="F"
    path=Path(ns.output);path.parent.mkdir(parents=True,exist_ok=True)
    path.write_text("# v2.30 retro.db: arcade code<TAB>core; F=FBNeo; M=MAME125\n"+
                    "".join(f"{k}\t{out[k]}\n" for k in sorted(out)),
                    encoding="ascii")
    print("PS3 ARCADE COMPAT:",len(out),
          "FBNeo:",sum(v=="F" for v in out.values()),
          "MAME125:",sum(v=="M" for v in out.values()),
          "map bytes",path.stat().st_size)
if __name__=="__main__":main()
