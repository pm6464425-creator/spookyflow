#ifndef SF_PS2_VIDEO_DIAG_H
#define SF_PS2_VIDEO_DIAG_H
/* V15.75: PS2-only, single bounded forensic video report. No system writes. */
#define SF_PS2_VIDEO_DIAG_PATH "/dev_hdd0/tmp/spookyflow/diagnostico_pantalla_ps2.txt"
void sf_ps2_video_diag_begin(const char *iso_path);
void sf_ps2_video_diag_preflight(int preflight_rc);
void sf_ps2_video_diag_game(const char *iso_path,const char *game_id);
void sf_ps2_video_diag_event(const char *phase,int rc,const char *detail);
void sf_ps2_video_diag_resume(void);
#endif
