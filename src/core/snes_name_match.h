/* SNES-only verified Libretro boxart filename matching; no ROM downloads. */
#ifndef SF_SNES_NAME_MATCH_H
#define SF_SNES_NAME_MATCH_H
#include <stddef.h>
#define SNES_BOXART_INDEX "/dev_hdd0/game/SPKF00001/USRDIR/snes_boxart_names.txt"
#define SNES_BOXART_INDEX_MAX (512000u)
#define SNES_TITLES_INDEX "/dev_hdd0/game/SPKF00001/USRDIR/snes_title_names.txt"
#define SNES_LOGOS_INDEX "/dev_hdd0/game/SPKF00001/USRDIR/snes_logo_names.txt"
#define SNES_SNAPS_INDEX "/dev_hdd0/game/SPKF00001/USRDIR/snes_snap_names.txt"
enum { SNES_ART_BOX=0, SNES_ART_TITLE=1, SNES_ART_LOGO=2, SNES_ART_SNAP=3, SNES_ART_KINDS=4 };
int snes_name_catalog_match(const char *rom_path,const char *index,size_t len,
                            char *filename,size_t cap);
int snes_name_catalog_url(const char *image_filename,char *url,size_t cap);
int snes_name_catalog_url_kind(const char *image_filename,int kind,char *url,size_t cap);
#endif
