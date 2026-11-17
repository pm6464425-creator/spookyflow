/* V15.48 pSNES-only audiovisual geometry forensics. Read-only. */
#ifndef SF_VIDEO_DIAG_H
#define SF_VIDEO_DIAG_H
void sf_video_diag_startup(void);
void sf_video_diag_before_snes(const char *rom_path, int config_result);
void sf_video_diag_verified_self(void);
#endif
