/* SNES-only verified Libretro boxart filename matching; no ROM downloads. */
#ifndef SF_SNES_NAME_MATCH_H
#define SF_SNES_NAME_MATCH_H
#include <stddef.h>
#define SNES_BOXART_INDEX "/dev_hdd0/game/SPKF00001/USRDIR/snes_boxart_names.txt"
#define SNES_BOXART_INDEX_MAX (512000u)
int snes_name_catalog_match(const char *rom_path,const char *index,size_t len,
                            char *filename,size_t cap);
int snes_name_catalog_url(const char *image_filename,char *url,size_t cap);
#endif
