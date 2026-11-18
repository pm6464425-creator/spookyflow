/* log.c - line logger, flushed after every line. */
#include "log.h"
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

static FILE *g_log;
static double g_time;
static int g_echo;
/* V15.48 forensic mode: one bounded TXT, across process handoffs.
 * Former diagnostic implementations remain source-available but do NOT
 * create audio, cover or SNES TXT files in this build. */
#ifdef SF_VIDEO_ONLY_DIAG
#define SF_VIDEO_MAX (192u * 1024u)
#define SF_VIDEO_REUSE_LIMIT (160u * 1024u)
static size_t s_video_written;
#if defined(SF_PS1_VIDEO_DIAG)
static const char s_video_header[] =
    "SPOOKYFLOW V15.52 - DIAGNOSTICO AVANZADO PANTALLA PS1\n"
    "Informe exclusivo PS1; registra valores verificados antes del montaje.\n"
    "SONY PS/PS2 UPSCALER: 0=OFF, 1=NORMAL 4:3, 2=PANTALLA COMPLETA 16:9.\n"
    "NO se modifica xRegistry.sys ni los emuladores PS1 del sistema.\n"
    "NO mide pixeles HDMI ni el framebuffer del siguiente proceso PS1.\n"
    "Unico TXT: /dev_hdd0/tmp/spookyflow/diagnostico_pantalla_ps1.txt\n"
    "----------------------------------------------------------------------\n";
#else
static const char s_video_header[] =
    "SPOOKYFLOW V15.48 - DIAGNOSTICO AVANZADO DE VIDEO SNES\n"
    "DATOS DEL HARDWARE REAL (hasta lanzamiento) y arranque posterior de pSNES.\n"
    "NO MIDE PIXELES DEL JUEGO NI LA IMAGEN HDMI DESPUES DE SALIR DE SPOOKYFLOW.\n"
    "NO CONFUNDIR una firma correcta del SELF con pantalla completa comprobada.\n"
    "Una unica ruta: /dev_hdd0/tmp/spookyflow/diagnostico_pantalla_snes.txt\n"
    "----------------------------------------------------------------------\n";
#endif
static int video_report_open(const char *path)
{
    FILE *f;
    long size=-1;
    int append=0;
    char h[sizeof(s_video_header)]={0};
    size_t hlen=sizeof(s_video_header)-1;
    if(g_log) { (void)fclose(g_log);g_log=NULL; }
    s_video_written=0;
    if(!path || !path[0])return -1;
    f=fopen(path,"rb");
    if(f) {
        if(fseek(f,0,SEEK_END)==0)size=ftell(f);
        rewind(f);
        if(size>=(long)hlen && size<(long)SF_VIDEO_REUSE_LIMIT &&
           fread(h,1,hlen,f)==hlen && memcmp(h,s_video_header,hlen)==0)append=1;
        (void)fclose(f);
    }
    g_log=fopen(path,append?"ab":"wb");
    if(!g_log)return -1;
    if(!append) {
        if(fwrite(s_video_header,1,hlen,g_log)!=hlen) {
            (void)fclose(g_log);g_log=NULL;return -1;
        }
        s_video_written=hlen;
    } else s_video_written=(size_t)size;
    {
#if defined(SF_PS1_VIDEO_DIAG)
        static const char start[]="\n========== NUEVA SESION - VIDEO PS1 V15.52 ==========\n";
#else
        static const char start[]="\n========== NUEVA SESION - VIDEO SNES V15.48 ==========\n";
#endif
        size_t n=sizeof(start)-1;
        if(fwrite(start,1,n,g_log)!=n || fflush(g_log)!=0) {
            (void)fclose(g_log);g_log=NULL;return -1;
        }
        s_video_written+=n;
    }
    return 0;
}
#endif
#if defined(SF_SINGLE_DIAG_FILE) && !defined(SF_VIDEO_ONLY_DIAG)
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
/* V15.46: independent, persistent, AUDIO-ONLY forensic report.
 * The legacy report is for SNES and may be full of old entries. Keep
 * audio traces completely separate and append only until its 64 KiB
 * bound is reached, then replace this same one TXT (never create more).
 * This cannot read audio from a DIFFERENT PS3 process after handoff. */
#ifndef SF_AUDIO_DIAG_PATH
#define SF_AUDIO_DIAG_PATH "/dev_hdd0/tmp/spookyflow/diagnostico_audio.txt"
#endif
#define SF_AUDIO_DIAG_MAX (64u * 1024u)
#define SF_AUDIO_DIAG_ROTATE (56u * 1024u)
static FILE *s_audio_diag;
static size_t s_audio_written;
static const char s_audio_header[] =
    "SPOOKYFLOW - DIAGNOSTICO EXCLUSIVO DE AUDIO V15.46\n"
    "Registra solo la actividad de SpookyFlow hasta entregar el control.\n"
    "No puede escuchar ni inspeccionar automaticamente juegos que ejecuta otro proceso.\n"
    "Compare el MISMO juego iniciado desde XMB y desde SpookyFlow.\n"
    "Los valores RC son retornos de API, no prueba directa de audio HDMI.\n"
    "----------------------------------------------------------------------\n";

