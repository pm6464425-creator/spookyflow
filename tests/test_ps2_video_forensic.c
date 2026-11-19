/* V15.75: host regression for PS2 video-only report, valid for ANY ISO ID.
 * No PS2 game / BIOS / keys required. Test PS3bootparam is synthetic and
 * explicitly contains fake credentials, which must NEVER leak into TXT.
 */
#include "sf_ps2_video_diag.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifndef SF_PS2_VIDEO_BOOTPARAM_PATH
#error "provide synthetic bootparam path"
#endif
static char report[50000];
static size_t readreport(void)
{
    FILE *f=fopen(SF_PS2_VIDEO_DIAG_PATH,"rb");
    size_t n;assert(f);
    n=fread(report,1,sizeof report-1,f);report[n]=0;
    assert(!ferror(f));fclose(f);return n;
}
int main(void)
{
    FILE *f;
    unsigned char boot[0x600]={0};
    const char *password="DO_NOT_LEAK_FAKE_WIFI_PASSWORD_2026";
    memcpy(boot+0x0c,"SLUS20312",9);
    memcpy(boot+0x54,password,strlen(password));
    boot[0x11a]=0x02;boot[0x11b]=0x10;boot[0x11c]=0x01;
    boot[0x4cc]=2;boot[0x4cd]=1;
    f=fopen(SF_PS2_VIDEO_BOOTPARAM_PATH,"wb");assert(f);
    assert(fwrite(boot,1,sizeof boot,f)==sizeof boot);fclose(f);
    f=fopen("/tmp/ps2_video_test_game.iso","wb");assert(f);
    assert(fseek(f,0x20000,SEEK_SET)==0);assert(fputc(0,f)!=EOF);fclose(f);
    sf_ps2_video_diag_begin("/tmp/ps2_video_test_game.iso");
    sf_ps2_video_diag_preflight(-41);
    sf_ps2_video_diag_game("/tmp/ps2_video_test_game.iso","SLUS_203.12");
    sf_ps2_video_diag_event("ANTES_DE_PS2CLASSICS",0,"NO_SE_MODIFICA_EL_CONFIG");
    assert(readreport()<18000);
    assert(strstr(report,"SPOOKYFLOW_PS2_VIDEO_DIAG=V15.75"));
    assert(strstr(report,"MOTOR_PREVISTO=HEN_PS2_CLASSICS_PS2_NETEMU"));
    assert(strstr(report,"BOOTPARAM_ESCALADOR=2"));
    assert(strstr(report,"BOOTPARAM_ESCALADOR_INTERPRETACION=FULL_ESCALADO_16_9_ESTIRADO"));
    assert(strstr(report,"BOOTPARAM_COINCIDE_JUEGO=SI"));
    assert(strstr(report,"BOOTPARAM_PORT_TIPO=1"));
    assert(strstr(report,"PS3_VIDEO_ESTADO=HOST_TEST_NO_HARDWARE"));
    assert(!strstr(report,password));
    assert(strstr(report,"MODIFICA_ROM=NO"));
    /* Returning after PS2 Classics app appends bootparam once per startup. */
    sf_ps2_video_diag_resume();
    sf_ps2_video_diag_resume();
    assert(readreport()<20000);
    assert(strstr(report,"REAPERTURA_SPOOKYFLOW=SI"));
    assert(!strstr(report,password));
    /* Selecting a different PS2 game replaces the report; no old logs. */
    sf_ps2_video_diag_begin("/tmp/ps2_video_another_game.iso");
    sf_ps2_video_diag_preflight(0);
    sf_ps2_video_diag_game("/tmp/ps2_video_another_game.iso","SLES_123.45");
    assert(readreport()<16000);
    assert(strstr(report,"MOTOR_PREVISTO=COBRA_PS2_EMULATOR_SEGUN_CONFIG"));
    assert(strstr(report,"BOOTPARAM_COINCIDE_JUEGO=NO_O_DESCONOCIDO"));
    assert(!strstr(report,"ANTES_DE_PS2CLASSICS"));
    for(int i=0;i<2000;i++)
        sf_ps2_video_diag_event("RUN",0,"BOUNDED_LOG_WITH_NO_GROWTH_BEYOND_CAP");
    assert(readreport()<=48*1024);
    assert(!strstr(report,password));
    puts("PASS PS2 V15.75: last-only TXT, >1 game, 16:9 bootparam, HDMI/output, exact ID, no secrets, cap");
    return 0;
}
