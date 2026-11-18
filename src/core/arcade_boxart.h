/* Arcade-only verified thumbnail index. Never used by PS1/PS2/PS3/BDISO/SNES. */
#ifndef SF_ARCADE_BOXART_H
#define SF_ARCADE_BOXART_H
#include <stddef.h>
#include <stdint.h>
#include "coverart.h"

#define ARCADE_BOXART_INDEX "/dev_hdd0/game/SPKF00001/USRDIR/ARCADE/arcade_boxart_index.tsv"
#define ARCADE_BOXART_MAX_BYTES (2u * 1024u * 1024u)
#define ARCADE_BOXART_MAX_ROWS 16000
typedef struct {
    char *code; char *fbneo; char *mame;
} ArcadeBoxartRow;
typedef struct {
    char *bytes;
    ArcadeBoxartRow *rows;
    int count;
} ArcadeBoxartIndex;

/* Parse sorted names produced from real Libretro HTTP indexes; owns data. */
int arcade_boxart_index_parse(ArcadeBoxartIndex *out,char *bytes,size_t length);
int arcade_boxart_index_load(ArcadeBoxartIndex *out,const CoverIO *io);
void arcade_boxart_index_free(ArcadeBoxartIndex *index);
/* Only verified online images, chosen by exact MAME/FBNeo ROM code. */
int arcade_boxart_urls(const ArcadeBoxartIndex *index,const char *rom_path,
                       char out[][COVER_URL_MAX],int max);
/* Return the raw short code, stripping .zip or .7z; never rename ROMs. */
int arcade_boxart_rom_code(const char *path,char *out,size_t cap);
#endif
