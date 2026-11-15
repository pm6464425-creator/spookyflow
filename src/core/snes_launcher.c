#include "snes_launcher.h"
#include "launch.h"
#include "util.h"
#include <stdio.h>
#include <string.h>

static int is_usb_snes(const char *path)
{
    const char *p;
    if (strncmp(path, "/dev_usb", 8) != 0) return 0;
    p = path + 8;
    if (!(p[0] >= '0' && p[0] <= '9') ||
        !(p[1] >= '0' && p[1] <= '9') ||
        !(p[2] >= '0' && p[2] <= '9')) return 0;
    return strncmp(p + 3, "/SNES/", 6) == 0;
}

static const char *rom_extension(const char *path)
{
    if (sf_ends_with_i(path, ".sfc")) return ".sfc";
    if (sf_ends_with_i(path, ".smc")) return ".smc";
    if (sf_ends_with_i(path, ".zip")) return ".zip";
    return NULL;
}

int snes_launcher_is_rom(const char *source)
{
    const char *p;
    if (!source || !rom_extension(source)) return 0;
    if (strncmp(source, "/dev_hdd0/SNES/", 15) != 0 &&
        !is_usb_snes(source)) return 0;
    if (strstr(source, "/../") || strstr(source, "/./") ||
        strstr(source, "//") || strstr(source, "\\")) return 0;
    for (p = source; *p; ++p)
        if ((unsigned char)*p < 0x20u || (unsigned char)*p == 0x7fu)
            return 0;
    return 1;
}

int snes_launcher_stage_path(const char *source, char *out, size_t cap)
{
    const char *ext;
    int n;
    if (!out || cap == 0) return -1;
    out[0] = '\0';
    if (!snes_launcher_is_rom(source)) return -1;
    ext = rom_extension(source);
    n = snprintf(out, cap, SF_SNES_STAGE_DIR "/SF_%08X%s",
                 (unsigned)sf_hash(source), ext);
    if (n < 0 || (size_t)n >= cap) { out[0] = '\0'; return -1; }
    return 0;
}

int snes_launcher_request(const char *staged, int wm_autoplay,
                          char *out, size_t cap)
{
    char encoded[LAUNCH_CMD_MAX];
    int n;
    if (!out || cap == 0) return -1;
    out[0] = '\0';
    if (!staged || strncmp(staged, SF_SNES_STAGE_DIR "/SF_",
                           sizeof(SF_SNES_STAGE_DIR "/SF_") - 1) != 0 ||
        !rom_extension(staged) || strstr(staged, "..")) return -1;
    if (launch_url_encode(staged, encoded, sizeof(encoded)) != 0) return -1;
    n = snprintf(out, cap, "/wait.ps3?xmb;/mount_ps3%s?random=0%s",
                 encoded,
                 wm_autoplay ? "" : ";/wait.ps3?2;/play.ps3");
    if (n < 0 || (size_t)n >= cap || n >= LAUNCH_CMD_MAX - 64) {
        out[0] = '\0'; return -1;
    }
    return 0;
}
