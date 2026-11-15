/* V15.42 pSNES 7.2: enforce FULL 16:9 without touching original ROMs.
 * No changes to PS1/PS2/PS3/BDISO, pSNES binary, audio or controls. */
#ifndef SF_SNES_SCREEN_H
#define SF_SNES_SCREEN_H
/* Return 0 on success (or optional config not present); negative on error.
 * File must be a pSNES/libconfig config with an EMULATION group.
 * Only EMULATION.SCALING is changed, all other settings are preserved. */
int sf_snes_config_set_full(const char *config_path, int create_if_missing);
/* Runs before process spawn; touches global and selected per-ROM pSNES cfg.
 * pSNES main() copies global EMULATION, then applies per-game overrides. */
int sf_snes_force_widescreen(const char *rom_path);
#endif
