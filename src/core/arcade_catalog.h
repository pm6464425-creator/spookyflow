/* Arcade-only ROM shortname -> official game description lookup.
 * No ROM ZIP rename, no dependency on an emulator. */
#ifndef SF_ARCADE_CATALOG_H
#define SF_ARCADE_CATALOG_H
void arcade_catalog_init(void);
const char *arcade_catalog_title(const char *filename);
int arcade_catalog_count(void);
#endif
