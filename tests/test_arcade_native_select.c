#include "arcade_native_select.h"
#include "arcade_engine.h"
#include <stdio.h>
#include <assert.h>
#include <string.h>
#define ROOT "/tmp/arcade_native_route_test/"
int main(void)
{
    char r[160];
    int engine=arcade_native_engine_select(ROOT "kof2002.zip",
             ARCADE_ENGINE_FBNEO,r,sizeof r);
    assert(engine==ARCADE_ENGINE_MAME125);
    assert(strstr(r,"ALL_GAME_CRC_MATCH"));
    printf("PASS legacy BIOS + original MAME0125 kof2002 CRC => MAME0125 (%s)\n",r);
    engine=arcade_native_engine_select(ROOT "gng.zip",
              ARCADE_ENGINE_FBNEO,r,sizeof r);
    assert(engine==ARCADE_ENGINE_FBNEO);
    engine=arcade_native_engine_select(ROOT "kof2002_missing.zip",
              ARCADE_ENGINE_FBNEO,r,sizeof r);
    assert(engine==ARCADE_ENGINE_FBNEO);
    engine=arcade_native_engine_select(ROOT "kof2002.zip",
              ARCADE_ENGINE_MAME125,r,sizeof r);
    assert(engine==ARCADE_ENGINE_MAME125);
    /* Modern BIOS must keep FBNeo for KOF2002; never choose MAME blindly. */
    assert(rename(ROOT "neogeo.zip",ROOT "neogeo_old_backup.zip")==0);
    assert(rename(ROOT "neogeo_modern.zip",ROOT "neogeo.zip")==0);
    engine=arcade_native_engine_select(ROOT "kof2002.zip",
              ARCADE_ENGINE_FBNEO,r,sizeof r);
    assert(engine==ARCADE_ENGINE_FBNEO);
    assert(strstr(r,"MODERN_CORRECT"));
    assert(rename(ROOT "neogeo.zip",ROOT "neogeo_modern.zip")==0);
    assert(rename(ROOT "neogeo_old_backup.zip",ROOT "neogeo.zip")==0);
    /* Simulate missing one declared chip with metadata-only ZIP fixture. */
    assert(rename(ROOT "kof2002.zip",ROOT "kof2002_full_backup.zip")==0);
    assert(rename(ROOT "kof2002_missing.zip",ROOT "kof2002.zip")==0);
    engine=arcade_native_engine_select(ROOT "kof2002.zip",
              ARCADE_ENGINE_FBNEO,r,sizeof r);
    assert(engine==ARCADE_ENGINE_NONE);
    assert(strstr(r,"CRC_MISMATCH"));
    assert(rename(ROOT "kof2002.zip",ROOT "kof2002_missing.zip")==0);
    assert(rename(ROOT "kof2002_full_backup.zip",ROOT "kof2002.zip")==0);
    puts("PASS modern BIOS=>FBNeo; missing chip=>no unsafe launch; unrelated arcade unchanged");
    return 0;
}
