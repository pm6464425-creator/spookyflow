/* SNES-only PS3 handoff: preserve source ROMs; stage into webMAN's explicit
 * SNES9X2010 core folder so ZIP is never confused with other systems.
 * This code does not touch the four completed platform backends. */
#include <stdio.h>
#include <string.h>
#include <sys/file.h>
#include "app.h"
#include "log.h"
#include "ps3_platform.h"
#include "sf_snes_launch.h"
#include "snes_launcher.h"

static unsigned char s_copy_buffer[32768];

static int stage_rom(const char *source, char *dest, size_t capacity)
{
    char tmp[256], backup[256];
    sysFSStat st;
    FILE *in, *out;
    unsigned long long total = 0;
    int copy_ok = 1;
    size_t n;
    if (snes_launcher_stage_path(source, dest, capacity) != 0) return -64;
    if (sysFsStat(source, &st) != 0 || !st.st_size) return -65;
    if (st.st_size > SF_SNES_MAX_ROM_BYTES) return -67;
    /* ROM source is never moved, altered or removed. */
    if (sysFsMkdir("/dev_hdd0/ROMS", 0777) != 0 &&
        !ps3_fs_exists("/dev_hdd0/ROMS")) return -66;
    if (sysFsMkdir(SF_SNES_STAGE_DIR, 0777) != 0 &&
        !ps3_fs_exists(SF_SNES_STAGE_DIR)) return -66;
    if (snprintf(tmp, sizeof(tmp), "%s.sfpart", dest) >= (int)sizeof(tmp) ||
        snprintf(backup, sizeof(backup), "%s.sfbak", dest) >= (int)sizeof(backup)) return -66;
    in = fopen(source, "rb");
    if (!in) return -65;
    out = fopen(tmp, "wb");
    if (!out) { fclose(in); return -66; }
    while ((n = fread(s_copy_buffer, 1, sizeof(s_copy_buffer), in)) != 0) {
        total += n;
        if (total > (unsigned long long)st.st_size ||
            fwrite(s_copy_buffer, 1, n, out) != n) { copy_ok = 0; break; }
    }
    if (ferror(in) || total != (unsigned long long)st.st_size) copy_ok = 0;
    if (fflush(out) != 0) copy_ok = 0;
    if (fclose(out) != 0) copy_ok = 0;
    if (fclose(in) != 0) copy_ok = 0;
    if (!copy_ok) { (void)remove(tmp); return -66; }
    /* On PS3, rename(tmp, existing) may fail. Preserve old stage on failure.
     * An old .sfbak indicates interrupted work: don't silently overwrite it. */
    if (rename(tmp, dest) != 0) {
        if (!ps3_fs_exists(dest) || ps3_fs_exists(backup) ||
            rename(dest, backup) != 0) { (void)remove(tmp); return -66; }
        if (rename(tmp, dest) != 0) {
            (void)rename(backup, dest);
            (void)remove(tmp);
            return -66;
        }
        (void)remove(backup);
    }
    log_printf("snes: staged %llu bytes %s -> %s", total, source, dest);
    return 0;
}

/* Check exactly the installations the bundled PKGLAUNCH launcher supports.
 * No writes are attempted until the required core and launcher exist. */
static int snes_backend_preflight(void)
{
    static const char *const cores[] = {
        "/dev_hdd0/game/RETROARCH/USRDIR/cores/snes9x2010_libretro_ps3.SELF",
        "/dev_hdd0/game/SSNE10000/USRDIR/cores/snes9x2010_libretro_ps3.SELF",
        "/dev_hdd0/game/SSNE10001/USRDIR/cores/snes9x2010_libretro_psl1ght.SELF"
    };
    unsigned i;
    if (!ps3_fs_exists("/dev_hdd0/game/PKGLAUNCH/USRDIR/EBOOT.BIN")) {
        log_printf("snes: missing PKGLAUNCH, no changes made");
        return -70;
    }
    for (i = 0; i < sizeof(cores)/sizeof(cores[0]); ++i) {
        if (ps3_fs_exists(cores[i])) {
            log_printf("snes: Snes9x 2010 core verified at %s", cores[i]);
            return 0;
        }
    }
    log_printf("snes: Snes9x 2010 core not installed, no changes made");
    return -71;
}

int sf_snes_launch(const char *rom_path)
{
    unsigned char config[1024];
    char path[256], command[LAUNCH_CMD_MAX];
    WmConfig wm;
    size_t n = 0;
    FILE *f;
    int rc;
    if (!snes_launcher_is_rom(rom_path)) return -64;
    rc = snes_backend_preflight();
    if (rc != 0) return rc;
    /* Query the existing webMAN Auto-play setting; avoid double invocation. */
    f = fopen(WM_CONFIG_PATH, "rb");
    if (f) { n = fread(config, 1, sizeof(config), f); (void)fclose(f); }
    if (!launch_parse_wm_config(config, n, &wm)) memset(&wm, 0, sizeof(wm));
    rc = stage_rom(rom_path, path, sizeof(path));
    if (rc != 0) { log_printf("snes: stage failed (%d) for %s", rc, rom_path); return rc; }
    if (snes_launcher_request(path, wm.valid && wm.autoplay, command,
                              sizeof(command)) != 0) return -68;
    log_printf("snes: Snes9x 2010 webMAN handoff: %s", command);
    if (ps3_webman_send(command) != 0) return -69;
    return 0;
}
