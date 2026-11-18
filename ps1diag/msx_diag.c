/* Only for Metal Slug X. Replaces one TXT each run; no logs for other games.
 * Reports raw START/SELECT, input blocking and START delivered to libretro.
 * Deletes only historic diagnostic TXT, never saves/settings/cover files. */
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <ctype.h>
#include <dirent.h>
#include <sys/stat.h>
#include <sys/systime.h>
#include "msx_diag.h"

#define DDIR "/dev_hdd0/tmp/spookyflow"
#define DFILE DDIR "/diagnostico_metal_slug_x.txt"
static FILE *df;
static int active, target_game, prev_raw=-1, prev_core=-1;
static unsigned physical_start, core_start, core_calls, blocked_calls;
static u64 summary_us;

static void say(const char *fmt, ...)
{
    va_list a;
    if (!df) return;
    fprintf(df, "[%llu ms] ", (unsigned long long)(sysGetSystemTime()/1000));
    va_start(a, fmt); vfprintf(df, fmt, a); va_end(a);
    fputc('\n', df); fflush(df);
}
static int old_diagnostic(const char *s)
{
    char b[256]; size_t i, n=strlen(s);
    if (n>=sizeof b || n<4) return 0;
    for (i=0; i<n; ++i) b[i]=(char)tolower((unsigned char)s[i]);
    b[n]=0;
    return !strcmp(b+n-4,".txt") &&
       (strstr(b,"diagnostico") || strstr(b,"diagnostic") ||
        strstr(b,"diag_") || strstr(b,"pantalla_ps1"));
}
void msx_diag_clean_previous(void)
{
    struct dirent *e; DIR *d; char p[512];
    mkdir(DDIR,0777);
    d=opendir(DDIR); if (!d) return;
    while ((e=readdir(d))) {
        if (!old_diagnostic(e->d_name)) continue;
        if (strlen(e->d_name)+sizeof(DDIR)>=sizeof p) continue;
        snprintf(p,sizeof p,"%s/%s",DDIR,e->d_name);
        remove(p);
    }
    closedir(d);
}
void msx_diag_boot(const char *launch)
{
    mkdir(DDIR,0777);
    df=fopen(DFILE,"w");
    if(!df) return;
    say("SpookyFlow PS1 engine started");
    say("Direct-launch target: %s",launch && *launch ? launch : "(none/unknown)");
    say("Waiting to start a PS1 game; this file will be replaced on game load");
    fclose(df); df=NULL;
}

static int is_metal_slug_x(const char *name)
{
    char b[512]; size_t i,j=0;
    for (i=0; name[i] && j<sizeof b-1; ++i)
        if (isalnum((unsigned char)name[i]))
            b[j++]=(char)tolower((unsigned char)name[i]);
    b[j]=0;
    return strstr(b,"metalslugx")!=NULL ||
           strstr(b,"slus01212")!=NULL || strstr(b,"sles03575")!=NULL ||
           strstr(b,"slpm86456")!=NULL || strstr(b,"slps02600")!=NULL ||
           strstr(b,"slps03449")!=NULL || strstr(b,"scps45490")!=NULL;
}
void msx_diag_begin(const char *name,const char *path)
{
    if (df) { fclose(df); df=NULL; }
    active=1; target_game=is_metal_slug_x(name);
    physical_start=core_start=core_calls=blocked_calls=0;
    prev_raw=prev_core=-1; summary_us=0;
    if (!active) return;
    mkdir(DDIR,0777);
    df=fopen(DFILE,"w");
    if (!df) return;
    say("SpookyFlow Metal Slug X - START diagnostic");
    say("Game: %s | image: %s",name,path);
    say("Target recognized=%d; digital pad only if 1",target_game);
    say("Log overwritten at each new Metal Slug X launch");
    say("Raw controller input and libretro START are logged separately");
}
int msx_diag_active(void) { return target_game; }
void msx_diag_virtual_pad(const char *type)
{
    if (active) say("Virtual PS1 controller port 1: %s",type);
}
void msx_diag_poll(const pad_state_t *p,int blocked)
{
    int s; u64 us;
    if (!active || !target_game) return;
    s=!!(p->buttons & PAD_START);
    if (s!=prev_raw) {
        if(s) physical_start++;
        say("Physical START=%d SELECT=%d connected=%d blocked=%d",
            s,!!(p->buttons & PAD_SELECT),p->connected,blocked);
        prev_raw=s;
    }
    us=sysGetSystemTime();
    if (us-summary_us >= 5000000) {
        say("Summary: physical_start=%u virtual_start=%u core_samples=%u blocked=%u",
            physical_start,core_start,core_calls,blocked_calls);
        summary_us=us;
    }
}
void msx_diag_core_start(int start,int blocked)
{
    if (!active || !target_game) return;
    core_calls++;
    if(blocked) blocked_calls++;
    if(start!=prev_core) {
        if(start) core_start++;
        say("Forwarded START=%d blocked=%d",start,blocked);
        prev_core=start;
    }
}
void msx_diag_end(void)
{
    if(!active) return;
    say("FINISHED: physical_START=%u virtual_START=%u samples=%u blocked=%u",
        physical_start,core_start,core_calls,blocked_calls);
    if(df) { fclose(df); df=NULL; }
    active=0;
}
