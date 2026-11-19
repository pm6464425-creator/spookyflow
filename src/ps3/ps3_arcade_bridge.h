/* Optional trusted-LAN Arcade PC host fallback. Never touches user ROM/BIOS.
 * Safe when disabled: no sockets opened, native launch behavior preserved. */
#ifndef SF_PS3_ARCADE_BRIDGE_H
#define SF_PS3_ARCADE_BRIDGE_H
#include <stddef.h>
/* 1=accepted by PC host; 0=feature disabled; negative=PC unavailable/failure. */
int sf_pc_arcade_try_launch(const char *rom_path,char *reason,size_t cap);
#endif
