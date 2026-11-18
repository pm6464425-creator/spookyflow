/* V15.51 PS1 only. Diagnose Sony's NATIVE PS1 full-screen upscaler and
 * SpookyFlow mount preflight, without changing system flash, PS1 audio,
 * VM1 save cards, BIOS, Cobra, or webMAN parameters. */
#include <stdio.h>
#include <string.h>
#include <sys/file.h>
#include <sysutil/video.h>
#include <tiny3d.h>
#include "ps3_platform.h"
#include "sf_ps1_video_diag.h"
#include "sf_ps1_registry.h"
#include "log.h"

#define PS1_XREGISTRY "/dev_flash2/etc/xRegistry.sys"
static int s_scale=-1;
static void inspect_scaler(const char *stage)
{
    int smoothing=-1,rc;
    rc=sf_ps1_registry_read_modes(PS1_XREGISTRY,&s_scale,&smoothing);
    log_printf("ps1_video_diag: [%s] XREGISTRY_READONLY=YES STATUS=%d (0=READ_OK,-1=ACCESS,-2=FORMAT,-3=KEY_ABSENT,-4=VALUE_INVALID)",stage,rc);
    log_printf("ps1_video_diag: [%s] KEY=/setting/game/emuUpConvert VALUE=%d MODE=%s SMOOTHING=%d",
               stage,s_scale,sf_ps1_upscaler_name(s_scale),smoothing);
    if(rc==0 && s_scale==2)
        log_printf("ps1_video_diag: [%s] PS1_16_9_SETTING=VERIFIED_FULL_SCREEN (still not a measurement of emulator pixels)",stage);
    else if(rc==0)
        log_printf("ps1_video_diag: [%s] PS1_16_9_SETTING=NOT_FULL; FIX_ON_PS3_XMB=AJUSTES_AJUSTES_DE_JUEGO_PS_MEJORADOR_DE_RESOLUCION_PANTALLA_COMPLETA",stage);
    else
        log_printf("ps1_video_diag: [%s] PS1_16_9_SETTING=UNKNOWN; DO_NOT_ASSUME_FULLSCREEN_OR_CHANGE_REGISTRY",stage);
}
static void inspect_video(const char *stage)
{
    videoState state;
    videoResolution resolution;
    int state_rc,res_rc=-1;
    memset(&state,0,sizeof(state));
    memset(&resolution,0,sizeof(resolution));
    state_rc=videoGetState(0,0,&state);
    if(state_rc==0)
        res_rc=videoGetResolution(state.displayMode.resolution,&resolution);
    log_printf("ps1_video_diag: [%s] SPOOKYFLOW_VIDEO_STATE_rc=%d VIDEO_RESOLUTION_rc=%d resolution_id=%u width=%u height=%u aspect=%u",
             stage,state_rc,res_rc,(unsigned)state.displayMode.resolution,
             (unsigned)resolution.width,(unsigned)resolution.height,
             (unsigned)state.displayMode.aspect);
    log_printf("ps1_video_diag: [%s] SPOOKYFLOW_TINY3D=%ux%u aspect=%d (not the PS1 emulator's framebuffer)",
             stage,(unsigned)Video_Resolution.width,
             (unsigned)Video_Resolution.height,(int)Video_aspect);
    if(!state_rc && !res_rc && resolution.width<1280)
        log_printf("ps1_video_diag: [%s] WARNING=HDMI_HD_MODE_REQUIRED_FOR_SONY_PS1_UPSCALER; REQUIRE_720P_1080I_1080P",stage);
    else if(!state_rc && !res_rc)
        log_printf("ps1_video_diag: [%s] PS3_HD_OUTPUT=CAPABLE; PS1_RUNTIME_DISPLAY_NOT_OBSERVABLE_FROM_LAUNCHER",stage);
}
void sf_ps1_video_diag_startup(void)
{
    log_printf("ps1_video_diag: VERSION=V15.51 FORENSIC_TARGET=PS1_NATIVE_16_9");
    log_printf("ps1_video_diag: SOURCE=SONY_PS3_GAME_SETTINGS_PS_UPSCALER");
    log_printf("ps1_video_diag: SYSTEM_UPSCALER=0_OFF_1_NORMAL_4_3_2_FULL_SCREEN_16_9");
    log_printf("ps1_video_diag: SAFETY=NO_FLASH_WRITE_NO_REGISTRY_UPDATE_NO_PS1_EMULATOR_PATCH");
    log_printf("ps1_video_diag: SCOPE=PS1_NATIVE_COBRA_AND_WEBMAN_FALLBACK; OTHER_PLATFORMS_UNMODIFIED");
    log_printf("ps1_video_diag: LIMITATION=PS1_EMULATOR_RUNS_AFTER_XMB_HANDOFF; NO_DIRECT_GAME_FRAMEBUFFER_OR_HDMI_PIXEL_MEASURE");
    inspect_video("APP_READY");
    inspect_scaler("APP_READY");
}
void sf_ps1_video_diag_before_launch(const char *path,int emulator,const char *backend)
{
    sysFSStat st;
    int stat_rc=-1;
    const char *ext, *name;
    log_printf("ps1_video_diag: -------- PS1_FORENSIC_CASE_BEGIN --------");
    log_printf("ps1_video_diag: LAUNCH backend=%s emulator_preference=%d",backend?backend:"UNKNOWN",emulator);
    if(path) {
        size_t len=strlen(path);
        name=strrchr(path,'/');name=name?name+1:path;
        ext=strrchr(name,'.');ext=ext?ext:"(none)";
        stat_rc=sysFsStat(path,&st);
        log_printf("ps1_video_diag: IMAGE path=%.320s extension=%.32s path_length=%u stat_rc=%d bytes=%llu",
                   path,ext,(unsigned)len,stat_rc,stat_rc==0?(unsigned long long)st.st_size:0ull);
        if(strstr(path,".ntfs[PSXISO]"))
            log_printf("ps1_video_diag: IMAGE_TYPE=NTFS_INDEX; SYSFS_STAT_MAY_NOT_RESOLVE_INDIRECT_PATH");
    } else {
        log_printf("ps1_video_diag: IMAGE=JOINED_CUE_CACHE (path managed inside sf_ps1_join)");
    }
    inspect_scaler("PS1_BEFORE_MOUNT");
    inspect_video("PS1_BEFORE_MOUNT");
    log_printf("ps1_video_diag: LAUNCH_POLICY=NO_FORCE_CHANGE_TO_PS1_EMULATOR_GLOBAL_XMB_SETTING; SETTING_APPLIES_TO_ALL_PS1_TITLES");
}
void sf_ps1_video_diag_launch_result(const char *backend,int rc)
{
    log_printf("ps1_video_diag: LAUNCH_STAGE backend=%s result_rc=%d status=%s",
               backend?backend:"UNKNOWN",rc,rc==0?"MOUNT_ACCEPTED":"ERROR_OR_FALLBACK");
    if(rc==0)
        log_printf("ps1_video_diag: HANDOFF=RETURN_TO_XMB_AND_LAUNCH_SONY_PS1_EMU; ACTUAL_EMU_RESOLUTION_UNKNOWN");
    else
        log_printf("ps1_video_diag: HANDOFF=NOT_CONFIRMED; SEE_PS1_DIAG_MOUNT_STAGES_FOR_ROOT_CAUSE");
}
void sf_ps1_video_diag_webman(const char *request,int rc)
{
    log_printf("ps1_video_diag: WEBMAN_FALLBACK_TRANSPORT result_rc=%d request_prefix=%.180s",rc,request?request:"(null)");
    if(rc!=0)log_printf("ps1_video_diag: WEBMAN_FALLBACK=TRANSPORT_FAILED; NOT_AN_UPSCALER_ERROR");
}
