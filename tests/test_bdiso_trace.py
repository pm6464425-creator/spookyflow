#!/usr/bin/env python3
"""Ensure diagnostic build measures real stages without changing UI or ISO."""
from pathlib import Path
def read(p):return Path(p).read_text()
app=read("src/core/app.c")
main=read("src/ps3/main.c")
ui=read("src/core/ui.c")
cover=read("src/core/coverart.c")
ps3=read("src/ps3/ps3_covers.c")
art=read("src/ps3/sf_bdiso_art.c")
diag=read("src/core/cover_diag.c")
checks=[
 ("Auto report at startup", 'cover_diag_start(SF_DATA_DIR "/diagnostico_portadas.txt")' in main),
 ("Standalone synchronized file", '__sync_lock_test_and_set(&g_lock,1)' in diag),
 ("Immediately flushed lines", "fflush(g_file);" in diag),
 ("No added 3D case stripe", '"BLU-RAY 3D"' not in ui),
 ("USB title and cover data untouched by diag code", 'cover_diag("INICIO"' in main),
 ("Actual cover request logged", 'cover_diag("SOLICITUD"' in cover),
 ("Real candidate bases logged", 'cover_diag("BUSQUEDA"' in cover),
 ("Real cover file located", 'cover_diag("ARCHIVO"' in cover),
 ("Real decode error returned", 'cover_diag("DECOD_ERROR"' in cover),
 ("Native NTFS read success and signature", 'cover_diag("NTFS_READ_OK"' in ps3),
 ("Native NTFS read failure", 'cover_diag("NTFS_READ_ERR"' in ps3),
 ("GPU pixel buffer state", 'cover_diag(ok?"PIXELS_OK":"PIXELS_FALLO"' in cover),
 ("Resulting texture slot state", 'cover_diag(j->done_ok?"TEXTURA_OK":"TEXTURA_FALLO"' in cover),
 ("Screen state measured without log every frame", '"PANTALLA_OK":"PANTALLA_ESTADO"' in ui and 'last_signature[slot]!=sig' in ui),
 ("Download API source statuses", 'cover_diag("HTTP_FALLO"' in art),
 ("Actual USB write result", 'cover_diag("USB_WRITE_OK"' in art and 'cover_diag("USB_WRITE_ERR"' in art),
 ("Terminal download return code", '"DESCARGA_OK":"DESCARGA_ERR"' in art),
 ("Diagnostic file closes", 'cover_diag_stop();' in main),
 ("Native manual JPG pipeline retained", 'n=add(out,n,max,covers,"/",name);' in cover),
 ("Different BDISO app cases preserved", "sf_bdntfs_start()" in main),
]
for i,(name,ok) in enumerate(checks,1):print(f"{i:02d} {'PASS' if ok else 'FAIL'} {name}")
n=sum(ok for _,ok in checks)
print("Runtime-trace instrumentation checks:",n,"/",len(checks))
raise SystemExit(n!=len(checks))
