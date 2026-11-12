#ifndef SF_COBRA_MOUNT_H
#define SF_COBRA_MOUNT_H
/* Experimental: mounts a PS3 game ISO or directory with Cobra/HEN. 
 * Return 0 on success; nonzero error code on failure. */
int sf_cobra_mount_ps3(const char *path, int is_folder, const char *title_id);
int sf_cobra_mount_bd_iso(const char *path);
void sf_cobra_diag_report_previous(void);
/* Initial checkpoint: runs before Tiny3D or XMB render initialization. */
int sf_cobra_diag_startup(const char *stage);
/* Pre-mount checkpoint: must complete before any Cobra syscall. */
int sf_cobra_diag_selection(const char *path, int folder);
#endif
