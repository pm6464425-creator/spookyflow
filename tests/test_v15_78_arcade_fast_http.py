#!/usr/bin/env python3
"""V15.78: exercise the ACTUAL isolated Arcade HTTPS allowlist and chunk framing.
No network access or PS3 hardware is required for these small C regressions.
"""
from pathlib import Path
import subprocess
import tempfile

src=Path("src/ps3/ps3_webman.c").read_text(encoding="utf-8")
def between(a,b):
    assert a in src and b in src, (a,b)
    return src[src.index(a):src.index(b,src.index(a))]
allow=between("static int arcade_boxart_url(", "static void arcade_keep_close(")
chunk=between("static int arcade_chunk_complete(", "static int arcade_keep_get(")
legacy=between("static int ps3_http_get_single(", "/* V15.78:")
assert 'HTTP/1.0' in legacy and 'Connection: close' in legacy
assert 'netDeinitialize()' in legacy
wrapper=src[src.index("int ps3_http_get("):src.index("int ps3_webman_get_wait(")]
assert "if(arcade_boxart_url(url))" in wrapper
assert "arcade_keep_get(url" in wrapper
assert "result=ps3_http_get_single(ctx,url" in wrapper
assert "arcade_keep_close();" in wrapper
assert 'Connection: keep-alive' in src
assert "if(!framed)return COVER_HTTP_NET;" in src
assert "if(chunked && http_dechunk" in src
harness=r'''
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
''' + allow + chunk + r'''
static void test(int value, int expected, const char *name){
 if(value!=expected){fprintf(stderr,"FAIL %s: %d != %d\n",name,value,expected);exit(1);}
}
int main(void){
 const uint8_t valid[]="3\r\nabc\r\n0\r\n\r\n";
 const uint8_t fragmented[]="3\r\nab";
 const uint8_t with_extension[]="3;foo=bar\r\nabc\r\n0\r\n\r\n";
 const uint8_t with_trailer[]="3\r\nabc\r\n0\r\nX-Test: ok\r\n\r\n";
 const uint8_t malformed[]="Z\r\nabc\r\n0\r\n\r\n";
 test(arcade_boxart_url("https://thumbnails.libretro.com/FBNeo%20-%20Arcade%20Games/Named_Boxarts/Pac-Man.png"),1,"FBNeo allow");
 test(arcade_boxart_url("https://thumbnails.libretro.com/MAME/Named_Boxarts/1942.png"),1,"MAME allow");
 test(arcade_boxart_url("https://thumbnails.libretro.com/Sony%20-%20PlayStation/Named_Boxarts/a.png"),0,"PS1 excluded");
 test(arcade_boxart_url("https://thumbnails.libretro.com/Nintendo/Named_Boxarts/a.png"),0,"SNES excluded");
 test(arcade_boxart_url("https://thumbnails.libretro.com/MAME/Named_Snaps/a.png"),0,"screenshots excluded");
 test(arcade_boxart_url("http://thumbnails.libretro.com/MAME/Named_Boxarts/a.png"),0,"HTTP excluded");
 test(arcade_boxart_url("https://thumbnails.libretro.com.evil.test/MAME/Named_Boxarts/a.png"),0,"foreign host excluded");
 test(arcade_boxart_url(NULL),0,"NULL excluded");
 test(arcade_chunk_complete(valid,sizeof(valid)-1),1,"chunk complete");
 test(arcade_chunk_complete(fragmented,sizeof(fragmented)-1),0,"chunk incomplete");
 test(arcade_chunk_complete(with_extension,sizeof(with_extension)-1),1,"chunk extension");
 test(arcade_chunk_complete(with_trailer,sizeof(with_trailer)-1),1,"chunk trailer");
 test(arcade_chunk_complete(malformed,sizeof(malformed)-1),-1,"reject malformed chunk");
 puts("PASS: 13 Arcade HTTP cases + unchanged legacy network path + bounded streaming frame");
 return 0;
}
'''
with tempfile.TemporaryDirectory(prefix="spooky_v1578_") as td:
    source=Path(td)/"test_arcade_http.c"
    binary=Path(td)/"test_arcade_http"
    source.write_text(harness,encoding="utf-8")
    subprocess.run(["cc","-std=c99","-O2","-Wall","-Wextra","-Werror",str(source),"-o",str(binary)],check=True)
    subprocess.run([str(binary)],check=True)
