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
    puts("PASS unrelated arcade platforms unchanged, no re-routing non-FBNeo games");
    return 0;
}
