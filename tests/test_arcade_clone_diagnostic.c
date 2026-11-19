/* Exact user-reported kf2k2mp.zip 3 files + kof2002.zip 14 files.
 * No ROM bytes: metadata-only fake ZIPs fabricated for regression tests. */
#include "arcade_diag.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define ROOT "/tmp/arcade_native_route_test/"
static char report[20000];
static void load(void)
{
    FILE *f=fopen(SF_ARCADE_DIAG_PATH,"rb");
    size_t n;
    assert(f);
    n=fread(report,1,sizeof report-1,f);
    assert(!ferror(f));
    fclose(f);report[n]=0;
}
int main(void)
{
    const char *rom=ROOT "kf2k2mp.zip";
    assert(arcade_diag_begin(rom,3,"fb_neo_legacy.self","reload.self","cfg")==0);
    load();
    assert(strstr(report,"SPOOKYFLOW V15.73"));
    assert(strstr(report,"JUEGO_ENTRADAS=3/3"));
    assert(strstr(report,"PADRE_ZIP=" ROOT "kof2002.zip"));
    assert(strstr(report,"BIOS_ZIP=" ROOT "neogeo.zip"));
    assert(strstr(report,"MAME_BIOS_ANTIGUA_CLAVE=3/3"));
    assert(strstr(report,"MAME_ROM_CRC_ESPERADOS=14"));
    assert(strstr(report,"MAME_ROM_CRC_JUEGO_OK=3"));
    assert(strstr(report,"MAME_ROM_CRC_PADRE_OK=11"));
    assert(strstr(report,"MAME_ROM_FALTANTES=0"));
    assert(strstr(report,"VALIDACION_PADRE_CLON=CLON_CON_PADRE_CRC_COMPLETOS"));
    assert(strstr(report,"CRC_DE_CABECERA_NO_COMPRUEBA_DESCOMPRESION_INTEGRAL"));
    arcade_diag_event("PC_ARCADE_FALLBACK",0,"PC_BRIDGE_DESACTIVADO");
    load();
    assert(strstr(report,"FASE=PC_ARCADE_FALLBACK"));
    /* Next selection replaces the one authoritative TXT. */
    assert(arcade_diag_begin(rom,0,"(null)","reload.self","cfg")==0);
    load();
    assert(!strstr(report,"FASE=PC_ARCADE_FALLBACK"));
    assert(strstr(report,"REVISION_ADICIONAL=MAME0125_PARENT_CLONE_CRC_AUDIT"));
    assert(strstr(report,"MAME_ROM_CRC_PADRE_OK=11"));
    /* A truly absent parent must NOT be silently accepted. */
    assert(rename(ROOT "kof2002.zip",ROOT "kof2002_diag_backup.zip")==0);
    assert(arcade_diag_begin(rom,0,"(null)","reload.self","cfg")==0);
    load();
    assert(strstr(report,"PADRE_ESTADO=NO_ABRE_ZIP_O_CENTRAL_DIRECTORY_DANADA"));
    assert(strstr(report,"MAME_ROM_FALTANTES=11"));
    assert(rename(ROOT "kof2002_diag_backup.zip",ROOT "kof2002.zip")==0);
    puts("PASS user kf2k2mp: child=3 parent=11 BIOS=3/3; overwritten TXT; missing-parent detected accurately");
    return 0;
}
