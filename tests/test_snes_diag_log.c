/* Host regression of the real C logger used on PS3.
 * Reproduces V15.39 failure: ~500 ROMs overwhelm the useful launch trace,
 * and the next SpookyFlow boot destroys the crash evidence. */
#include "log.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static char buf[32768];

static size_t read_file(const char *path)
{
    FILE *f = fopen(path, "rb");
    size_t n;
    assert(f);
    n = fread(buf, 1, sizeof(buf)-1, f);
    buf[n] = '\0';
    assert(!ferror(f));
    fclose(f);
    return n;
}
static int count(const char *s, const char *needle)
{
    int n = 0;
    size_t len = strlen(needle);
    while ((s = strstr(s, needle)) != NULL) { n++; s += len; }
    return n;
}
int main(int argc, char **argv)
{
    int i;
    FILE *f;
    const char *path;
    size_t n;
    assert(argc == 2);
    path = argv[1];
    /* Existing user's V15.32 diagnostic has hundreds of ROMs and
     * MUST be replaced by the new, more useful format. */
    f = fopen(path, "wb");
    assert(f);
    fputs("SPOOKYFLOW - DIAGNOSTICO SNES (V15.32)\n"
          "snes: ROM_FOUND /dev_hdd0/SNES/irrelevant.smc\n", f);
    fclose(f);

    assert(log_open(path) == 0);
    for (i = 0; i < 500; i++) {
        log_printf("snes: ROM_FOUND path=/dev_hdd0/SNES/Game%04d.sfc", i);
        log_printf("snes: CACHE_HIT rom=/dev_hdd0/SNES/Game%04d.sfc", i);
        log_printf("snes: COVER_GPU slot=%d ok=1", i % 30);
    }
    log_printf("snes: DIAG_LAUNCH_SELECTED rom=/dev_hdd0/SNES/X-Kaliber 2097.smc");
    log_printf("snes: DIAG_ENGINE_PREPARE_RESULT=0");
    log_printf("snes: DIAG_SPAWN_CALL exe=/dev_hdd0/game/PSNE00001/USRDIR/EBOOT.BIN");
    log_printf("bdiso: unrelated system");
    log_close();
    n = read_file(path);
    assert(n < 4096);
    assert(strstr(buf, "V15.40"));
    assert(strstr(buf, "DIAG_LAUNCH_SELECTED"));
    assert(strstr(buf, "DIAG_SPAWN_CALL"));
    assert(!strstr(buf, "ROM_FOUND"));
    assert(!strstr(buf, "CACHE_HIT"));
    assert(!strstr(buf, "COVER_GPU"));
    assert(!strstr(buf, "bdiso: unrelated"));

    /* Simulate the exit to XMB followed by SpookyFlow restart. */
    assert(log_open(path) == 0);
    log_printf("snes: DIAG_PREVIOUS_HANDOFF_FOUND ROM=/dev_hdd0/SNES/X-Kaliber 2097.smc");
    log_printf("snes: DIAG_PSNES_BOOT[1]=PSNE00001 ctor first");
    log_printf("snes: DIAG_RESULT=PSNES_STARTED_BUT_NOT_RENDER_LOOP");
    log_close();

    n = read_file(path);
    assert(n < 4096);
    assert(count(buf, "NUEVO INICIO DE SPOOKYFLOW V15.40") == 2);
    assert(strstr(buf, "DIAG_SPAWN_CALL"));
    assert(strstr(buf, "DIAG_PREVIOUS_HANDOFF_FOUND"));
    assert(strstr(buf, "DIAG_PSNES_BOOT"));
    assert(strstr(buf, "DIAG_RESULT=PSNES_STARTED_BUT_NOT_RENDER_LOOP"));

    /* Repeated read must not discard the original launch attempt. */
    assert(log_open(path) == 0);
    log_printf("snes: DIAG_NO_PENDING_HANDOFF");
    log_close();
    n = read_file(path);
    assert(n < 8192);
    assert(count(buf, "DIAG_SPAWN_CALL") == 1);
    assert(count(buf, "NUEVO INICIO DE SPOOKYFLOW V15.40") == 3);
    remove(path);
    puts("PASS V15.40: 1500 noise lines suppressed; launch and pSNES boot retained after restart");
    return 0;
}