static int audio_diag_open(void)
{
    FILE *f;
    char head[sizeof(s_audio_header)];
    long old_size=-1;
    int append=0;
    const size_t hlen=sizeof(s_audio_header)-1;
    if (s_audio_diag) { (void)fclose(s_audio_diag); s_audio_diag=NULL; }
    s_audio_written=0;
    f=fopen(SF_AUDIO_DIAG_PATH,"rb");
    if(f) {
        if(fseek(f,0,SEEK_END)==0) old_size=ftell(f);
        rewind(f);
        if(old_size>=(long)hlen &&
           old_size<(long)SF_AUDIO_DIAG_ROTATE &&
           fread(head,1,hlen,f)==hlen &&
           memcmp(head,s_audio_header,hlen)==0)
            append=1;
        (void)fclose(f);
    }
    s_audio_diag=fopen(SF_AUDIO_DIAG_PATH,append?"ab":"wb");
    if(!s_audio_diag) return -1;
    if(!append) {
        if(fwrite(s_audio_header,1,hlen,s_audio_diag)!=hlen) {
            (void)fclose(s_audio_diag);s_audio_diag=NULL; return -1;
        }
        s_audio_written=hlen;
    } else s_audio_written=(size_t)old_size;
    {
        const char *mark="\n========== NUEVO INICIO DE SPOOKYFLOW - AUDIO V15.46 ==========\n";
        const size_t n=strlen(mark);
        if(fwrite(mark,1,n,s_audio_diag)!=n || fflush(s_audio_diag)!=0) {
            (void)fclose(s_audio_diag);s_audio_diag=NULL;return -1;
        }
        s_audio_written+=n;
    }
    return 0;
}


static int snes_launch_trace(const char *msg)
{
    const char *s;
    if (strncmp(msg, "SNES_DIAG", 9) == 0) return 1;
    /* V15.45: the original allowlist inadvertently hid EVERY audio trace,
     * including HANDOFF_STOP_RESULT from V15.44. Keep both formats in the
     * same bounded single diagnostic TXT, flushed before game handoff.
     * Only audio lifecycle events, not every platform scan, are recorded. */
    if (strncmp(msg, "audio: ", 7) == 0) {
        const char *a = msg + 7;
        return strncmp(a, "DIAG_", 5) == 0 ||
               strncmp(a, "HANDOFF_", 8) == 0 ||
               strncmp(a, "audioInit failed", 16) == 0 ||
               strncmp(a, "audioPortOpen failed", 20) == 0 ||
               strncmp(a, "audioPortStart failed", 21) == 0 ||
               strncmp(a, "audioGetPortConfig failed", 25) == 0 ||
               strncmp(a, "audioSetNotifyEventQueue failed", 30) == 0 ||
               strncmp(a, "event queue failed", 18) == 0 ||
               strncmp(a, "thread create failed", 20) == 0 ||
               strncmp(a, "started", 7) == 0;
    }
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
#if defined(SF_VIDEO_ONLY_DIAG)
    return video_report_open(path);
#elif defined(SF_SINGLE_DIAG_FILE)
    FILE *prev = NULL;
    char header[sizeof(s_diag_header)] = {0};
    long old_size = -1;
    int append = 0;
    const size_t header_size = sizeof(s_diag_header) - 1;
    if (g_log) { fclose(g_log); g_log = NULL; }
    s_file_bytes = 0;
    /* Open independently from the SNES report. An older SNES trace does
     * not control how much of the audio evidence is saved. */
    (void)audio_diag_open();
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
#if defined(SF_SINGLE_DIAG_FILE) && !defined(SF_VIDEO_ONLY_DIAG)
    if (s_audio_diag) {
        (void)fflush(s_audio_diag);
        (void)fclose(s_audio_diag);
        s_audio_diag=NULL;
    }
#endif
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
#if defined(SF_VIDEO_ONLY_DIAG)
    if(g_log) {
        const char *msg=line+n;
        size_t len=strlen(line);
#if defined(SF_PS1_VIDEO_DIAG)
        int is_video=strncmp(msg,"ps1_video_diag: ",16)==0;
        int is_ps1=strncmp(msg,"PS1_DIAG",8)==0 ||
                   strncmp(msg,"PS1_MOUNT",9)==0 ||
                   strncmp(msg,"PS1_JOIN",8)==0 ||
                   strncmp(msg,"ps1: ",5)==0;
        if((is_video || is_ps1) && s_video_written+len+1<SF_VIDEO_MAX) {
#else
        int is_video=strncmp(msg,"video_diag: ",12)==0;
        int is_snes=strncmp(msg,"snes: DIAG_",11)==0 ||
                    strncmp(msg,"snes: handoff pSNES",19)==0;
        if((is_video || is_snes) && s_video_written+len+1<SF_VIDEO_MAX) {
#endif
            if(fputs(line,g_log)>=0 && fputc('\n',g_log)!=EOF && fflush(g_log)==0)
                s_video_written+=len+1;
        }
    }
#elif defined(SF_SINGLE_DIAG_FILE)
    /* Write to one independent AUDIO-ONLY TXT, never mix ROM scans,
     * cover cache records or non-audio diagnostics into this file. */
    if(s_audio_diag) {
        const char *msg=line+n;
        size_t len=strlen(line);
        if((strncmp(msg,"audio: ",7)==0 || strncmp(msg,"music: ",7)==0) &&
           s_audio_written+len+1<SF_AUDIO_DIAG_MAX) {
            if(fputs(line,s_audio_diag)>=0 && fputc('\n',s_audio_diag)!=EOF &&
               fflush(s_audio_diag)==0)
                s_audio_written+=len+1;
        }
    }
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
