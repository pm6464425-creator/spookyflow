/* Regression: V15.58 originally crashed on old settings.ini:
 * k_plat_suffix[5] was NULL and strcmp() dereferenced it.
 * Test existing categories plus the new arcade category. */
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "settings.h"
int main(void)
{
    Settings s, again;
    static const char *const labels[]={"ps1","ps2","ps3","bdiso","snes","arcade"};
    int p;
    settings_defaults(&s);
    for(p=0;p<PLAT_COUNT;p++){
        char line[160],key[64];
        snprintf(key,sizeof key,"SavedArcade%d",p);
        snprintf(line,sizeof line,"last_key_%s=%s",labels[p],key);
        settings_parse_line(&s,line);
        assert(strcmp(s.last_key[p],key)==0);
    }
    settings_parse_line(&s,"last_tab=5");
    assert(s.last_tab==PLAT_ARCADE);
    assert(settings_save(&s,"/tmp/spookyflow_arcade_settings_sixth.txt")==0);
    settings_defaults(&again);
    assert(settings_load(&again,"/tmp/spookyflow_arcade_settings_sixth.txt")==0);
    for(p=0;p<PLAT_COUNT;p++){
        char key[64];
        snprintf(key,sizeof key,"SavedArcade%d",p);
        assert(strcmp(again.last_key[p],key)==0);
    }
    assert(again.last_tab==PLAT_ARCADE);
    remove("/tmp/spookyflow_arcade_settings_sixth.txt");
    puts("PASS: six platforms, existing settings compatible, no NULL dereference");
    return 0;
}
