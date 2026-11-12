/* movie_spanish.h - offline Spanish display titles for detected BDISO films.
 * Original ISO names and manual cover filenames never change. */
#ifndef SF_MOVIE_SPANISH_H
#define SF_MOVIE_SPANISH_H
#include <stddef.h>
/* Returns 1 only when a known title should replace an absent/automatically
 * generated English .title. A manually supplied Spanish title wins. */
int movie_spanish_title(const char *iso_filename,const char *sidecar_title,
                        char *out,size_t cap);
/* Original filename controls whether the 3D cover label is shown. */
int movie_filename_is_3d(const char *path);
#endif
