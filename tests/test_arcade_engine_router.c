/* ARCADE-only launch selection, exact names, zip format and BIOS guard. */
#include "arcade_engine.h"
#include <assert.h>
#include <stdio.h>
int main(void)
{
    assert(arcade_engine_list_count()==7);
    assert(arcade_engine_for_rom("/dev_hdd0/ARCADE/gng.zip")==ARCADE_ENGINE_FBNEO);
    assert(arcade_engine_for_rom("/dev_hdd0/ARCADE/MSLUG3.ZIP")==ARCADE_ENGINE_FBNEO);
    assert(arcade_engine_for_rom("/dev_hdd0/ARCADE/mk2.zip")==ARCADE_ENGINE_MAME125);
    assert(arcade_engine_for_rom("/dev_hdd0/ARCADE/nbajam.zip")==ARCADE_ENGINE_MAME125);
    assert(arcade_engine_for_rom("/dev_hdd0/ARCADE/not_supported.zip")==ARCADE_ENGINE_NONE);
    assert(arcade_engine_for_rom("/dev_hdd0/ARCADE/ghosts.7z")==ARCADE_ENGINE_NONE);
    assert(arcade_engine_for_rom("/dev_hdd0/ROMS/ARCADE/gng.zip")==ARCADE_ENGINE_NONE);
    assert(arcade_engine_for_rom("/dev_hdd0/ARCADE/../gng.zip")==ARCADE_ENGINE_NONE);
    assert(arcade_engine_for_rom("/dev_hdd0/ARCADE/neogeo.zip")==ARCADE_ENGINE_NONE);
    assert(arcade_engine_for_rom("/dev_hdd0/ARCADE/pgm.zip")==ARCADE_ENGINE_NONE);
    puts("PASS: single X routes direct ZIP -> verified FBNeo/MAME125, rejects BIOS, 7z and unknown");
    return 0;
}
