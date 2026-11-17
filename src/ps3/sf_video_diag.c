/* V15.48 forensic SNES-video measurements from the running PS3 app.
 * Does not claim to observe the following pSNES process or TV HDMI pixels.
 * All data is logged into the ONE persistent video report.
 */
#include <stdio.h>
#include <string.h>
#include <sys/file.h>
#include <sysutil/video.h>
#include <tiny3d.h>
#include "ps3_platform.h"
#include "sf_snes_screen.h"
#include "sf_video_diag.h"
#include "sf_psnes_plain_hash.h"
#include "log.h"

#define SF_NATIVE_CONFIG "/dev_hdd0/game/PSNE00001/USRDIR/config.cfg"
#define SF_NATIVE_GAMECFG "/dev_hdd0/game/PSNE00001/USRDIR/configs/"

static void report_output(const char *phase)
{
    videoState state;
    videoResolution actual;
    int state_rc, res_rc=-1;
    memset(&state,0,sizeof(state));
    memset(&actual,0,sizeof(actual));
    state_rc=videoGetState(0,0,&state);
    if(state_rc==0)
        res_rc=videoGetResolution(state.displayMode.resolution,&actual);
    log_printf("video_diag: [%s] PS3_VIDEO_STATE rc=%d resolved_rc=%d resolution_id=%u mode=%ux%u aspect_id=%u",
               phase,state_rc,res_rc,(unsigned)state.displayMode.resolution,
               (unsigned)actual.width,(unsigned)actual.height,
               (unsigned)state.displayMode.aspect);
    log_printf("video_diag: [%s] TINY3D_CURRENT width=%u height=%u aspect_id=%d (this_is_spookyflow_not_psnes)",
               phase,(unsigned)Video_Resolution.width,(unsigned)Video_Resolution.height,(int)Video_aspect);
    if(!state_rc && !res_rc && 
        (actual.width!=Video_Resolution.width || actual.height!=Video_Resolution.height))
        log_printf("video_diag: [%s] WARNING=VIDEO_STATE_DIFFERS_FROM_TINY3D",phase);
}

static void log_config(const char *kind, const char *path)
{
    char result[256]={0};
    int rc=sf_snes_config_video_probe(path,result,sizeof(result));
    log_printf("video_diag: CFG kind=%s probe_rc=%d path=%.230s %s",
               kind,rc,path?path:"(null)",result);
}

void sf_video_diag_startup(void)
{
    log_printf("video_diag: VERSION=V15.48 FORENSIC_TARGET=SNES_BLACK_BORDERS");
    log_printf("video_diag: DATA_BOUNDARY=PS3_PROCESS_ONLY; next_process_pSNES_metrics_need_instrumentation");
    log_printf("video_diag: OBSERVABLE=PS3_video_state,Tiny3D_resolution,SELF_SHA,config_on_disk,pSNES_boot_log");
    log_printf("video_diag: NOT_OBSERVABLE=TV_active_pixels,pSNES_GPU_viewport,per_frame_SNES_buffer,HDMI_overscan");
    log_printf("video_diag: ENGINE_ELF_SHA256_INTERMEDIATE=" SF_PSNES_NATIVE_169_ELF_SHA256);
    log_printf("video_diag: ENGINE_ELF_SHA256_FINAL=" SF_PSNES_NATIVE_BORDER_FILL_ELF_SHA256);
    log_printf("video_diag: ENGINE_SELF_SHA256_EXPECTED=" SF_PSNES_APP_SELF_SHA256);
    log_printf("video_diag: SOURCE_RENDERER_LOGICAL_SIZE=848x512 SOURCE_ORIGIN=libcross2d_TINY3DRenderer");
    log_printf("video_diag: SOURCE_EXPECTED_FULLSCREEN_METHOD=C2DUIVideo_updateScaling_FULL");
    report_output("SPOOKYFLOW_READY");
}

void sf_video_diag_before_snes(const char *rom_path, int config_result)
{
    const char *name,*dot;
    size_t base;
    char gamecfg[768];
    sysFSStat st;
    int stat_rc;
    log_printf("video_diag: ---------- FORENSIC_CASE_BEGIN ----------");
    log_printf("video_diag: PRESPAWN_CONFIG_WRITER_RESULT=%d",config_result);
    stat_rc=rom_path?sysFsStat(rom_path,&st):-1;
    log_printf("video_diag: ROM stat_rc=%d size=%llu path=%.350s",
               stat_rc,stat_rc?0ULL:(unsigned long long)st.st_size,
               rom_path?rom_path:"(null)");
    log_config("GLOBAL",SF_NATIVE_CONFIG);
    if(rom_path) {
        name=strrchr(rom_path,'/');
        name=name?name+1:rom_path;
        dot=strrchr(name,'.');
        base=dot?(size_t)(dot-name):strlen(name);
        if(base>0 && base<450 && !strchr(name,'\\') && !strchr(name,'\r') && !strchr(name,'\n') &&
           snprintf(gamecfg,sizeof(gamecfg),SF_NATIVE_GAMECFG "%.*s.cfg",
               (int)base,name)<(int)sizeof(gamecfg)) {
            log_config("PER_ROM",gamecfg);
        } else {
            log_printf("video_diag: CFG kind=PER_ROM status=PATH_INVALID");
        }
    }
    report_output("SNES_PRESPAWN");
    if(Video_Resolution.width==1280 && Video_Resolution.height==720) {
        log_printf("video_diag: PATCH_MODE=720P geometry=expected_x_-40_y_-24 scaled_1360x768 viewport_target_1280x720");
        log_printf("video_diag: PROJECTION_INTENT=center_crop_40px_horizontal_24px_vertical; NOT_A_PIXEL_OBSERVATION");
    } else if(Video_Resolution.width==1920 && Video_Resolution.height==1080) {
        log_printf("video_diag: PATCH_MODE=1080P geometry=expected_x_-60_y_-24 scaled_2040x1128 viewport_target_1920x1080");
        log_printf("video_diag: PROJECTION_INTENT=center_crop_60px_horizontal_24px_vertical; NOT_A_PIXEL_OBSERVATION");
    } else {
        log_printf("video_diag: PATCH_MODE=OTHER resolution=%ux%u overscan_correction_not_applicable_or_unknown",
                   (unsigned)Video_Resolution.width,(unsigned)Video_Resolution.height);
    }
    log_printf("video_diag: STATIC_PROJECTION_NOT_PROOF_OF_RUNTIME_PIXEL_COVERAGE=YES");
}

void sf_video_diag_verified_self(void)
{
    log_printf("video_diag: APP_SELF_RUNTIME_SHA256_VERIFIED=YES type4=APP expected_size=%llu",
               (unsigned long long)PSNES_PLAIN_SIZE);
    log_printf("video_diag: CANNOT_ASSERT_ACTUAL_FRAME_BORDER_FROM_SIGNATURE=YES");
}
