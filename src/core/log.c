/* log.c - line logger, flushed after every line. */
#include "log.h"
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

static FILE *g_log;
static double g_time;
static int g_echo;
#ifdef SF_SINGLE_DIAG_FILE
#define SF_DIAG_MAX_BYTES (256u*1024u)
static size_t s_file_bytes;
#endif

int log_open(const char *path)
{
#ifdef SF_SINGLE_DIAG_FILE
    if (g_log) { fclose(g_log); g_log=NULL; }
    s_file_bytes=0;
    g_log=(path && path[0])?fopen(path,"wb"):NULL;
    if (!g_log) return -1;
    {const char *header =
        "SPOOKYFLOW - DIAGNOSTICO UNICO (V15.20)\n"
        "El informe se sobrescribe en cada inicio. NO captura directamente\n"
        "errores del ejecutable externo PS2 Classics Launcher despues del XMB.\n"
        "Solo nombres conocidos de diagnosticos antiguos se eliminan.\n"
        "--------------------------------------------------------------\n";
     size_t n=strlen(header);
     if(fwrite(header,1,n,g_log)!=n) { fclose(g_log);g_log=NULL;return -1; }
     s_file_bytes=n;
     (void)fflush(g_log);
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
        /* A focused report avoids megabytes of per-frame/per-cover chatter.
         * Always retain PS2 preparation, mount failures and system context. */
        int important =
            strstr(msg,"PS2") || strstr(msg,"ps2") ||
            strstr(msg,"cobra") || strstr(msg,"Cobra") ||
            strstr(msg,"BDISO") || strstr(msg,"bdiso") ||
            strstr(msg,"ERROR") || strstr(msg,"error") ||
            strstr(msg,"FALLO") || strstr(msg,"network:") ||
            strstr(msg,"====") || strstr(msg,"ntfs:");
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
