/* V15.40: SNES-only cross-process launch forensics.
 * Keep one human-readable diagnostico_snes.txt. A tiny private .bin marker
 * tells the next SpookyFlow start to import pSNES's native boot.log. */
#ifndef SF_SNES_DIAG_H
#define SF_SNES_DIAG_H
/* Call after log_open, before loading the library. */
void sf_snes_diag_on_startup(void);
/* 0 if the durable launch marker was written; negative on I/O error. */
int sf_snes_diag_before_spawn(const char *rom_path);
/* Only if the PS3 spawn API unexpectedly returns to SpookyFlow. */
void sf_snes_diag_spawn_returned(void);
#endif
