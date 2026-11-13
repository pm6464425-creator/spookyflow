/* Explicit BDISO trace of scan -> candidate -> file -> decoder -> GPU.
 * This keeps the runtime code independent of support tools, and makes it
 * clear what the PS3 itself did, not what a CI runner simulated. */
#include "cover_diag.h"
#include <stdio.h>
#include <stdarg.h>
#include <string.h>

static FILE *g_file;
static volatile int g_lock;
static unsigned g_line;

static void lock_diag(void)
{
    while(__sync_lock_test_and_set(&g_lock,1)){}
}
static void unlock_diag(void){__sync_lock_release(&g_lock);}

int cover_diag_start(const char *path)
{
#ifdef SF_DISABLE_DIAG_TXT
    (void)path;
    return 0;
#else
    lock_diag();
    if(g_file){fflush(g_file);fclose(g_file);g_file=NULL;}
    g_line=0;
    g_file=(path && path[0])?fopen(path,"w"):NULL;
    if(g_file){
        fputs("SPOOKYFLOW - DIAGNOSTICO DE PORTADAS BDISO\n"
              "Este archivo lo crea la PS3, no GitHub.\n"
              "Etapas: SCAN -> SOLICITUD -> RUTA -> LECTURA -> DECODIFICACION -> TEXTURA -> PANTALLA\n"
              "Si una etapa falla, su codigo indica donde investigar.\n"
              "------------------------------------------------------------\n",g_file);
        fflush(g_file);
    }
    unlock_diag();
    return g_file?0:-1;
#endif
}
void cover_diag(const char *stage,const char *fmt,...)
{
    va_list ap;
    if(!g_file)return;
    lock_diag();
    if(g_file && g_line<2500){
        fprintf(g_file,"%05u | %-13.13s | ",++g_line,stage?stage:"INFO");
        va_start(ap,fmt);
        vfprintf(g_file,fmt,ap);
        va_end(ap);
        fputc('\n',g_file);
        fflush(g_file);
    }
    unlock_diag();
}
void cover_diag_stop(void)
{
    lock_diag();
    if(g_file){fputs("FIN DEL INFORME\n",g_file);fflush(g_file);fclose(g_file);g_file=NULL;}
    unlock_diag();
}
