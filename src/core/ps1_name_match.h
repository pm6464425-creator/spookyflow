/* PS1 cover title matching against a REAL bundled Libretro cover index.
 * Portable, deterministic and read-only. No ROM file fetching. */
#ifndef SF_PS1_NAME_MATCH_H
#define SF_PS1_NAME_MATCH_H
#include <stddef.h>
#define PS1_BOXART_INDEX "/dev_hdd0/game/SPKF00001/USRDIR/ps1_boxart_names.txt"
#define PS1_BOXART_INDEX_MAX (1500000u)
int ps1_name_catalog_match(const char *game_path,const char *index,size_t len,
                           char *image_filename,size_t cap);
int ps1_name_catalog_url(const char *image_filename,char *url,size_t cap);
#endif
