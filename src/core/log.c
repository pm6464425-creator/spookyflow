/* log.c - line logger, flushed after every line. */
#include "log.h"
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

static FILE *g_log;
static double g_time;
static int g_echo;
#ifdef SF_SINGLE_DIAG_FILE
/* The old V15.32 report was flooded by ~500 ROM_FOUND + cover cache
 * events, then overwritten on reboot. Preserve the last useful launch
 * across XMB restarts and record ONLY execution diagnostics. */
#define SF_DIAG_MAX_BYTES (96u * 1024u)
#define SF_DIAG_NEW_SESSION_LIMIT (80u * 1024u)
static const char s_diag_header[] =
    "SPOOKYFLOW - DIAGNOSTICO DE ARRANQUE SNES V15.40\n"
    "Este reporte conserva el intento anterior al regresar al XMB.\n"
    "Recoge preflight, copia del motor, lanzamiento y boot.log de pSNES.\n"
    "Sin listados de ROMs, cache de caratulas ni rastros de otras plataformas.\n"
    "----------------------------------------------------------------------\n";
static size_t s_file_bytes;

static int snes_launch_trace(const char *msg)
{
    const char *s;
    if (strncmp(msg, "SNES_DIAG", 9) == 0) return 1;
    if (strncmp(msg, "snes: ", 6) != 0) return 0;
    s = msg + 6;
    /* Filter by ALLOWLIST: no hundreds of ROM_FOUND/CACHE_HIT/COVER_GPU. */
    return strncmp(s, "DIAG_", 5) == 0 ||
           strncmp(s, "handoff pSNES", 13) == 0 ||
           strncmp(s, "embedded pSNES", 14) == 0 ||
           strncmp(s, "PSNE00001", 9) == 0 ||
           strncmp(s, "verified pSNES", 14) == 0 ||
           strncmp(s, "missing bundled", 15) == 0 ||
           strncmp(s, "could not", 9) == 0 ||
           strncmp(s, "failed to", 9) == 0;
}
#endif

int log_open(const char *path)
{
#ifdef SF_SINGLE_DIAG_FILE
    FILE *prev = NULL;
    char header[sizeof(s_diag_header)] = {0};
    long old_size = -1;
    int append = 0;
    const size_t header_size = sizeof(s_diag_header) - 1;
    if (g_log) { fclose(g_log); g_log = NULL; }
    s_file_bytes = 0;
    if (!path || !path[0]) return -1;
    prev = fopen(path, "rb");
    if (prev) {
        if (fseek(prev, 0, SEEK_END) == 0) old_size = ftell(prev);
        rewind(prev);
        if (old_size >= (long)header_size &&
            old_size < (long)SF_DIAG_NEW_SESSION_LIMIT &&
            fread(header, 1, header_size, prev) == header_size &&
            memcmp(header, s_diag_header, header_size) == 0) {
            append = 1;
        }
        (void)fclose(prev);
    }
    g_log = fopen(path, append ? "ab" : "wb");
    if (!g_log) return -1;
    if (!append) {
        if (fwrite(s_diag_header, 1, header_size, g_log) != header_size) {
            fclose(g_log); g_log = NULL;
            return -1;
        }
        s_file_bytes = header_size;
    } else {
        s_file_bytes = (size_t)old_size;
    }
    {
        const char *separator =
            "\n========== NUEVO INICIO DE SPOOKYFLOW V15.40 ==========\n";
        const size_t n = strlen(separator);
        if (fwrite(separator, 1, n, g_log) != n || fflush(g_log) != 0) {
            fclose(g_log); g_log = NULL;
            return -1;
        }
        s_file_bytes += n;
    }
    return 0;
#elif defined(SF_DISABLE_DIAG_TXT)
    (void)path;
    if (g_log) { fclose(g_log); g_log = NULL; }
    return 0;
#else
    if (g_log) fclose(g_log);
    g_log = fopen(path, "a");
    return g_log ? 0 : -1;
#endif
}

void log_close(void)
{
    if (g_log) { fflush(g_log); fclose(g_log); g_log = 0; }
}

void log_set_time(double seconds) { g_time = seconds; }
void log_set_echo(int on) { g_echo = on; }

static volatile int g_lock;

void log_printf(const char *fmt, ...)
{
    char line[600];
    va_list ap;
    int n = snprintf(line, sizeof(line), "[%8.3f] ", g_time);
    if (n < 0) n = 0;
    if ((size_t)n >= sizeof(line)) n = (int)sizeof(line) - 1;
    va_start(ap, fmt);
    vsnprintf(line + n, sizeof(line) - (size_t)n, fmt, ap);
    va_end(ap);
    /* the library scan logs from a worker thread: serialize file writes */
    while (__sync_lock_test_and_set(&g_lock, 1)) { }
#ifdef SF_SINGLE_DIAG_FILE
    if(g_log) {
        const char *msg=line+n;
        /* Only SNES launch, crash-handoff and embedded pSNES boot trace. */
        int important = snes_launch_trace(msg);
        size_t len=strlen(line);
        if(important && s_file_bytes+len+1<SF_DIAG_MAX_BYTES) {
            fputs(line,g_log);fputc('\n',g_log);(void)fflush(g_log);
            s_file_bytes+=len+1;
        }
    }
#else
    if (g_log) { fputs(line, g_log); fputc('\n', g_log); fflush(g_log); }
#endif
    if (g_echo) { fputs(line, stdout); fputc('\n', stdout); }
    __sync_lock_release(&g_lock);
}
