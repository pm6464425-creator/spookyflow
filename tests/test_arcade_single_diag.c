/* Arcade single-file pre-launch CRC audit. Test ZIP creation is separate Python. */
#include "arcade_diag.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static void readreport(char *buf,size_t n)
{
    FILE *f=fopen(SF_ARCADE_DIAG_PATH,"rb");
    size_t got;
    assert(f);
    got=fread(buf,1,n-1,f);
    fclose(f);
    buf[got]=0;
}
int main(int argc,char **argv)
{
    char *text=(char *)malloc(65000);
    const char *zip;
    int i;
    assert(argc>=3);
    assert(text);
    zip=argv[1];
    assert(arcade_diag_begin(zip,1,argv[2],argv[2],argv[2])==0);
    readreport(text,65000);
    assert(strstr(text,"MOTOR_ELEGIDO=FBNeo"));
    assert(strstr(text,"JUEGO_ESTADO=ZIP_CENTRAL_DIRECTORY_OK"));
    assert(strstr(text,"MANIFEST_VERSION=FBNeo_PS3_fccba95f2"));
    assert(strstr(text,"MANIFEST_CRC_INCORRECTOS=1"));
    assert(strstr(text,"MANIFEST_ROMS_FALTANTES=1"));
    assert(strstr(text,"RESULTADO=CRC_DISTINTO_ROMSET_INCOMPATIBLE_PARA_FBNEO_PS3"));
    arcade_diag_event("INTENTANDO_INICIAR_MOTOR_PS3",0,"fbneo.self");
    readreport(text,65000);
    assert(strstr(text,"FASE=INTENTANDO_INICIAR_MOTOR_PS3"));
    assert(strstr(text,"CODIGO_LANZAMIENTO=0"));
    /* The report must be overwritten; no versioned txt accumulation. */
    for(i=0;i<2;i++){
        assert(arcade_diag_begin(zip,1,argv[2],argv[2],argv[2])==0);
        readreport(text,65000);
        assert(!strstr(text,"FASE=INTENTANDO_INICIAR_MOTOR_PS3"));
        assert(strstr(text,"MANIFEST_ROMS_FALTANTES=1"));
    }
    puts("PASS: Wrong romset CRC+missing, one overwritten TXT, zip unchanged");
    free(text);
    return 0;
}
