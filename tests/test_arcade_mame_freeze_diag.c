/* Metadata-only fixture. Never tests or distributes copyrighted ROM bytes.
 * Verify MAME 0.125 reporting + single TXT overwrite and post-spawn limits. */
#include "arcade_diag.h"
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <stdlib.h>
#include <unistd.h>
#define ROOT "/tmp/arcade_native_route_test/"
static char report[16384];
static void read_report(void) {
    FILE *f=fopen(SF_ARCADE_DIAG_PATH,"rb");
    size_t n;assert(f);
    n=fread(report,1,sizeof(report)-1,f);
    assert(!ferror(f));fclose(f);report[n]=0;
}
int main(void){
    const char *zip=ROOT "kof2002.zip";
    assert(arcade_diag_begin(zip,2,"mame125.self","reload.self","cfg")==0);
    read_report();
    assert(strstr(report,"MAME_BIOS_ANTIGUA_CLAVE=3/3"));
    assert(strstr(report,"MAME_ROM_CRC_ESPERADOS=15"));
    assert(strstr(report,"MAME_ROM_FALTANTES=0"));
    assert(strstr(report,"MAME_ROMSET_ESTADO=METADATOS_CRC_COMPLETOS_NO_GARANTIZA_ARRANQUE"));
    assert(strstr(report,"FASE_POST_100_PORCIENTO=NO_OBSERVABLE_DESDE_SPOOKYFLOW"));
    arcade_diag_event("INTENTANDO_INICIAR_MOTOR_PS3",0,"mame125.self");
    read_report();
    assert(strstr(report,"FASE=INTENTANDO_INICIAR_MOTOR_PS3"));
    assert(arcade_diag_begin(zip,2,"mame125.self","reload.self","cfg")==0);
    read_report();
    assert(!strstr(report,"FASE=INTENTANDO_INICIAR_MOTOR_PS3"));
    assert(rename(ROOT "kof2002.zip",ROOT "kof2002_full_backup.zip")==0);
    assert(rename(ROOT "kof2002_missing.zip",ROOT "kof2002.zip")==0);
    assert(arcade_diag_begin(zip,2,"mame125.self","reload.self","cfg")==0);
    read_report();
    assert(strstr(report,"MAME_ROM_FALTANTES=1"));
    assert(strstr(report,"MAME_ROMSET_ESTADO=DEPENDENCIA_CRC_O_BIOS_FALTANTE"));
    assert(rename(ROOT "kof2002.zip",ROOT "kof2002_missing.zip")==0);
    assert(rename(ROOT "kof2002_full_backup.zip",ROOT "kof2002.zip")==0);
    puts("PASS MAME 0.125 15/15 chip CRC + 3/3 legacy BIOS; one overwritten TXT; missing chip detected; runtime limitations explicit");
    return 0;
}
