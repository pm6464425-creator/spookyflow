/* V15.77: isolated host regression for PS2 horizontal-border report.
 * Zero hardware, no games/firmware modified. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "sf_ps2_video_diag.h"

static void require_text(const char *needle)
{
    char buffer[65536];
    FILE *f=fopen("/tmp/sf1577_ps2_video_diag.txt","rb");
    size_t n;
    if(!f){perror("open PS2 report");exit(2);}
    n=fread(buffer,1,sizeof buffer-1,f);
    fclose(f);
    buffer[n]=0;
    if(!strstr(buffer,needle)){
        fprintf(stderr,"FAIL: missing marker: %s\n",needle);
        exit(3);
    }
}
static void require_absent(const char *needle)
{
    char buffer[65536];
    FILE *f=fopen("/tmp/sf1577_ps2_video_diag.txt","rb");
    size_t n;
    if(!f)exit(4);
    n=fread(buffer,1,sizeof buffer-1,f);
    fclose(f);
    buffer[n]=0;
    if(strstr(buffer,needle)){
        fprintf(stderr,"FAIL: unwanted marker: %s\n",needle);
        exit(5);
    }
}
int main(void)
{
    unsigned char bytes[0x600];
    FILE *f;
    memset(bytes,0,sizeof bytes);
    memcpy(bytes+0x0c,"SLUS21978",9);
    bytes[0x11a]=0x01;
    bytes[0x11b]=0x23;
    bytes[0x11c]=0x02;
    bytes[0x4cc]=0x02;
    bytes[0x4cd]=0x00;
    f=fopen("/tmp/sf1577_ps2bootparam.dat","wb");
    if(!f){perror("bootparam");return 1;}
    if(fwrite(bytes,1,sizeof bytes,f)!=sizeof bytes){fclose(f);return 1;}
    fclose(f);
    sf_ps2_video_diag_begin("/dev_hdd0/PS2ISO/Prueba.iso");
    sf_ps2_video_diag_preflight(-41);
    sf_ps2_video_diag_game("/dev_hdd0/PS2ISO/Prueba.iso","SLUS_219.78");
    sf_ps2_video_diag_final_config(1);
    require_text("DIAG_OBJETIVO=FRANJAS_NEGRAS_HORIZONTALES");
    require_text("BOOTPARAM_COINCIDE_JUEGO=SI");
    require_text("BOOTPARAM_MTIME_UNIX=");
    require_text("PS2_CLASSICS_EBOOT_RAIZ_EXISTE=");
    require_text("HEN_POST_CONFIG_AUSENTE_NO_PRUEBA=");
    sf_ps2_video_diag_resume();
    require_text("JUEGO_ID_PARA_COMPARAR=SLUS_219.78");
    require_text("REAPERTURA_NO_CONFIRMA=QUE_PS2_NETEMU_HAYA_CORRIDO");
    sf_ps2_video_diag_resume(); /* same process: no duplicated report */
    sf_ps2_video_diag_begin("/dev_hdd0/PS2ISO/Siguiente.iso");
    require_absent("REAPERTURA_SPOOKYFLOW=SI");
    require_text("EVIDENCIA_BORDES_REALES=NO_MEDIBLE_DESDE_SPOOKYFLOW");
    remove("/tmp/sf1577_ps2bootparam.dat");
    remove("/tmp/sf1577_ps2_video_diag.txt");
    puts("PASS: 12 PS2 fullscreen-border diagnostic checks");
    return 0;
}
