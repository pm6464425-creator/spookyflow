#include "snes_launcher.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static void ok(const char *source, const char *extension)
{
    char stage[256], uri[2048];
    assert(snes_launcher_is_rom(source));
    assert(snes_launcher_stage_path(source, stage, sizeof(stage)) == 0);
    assert(strstr(stage, "/dev_hdd0/ROMS/SNES9X2010/SF_") == stage);
    assert(strstr(stage, extension) == stage + strlen(stage) - strlen(extension));
    assert(snes_launcher_request(stage, 0, uri, sizeof(uri)) == 0);
    assert(strstr(uri, "/mount_ps3/dev_hdd0/ROMS/SNES9X2010/") != NULL);
    assert(strstr(uri, ";/wait.ps3?2;/play.ps3") != NULL);
    assert(snes_launcher_request(stage, 1, uri, sizeof(uri)) == 0);
    assert(strstr(uri, "/play.ps3") == NULL);
}

int main(void)
{
    char a[128], b[128], tiny[6], uri[2048];
    ok("/dev_hdd0/SNES/Super Mario World.sfc", ".sfc");
    ok("/dev_hdd0/SNES/Final Fantasy VI.ZIP", ".zip");
    ok("/dev_usb007/SNES/Donkey Kong Country.smc", ".smc");
    ok("/dev_usb000/SNES/RPG/Rom (Japan).SFC", ".sfc");
    assert(!snes_launcher_is_rom("/dev_hdd0/PS3ISO/game.iso"));
    assert(!snes_launcher_is_rom("/dev_hdd0/SNES/../PSXISO/game.sfc"));
    assert(!snes_launcher_is_rom("/dev_hdd0/SNES/foo.7z"));
    assert(!snes_launcher_is_rom("/dev_usb011/PS3ISO/game.zip"));
    assert(!snes_launcher_is_rom("/dev_hdd0/SNES/file.sfc\n"));
    assert(snes_launcher_stage_path("/dev_hdd0/SNES/Super Mario World.sfc", a, sizeof(a)) == 0);
    assert(snes_launcher_stage_path("/dev_hdd0/SNES/Super Mario World.sfc", b, sizeof(b)) == 0);
    assert(!strcmp(a, b));
    assert(snes_launcher_stage_path("/dev_hdd0/SNES/foo.zip", tiny, sizeof(tiny)) != 0);
    assert(snes_launcher_request("/dev_hdd0/ROMS/SNES9X2010/not-staged.zip", 0, uri, sizeof(uri)) != 0);
    assert(snes_launcher_request(a, 0, tiny, sizeof(tiny)) != 0);
    puts("PASS: SNES formats, roots, staging, deterministic naming, Auto-play deduplication");
    return 0;
}
