/* SNES-only, portable ROM staging and webMAN launch request builder.
 * Does not modify PS1/PS2/PS3/BDISO launch rules. */
#ifndef SF_SNES_LAUNCHER_H
#define SF_SNES_LAUNCHER_H

#include <stddef.h>

#define SF_SNES_STAGE_DIR "/dev_hdd0/ROMS/SNES9X2010"
#define SF_SNES_MAX_ROM_BYTES (64u * 1024u * 1024u)

/* Only ROMs from the two roots already scanned by SpookyFlow are allowed. */
int snes_launcher_is_rom(const char *source);

/* Deterministic short staging filename: stable across launches for saves. */
int snes_launcher_stage_path(const char *source, char *out, size_t cap);

/* webMAN multi-command. Its /ROMS/SNES9X2010/ path selects Snes9x 2010.
 * wm_autoplay=1 avoids a duplicate explicit /play.ps3 command. */
int snes_launcher_request(const char *staged, int wm_autoplay,
                          char *out, size_t cap);

#endif
