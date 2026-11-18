/* V15.52: analyze an ACTUAL screenshot of a running PS1 game.
 * Never probe the Sony emulator GPU from SpookyFlow (different process).
 * No screenshot => explicitly no pixel measurements, never guessed.
 * A screenshot saved by the player can be placed at the documented path
 * using FTP/file-manager.  No other user files are scanned or deleted.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <pngdec/pngdec.h>
#include <sysmodule/sysmodule.h>
#include "ps3_platform.h"
#include "sf_ps1_border_measure.h"
#include "log.h"
#define SF_PS1_SCREENSHOT SF_DATA_DIR "/captura_ps1.png"
static void inspect_one(void)
{
    struct stat st;
    pngData png;
    SfPs1BorderResult b;
    int rc,pr;
    memset(&png,0,sizeof(png));
    if(stat(SF_PS1_SCREENSHOT,&st)!=0) {
        log_printf("ps1_video_diag: PIXEL_MEASUREMENT=NOT_AVAILABLE reason=NO_GAMEPLAY_SCREENSHOT path=" SF_PS1_SCREENSHOT);
        log_printf("ps1_video_diag: REQUIRED_EVIDENCE=PNG_OF_ACTUAL_PS1_GAMEPLAY_NOT_XMB_NOT_BLACK_LOADING_SCREEN");
        return;
    }
    if(st.st_size<80 || st.st_size>24000000) {
        log_printf("ps1_video_diag: PIXEL_MEASUREMENT=REJECTED reason=IMAGE_SIZE size=%llu",
                   (unsigned long long)st.st_size);return;
    }
    /* ps3lib's pngLoadFromFile uses the system pngdec module.
       Failure is recoverable: diagnostics never block launching games. */
    rc=sysModuleLoad(SYSMODULE_PNGDEC);
    log_printf("ps1_video_diag: PNGDEC_LOAD rc=%d (already_loaded_is_ok)",rc);
    pr=pngLoadFromFile(SF_PS1_SCREENSHOT,&png);
    if(pr!=0 || !png.bmp_out) {
        log_printf("ps1_video_diag: PIXEL_MEASUREMENT=REJECTED reason=PNG_DECODE_FAILURE rc=%d",pr);
        if(png.bmp_out)free(png.bmp_out);
        return;
    }
    log_printf("ps1_video_diag: REAL_CAPTURE file=" SF_PS1_SCREENSHOT " bytes=%llu resolution=%ux%u stride=%u",
               (unsigned long long)st.st_size,(unsigned)png.width,(unsigned)png.height,(unsigned)png.pitch);
    rc=sf_ps1_analyze_rgba((const unsigned char*)png.bmp_out,png.width,png.height,png.pitch,&b);
    free(png.bmp_out);
    if(rc!=0 || !b.valid) {
        log_printf("ps1_video_diag: PIXEL_MEASUREMENT=INCONCLUSIVE rc=%d nonblack_center_permille=%u",
                   rc,b.central_nonblack_permille);
        return;
    }
    log_printf("ps1_video_diag: PIXEL_MEASUREMENT=ACTUAL_CAPTURE; output=%ux%u",
               b.width,b.height);
    log_printf("ps1_video_diag: BLACK_BANDS_PIXELS left=%u right=%u top=%u bottom=%u",
               b.left,b.right,b.top,b.bottom);
    log_printf("ps1_video_diag: BLACK_BANDS_PERCENT_TENTHS left=%u right=%u top=%u bottom=%u",
               (unsigned)((1000ULL*b.left)/b.width),
               (unsigned)((1000ULL*b.right)/b.width),
               (unsigned)((1000ULL*b.top)/b.height),
               (unsigned)((1000ULL*b.bottom)/b.height));
    log_printf("ps1_video_diag: ACTIVE_IMAGE_RECT x=%u y=%u width=%u height=%u active_percent_hundredths=%u",
               b.left,b.top,b.active_width,b.active_height,
               (unsigned)((10000ULL*b.active_width*b.active_height)/
                            ((uint64_t)b.width*b.height)));
    log_printf("ps1_video_diag: CAPTURE_CONFIDENCE=%s center_nonblack_permille=%u",
               b.ambiguous?"AMBIGUOUS":"MEASURABLE",b.central_nonblack_permille);
    log_printf("ps1_video_diag: INTERPRETATION=MEASURED_BLACK_PIXELS_ONLY; SOURCE_COULD_BE_PS1_EMULATOR_GAME_ART_OR_CAPTURE; NOT_PROOF_OF_SONY_RENDERER_FAULT");
}
void sf_ps1_screenshot_diagnose(void){inspect_one();}
