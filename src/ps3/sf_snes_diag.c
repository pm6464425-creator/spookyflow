/* V15.40 - persistent SNES launch diagnosis.
 * A process replacement cannot write to SpookyFlow after sysProcessExitSpawn2.
 * We record the handoff DURABLY, and on the next SpookyFlow start merge the
 * original pSNES /USRDIR/boot.log into the same diagnostico_snes.txt.
 * Neither ROMs nor any PS1/PS2/PS3/BDISO backend is modified.
 */
#include <stdio.h>
#include <string.h>
#include <sys/file.h>
#include "ps3_platform.h"
#include "sf_snes_diag.h"
#include "log.h"

#define SF_PENDING_PATH SF_DATA_DIR "/snes_launch_pending.bin"
#define SF_PSNES_BOOT_LOG "/dev_hdd0/game/PSNE00001/USRDIR/boot.log"
#define SF_PENDING_MAGIC "SF_SNES40_LAUNCH\n"
#define SF_PSNES_BOOT_MAX_LINES 80
#define SF_PSNES_BOOT_MAX_BYTES 12288

static void trim_crlf(char *s)
{
    size_t n = strlen(s);
    while (n && (s[n-1] == '\n' || s[n-1] == '\r')) s[--n] = '\0';
}

/* Called ONLY once a real SNES ROM and the engine have passed preflight. */
int sf_snes_diag_before_spawn(const char *rom_path)
{
    FILE *f;
    int clean_boot = 1;
    size_t n;
    if (!rom_path || !rom_path[0] || strlen(rom_path) >= 490 ||
        strchr(rom_path, '\r') || strchr(rom_path, '\n')) {
        log_printf("snes: DIAG_PENDING_WRITE_INVALID_ROM");
        return -79;
    }
    /* This is the pSNES-generated diagnostic scratch log ONLY, not a ROM,
     * save state, config or asset. Removing it prevents stale logs from
     * being falsely attributed to the next pSNES launch. */
    if (ps3_fs_exists(SF_PSNES_BOOT_LOG) &&
        remove(SF_PSNES_BOOT_LOG) != 0) {
        clean_boot = 0;
        log_printf("snes: DIAG_OLD_PSNES_BOOTLOG_CANNOT_CLEAR; next log may be stale");
    } else {
        log_printf("snes: DIAG_OLD_PSNES_BOOTLOG_CLEARED_OR_ABSENT");
    }
    f = fopen(SF_PENDING_PATH, "wb");
    if (!f) {
        log_printf("snes: DIAG_PENDING_MARKER_OPEN_FAILED");
        return -79;
    }
    n = strlen(SF_PENDING_MAGIC);
    if (fwrite(SF_PENDING_MAGIC, 1, n, f) != n ||
        fprintf(f, "%d\n%s\n", clean_boot, rom_path) < 0 ||
        fflush(f) != 0) {
        (void)fclose(f);
        (void)remove(SF_PENDING_PATH);
        log_printf("snes: DIAG_PENDING_MARKER_WRITE_FAILED");
        return -79;
    }
    if (fclose(f) != 0) {
        (void)remove(SF_PENDING_PATH);
        log_printf("snes: DIAG_PENDING_MARKER_CLOSE_FAILED");
        return -79;
    }
    log_printf("snes: DIAG_PENDING_MARKER_SAVED clean_bootlog=%d", clean_boot);
    return 0;
}

void sf_snes_diag_spawn_returned(void)
{
    log_printf("snes: DIAG_SPAWN_UNEXPECTED_RETURN; SpookyFlow is still running");
    if (remove(SF_PENDING_PATH) != 0)
        log_printf("snes: DIAG_PENDING_MARKER_REMOVE_FAILED");
}

void sf_snes_diag_on_startup(void)
{
    FILE *f, *boot;
    char magic[sizeof(SF_PENDING_MAGIC) + 4] = {0};
    char clean[16] = {0}, rom[512] = {0}, line[340];
    unsigned lines = 0, bytes = 0;
    int entered = 0, mainloop = 0;
    const size_t expected = strlen(SF_PENDING_MAGIC);
    f = fopen(SF_PENDING_PATH, "rb");
    if (!f) {
        log_printf("snes: DIAG_NO_PENDING_HANDOFF (select a SNES game to record a new launch)");
        return;
    }
    if (fread(magic, 1, expected, f) != expected ||
        memcmp(magic, SF_PENDING_MAGIC, expected) != 0 ||
        !fgets(clean, sizeof(clean), f) ||
        !fgets(rom, sizeof(rom), f)) {
        (void)fclose(f);
        log_printf("snes: DIAG_PENDING_MARKER_INVALID; cannot link this startup to a ROM");
        (void)remove(SF_PENDING_PATH);
        return;
    }
    (void)fclose(f);
    trim_crlf(clean);
    trim_crlf(rom);
    log_printf("snes: DIAG_PREVIOUS_HANDOFF_FOUND ROM=%s", rom);
    log_printf("snes: DIAG_PREVIOUS_HANDOFF_BOOTLOG_RESET=%s", clean[0] == '1' ? "YES" : "NO");

    boot = fopen(SF_PSNES_BOOT_LOG, "rb");
    if (!boot) {
        log_printf("snes: DIAG_PSNES_BOOTLOG=ABSENT; pSNES did not produce a new boot log");
        log_printf("snes: DIAG_RESULT=HANDOFF_FAILED_OR_PROCESS_STOPPED_BEFORE_FIRST_PSNES_CONSTRUCTOR");
        (void)remove(SF_PENDING_PATH);
        return;
    }
    log_printf("snes: DIAG_PSNES_BOOTLOG=FOUND path=" SF_PSNES_BOOT_LOG);
    while (lines < SF_PSNES_BOOT_MAX_LINES &&
           bytes < SF_PSNES_BOOT_MAX_BYTES &&
           fgets(line, sizeof(line), boot)) {
        size_t size = strlen(line);
        bytes += (unsigned)size;
        trim_crlf(line);
        /* pSNES currently prints safe stage names, but truncate all long
         * lines to avoid a noisy/corrupt log taking over the main report. */
        if (!line[0]) continue;
        if (strstr(line, "ctor first") || strstr(line, "main entered"))
            entered = 1;
        if (strstr(line, "enter main loop") || strstr(line, "main loop flip"))
            mainloop = 1;
        log_printf("snes: DIAG_PSNES_BOOT[%u]=%.240s", lines + 1, line);
        lines++;
    }
    if (ferror(boot))
        log_printf("snes: DIAG_PSNES_BOOT_READ_ERROR");
    if (!feof(boot))
        log_printf("snes: DIAG_PSNES_BOOT_TRUNCATED max_lines=%u max_bytes=%u",
                   (unsigned)SF_PSNES_BOOT_MAX_LINES,
                   (unsigned)SF_PSNES_BOOT_MAX_BYTES);
    (void)fclose(boot);
    if (clean[0] != '1')
        log_printf("snes: DIAG_RESULT=BOOTLOG_MAY_BE_STALE; old file was not cleared");
    else if (!entered)
        log_printf("snes: DIAG_RESULT=BOOTLOG_EXISTS_BUT_NO_ENTRY_TRACE; inspect above");
    else if (mainloop)
        log_printf("snes: DIAG_RESULT=PSNES_STARTED_AND_REACHED_MAIN_LOOP; investigate runtime exit");
    else
        log_printf("snes: DIAG_RESULT=PSNES_STARTED_BUT_NOT_RENDER_LOOP; inspect last boot stage");
    (void)remove(SF_PENDING_PATH);
}
