#ifndef SF_PS1_VIDEO_DIAG_H
#define SF_PS1_VIDEO_DIAG_H
void sf_ps1_video_diag_startup(void);
void sf_ps1_video_diag_before_launch(const char *path,int emulator,const char *backend);
void sf_ps1_video_diag_launch_result(const char *backend,int rc);
void sf_ps1_video_diag_webman(const char *request,int rc);
#endif
