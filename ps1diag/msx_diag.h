#ifndef MSX_DIAG_H
#define MSX_DIAG_H
#include "common.h"
void msx_diag_clean_previous(void);
void msx_diag_boot(const char *launch_path);
void msx_diag_begin(const char *name, const char *path);
int msx_diag_active(void);
void msx_diag_virtual_pad(const char *type);
void msx_diag_poll(const pad_state_t *pad, int input_blocked);
void msx_diag_core_start(int active, int blocked);
void msx_diag_end(void);
#endif
