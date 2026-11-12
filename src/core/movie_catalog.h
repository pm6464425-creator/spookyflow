/* movie_catalog.h -- minimal, bounded Cinemeta metadata reader.
 * Keep in portable core so real JSON fixtures can be tested on Linux.
 */
#ifndef SF_MOVIE_CATALOG_H
#define SF_MOVIE_CATALOG_H
#include <stddef.h>
int movie_catalog_pick(const char *json, const char *query, int year,
                       char *poster, size_t poster_cap,
                       char *title, size_t title_cap);
#endif
