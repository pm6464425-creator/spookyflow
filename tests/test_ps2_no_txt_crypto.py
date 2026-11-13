#!/usr/bin/env python3
"""V15.17 release contract: mandatory PS2 Classics encryption; no diagnostic TXT
created by the packaged PS3 application. Data/index TXT files remain supported.
"""
from pathlib import Path
root = Path(__file__).resolve().parents[1]
def src(p): return (root/p).read_text(encoding="utf-8")
h=src("src/ps3/sf_ps2_hen.c")
app=src("src/core/app.c")
m=src("Makefile")
build=src("ci/build_ps3.sh")
checks={
 "01 mbedTLS 2.28 SHA1 API used for ISO fingerprint": "mbedtls_sha1_ret(raw,sizeof(raw),digest)!=0" in h,
 "02 mbedTLS 2.28 SHA1 API used for encrypted blocks": "mbedtls_sha1_ret(blk,SEG,meta+j*0x20)!=0" in h,
 "03 PS3 compilation rejects disabled encryption": '#error "PS2 Classics encryption requires mbedTLS' in h,
 "04 required mbedTLS build failure is fatal": 'if ! build_tls; then' in build and 'exit 1' in build,
 "05 CI always compiles PS2 with TLS and crypto": "make pkg SF_TLS=1" in build,
 "06 no insecure crypto-disabled fallback": "make pkg SF_TLS=0" not in build,
 "07 make defaults TLS enabled": "SF_TLS ?= 1" in m,
 "08 make defaults TXT reports disabled": "-DSF_DISABLE_DIAG_TXT" in m,
 "09 general application log file creation disabled": '#ifdef SF_DISABLE_DIAG_TXT' in src("src/core/log.c"),
 "10 covers diagnostic TXT creation disabled": '#ifdef SF_DISABLE_DIAG_TXT' in src("src/core/cover_diag.c"),
 "11 USB diagnostics begin and snapshots disabled": src("src/ps3/sf_usb_diag.c").count("#ifdef SF_DISABLE_DIAG_TXT")==2,
 "12 Cobra early and selection probes disabled": src("src/ps3/sf_cobra_mount.c").count("#ifdef SF_DISABLE_DIAG_TXT")==3,
 "13 PS2 Cobra journal disabled without blocking mount": '#ifdef SF_DISABLE_DIAG_TXT' in src("src/ps3/sf_ps2_mount.c") and '(void)stage; (void)path; (void)rc; (void)active;' in src("src/ps3/sf_ps2_mount.c"),
 "14 PS2 HEN launcher report disabled": '#ifndef SF_DISABLE_DIAG_TXT' in h and 'fopen(SF_LAUNCHER_PROBE,"wb")' in h,
 "15 PS2 HEN converter TXT report disabled": '#ifdef SF_DISABLE_DIAG_TXT' in h and 'fopen(SF_HEN_DIAG,"wb")' in h,
 "16 user messages no longer refer to TXT errors": all(x not in app for x in ("Error PS2 Classics (ps2_hen_last.txt)","ver registro","ver diagnóstico")),
 "17 preserves encrypted PS2 ISO and CONFIG files": 'SF_CLASSICS_TMP' in h and 'SF_CFG_TARGET' in h,
 "18 preserves boxart names data TXT": 'ps1_boxart_names.txt' in build,
 "19 preserves BDISO & PS3 mount callbacks": all(x in src("src/ps3/main.c") for x in ("host.launch_ps3 = sf_cobra_mount_ps3;","host.launch_bdiso = launch_bdiso;")),
 "20 preserves PS2 HEN launcher detection": 'launcher_sfo_id(char out[32])' in h,
}
for label,ok in checks.items(): print(("PASS" if ok else "FAIL"),label)
print("V15.17 PS2 encrypted TXT-free release checks: %d/%d"%(sum(checks.values()),len(checks)))
if not all(checks.values()): raise SystemExit(1)
