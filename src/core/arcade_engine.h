/* Arcade auto-choice: ORIGINAL PS3 FBNeoRLPlus v2.30 compatibility DB.
 * No contact with other systems, no ROM renaming. */
#ifndef SF_ARCADE_ENGINE_H
#define SF_ARCADE_ENGINE_H
enum { ARCADE_ENGINE_NONE=0, ARCADE_ENGINE_FBNEO=1, ARCADE_ENGINE_MAME125=2, ARCADE_ENGINE_FBNEO_LEGACY=3 };
#ifndef ARCADE_ENGINE_MAP_PATH
#define ARCADE_ENGINE_MAP_PATH "/dev_hdd0/game/SPKF00001/USRDIR/ARCADE/arcade_engines.tsv"
#endif
int arcade_engine_for_rom(const char *path);
int arcade_engine_list_count(void);
#endif
