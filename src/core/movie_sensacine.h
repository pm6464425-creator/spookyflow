/* Public SensaCine movie-search poster parsing.
 * Selects ONLY items explicitly marked "Cartel de <movie>", never a
 * Blu-ray 3D case, an advert, a trailer still or a random image. */
#ifndef SF_MOVIE_SENSACINE_H
#define SF_MOVIE_SENSACINE_H
#include <stddef.h>
int movie_sensacine_pick(const char *page,const char *title,char *url,size_t cap);
int movie_sensacine_fullsize(const char *thumb,char *url,size_t cap);
#endif
