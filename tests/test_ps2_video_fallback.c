/* V15.76 regression: when /dev_hdd0/tmp/spookyflow is not writable,
 * an alternate single forensic TXT must be created and remain on restart.
 * Run with synthetic bootparam file and missing primary parent directory.
 */
#include "sf_ps2_video_diag.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
int main(void)
{
    char buf[12000];size_t n;
    FILE *f;
    sf_ps2_video_diag_begin("/dev_hdd0/PS2ISO/example.iso");
    assert(sf_ps2_video_diag_status()==2);
    assert(strcmp(sf_ps2_video_diag_active_path(),
                  SF_PS2_VIDEO_DIAG_FALLBACK_PATH)==0);
    sf_ps2_video_diag_preflight(-41);
    sf_ps2_video_diag_game("/dev_hdd0/PS2ISO/example.iso","SLUS_201.74");
    f=fopen(SF_PS2_VIDEO_DIAG_FALLBACK_PATH,"rb");
    assert(f);
    n=fread(buf,1,sizeof buf-1,f);buf[n]=0;fclose(f);
    assert(strstr(buf,"SPOOKYFLOW_PS2_VIDEO_DIAG=V15.76"));
    assert(strstr(buf,"DIAGNOSTICO_GUARDADO=ALTERNATIVA"));
    assert(strstr(buf,"MOTOR_PREVISTO=HEN_PS2_CLASSICS_PS2_NETEMU"));
    assert(strstr(buf,"JUEGO_ID=SLUS_201.74"));
    assert(strstr(buf,"MODIFICA_XREGISTRY=NO"));
    sf_ps2_video_diag_resume();
    f=fopen(SF_PS2_VIDEO_DIAG_FALLBACK_PATH,"rb");assert(f);
    n=fread(buf,1,sizeof buf-1,f);buf[n]=0;fclose(f);
    assert(strstr(buf,"REAPERTURA_SPOOKYFLOW=SI"));
    puts("PASS V15.76 PS2 forensic fallback when primary directory is absent");
    return 0;
}