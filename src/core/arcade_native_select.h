/* ARCADE ONLY: choose isolated FBNeo legacy for verified KOF2002 old ROMset,\n * MAME 0.125 for other verified legacy Neo Geo ROMs+BIOS.
 * Keeps FBNeo for modern ROMsets. Never modifies or repairs user ZIP files.
 * Uses exact original MAME 0.125 driver CRC+size requirements.
 */
#ifndef SF_ARCADE_NATIVE_SELECT_H
#define SF_ARCADE_NATIVE_SELECT_H
#include <stddef.h>
#define SF_MAME125_NEOGEO_CATALOG "/dev_hdd0/game/SPKF00001/USRDIR/ARCADE/mame0125_neogeo.tsv"
int arcade_native_engine_select(const char *game_path,int original_engine,
                                char *reason,size_t reason_cap);
#endif
