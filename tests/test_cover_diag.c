#include "cover_diag.h"
#include <stdio.h>
#include <string.h>
int main(void)
{
    FILE *f;
    char text[4096];
    size_t n;
    const char *file="build-host/diagnostico_portadas_test.txt";
    if(cover_diag_start(file)!=0)return 2;
    cover_diag("INICIO","26 BDISO films");
    cover_diag("RUTA","JPG=%s","/dev_usb000/BDISO/COVERS/Avatar 3D.jpg");
    cover_diag("DECOD_ERROR","rc=%d %s",-7,"JPEG_PROGRESSIVE");
    cover_diag("TEXTURA_OK","slot=%d tex=%d",3,23);
    cover_diag_stop();
    f=fopen(file,"rb");if(!f)return 3;
    n=fread(text,1,sizeof(text)-1,f);fclose(f);
    text[n]=0;
    if(!strstr(text,"SPOOKYFLOW - DIAGNOSTICO DE PORTADAS BDISO") ||
       !strstr(text,"DECOD_ERROR") || !strstr(text,"TEXTURA_OK") ||
       !strstr(text,"Avatar 3D.jpg") || !strstr(text,"FIN DEL INFORME"))
        return 4;
    printf("PASS: isolated BDISO report writes, persists and includes all stage markers\n");
    return 0;
}
