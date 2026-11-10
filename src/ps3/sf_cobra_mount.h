#ifndef SF_COBRA_MOUNT_H
#define SF_COBRA_MOUNT_H
/* Experimental: mounts a PS3 game ISO or directory with Cobra/HEN. 
 * Return 0 on success; nonzero error code on failure. */
int sf_cobra_mount_ps3(const char *path, int is_folder, const char *title_id);
void sf_cobra_diag_report_previous(void);
#endif
