/* Movie edition cover identification.  These functions NEVER add text to
 * posters. They only pick covers from actual 3D Blu-ray edition listings.
 * No recognition by raw webpage search snippets or unrelated thumbnails. */
#ifndef SF_MOVIE_EDITION_H
#define SF_MOVIE_EDITION_H
#include <stddef.h>
int movie_edition_slug(const char *title,char *out,size_t cap);
int movie_edition_mubis_html(const char *page,char *poster,size_t cap);
int movie_edition_bluray_id(const char *title,int year,int spanish);
#endif
