/* No image manipulation: only trusted Blu-ray 3D edition front scans. */
#include "movie_edition.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static int passed,failed;
#define CHECK(x) do{if(x)passed++;else{failed++;printf("FAIL %s:%d %s\n",__FILE__,__LINE__,#x);}}while(0)
int main(void)
{
 char slug[260],poster[1024];
 const char *avatar="<html><head>"
  "<meta property=\"og:title\" content=\"Avatar Blu-ray 3D\" />"
  "<meta property=\"og:image\" content=\"https://www.mubis.es/media/releases/194/28525/avatar-blu-ray-3d-original.jpg\" />"
  "</head></html>";
 const char *thor="<meta property=\"og:title\" content=\"Thor Blu-ray 3D\" />"
  "<meta property=\"og:image\" content=\"https://www.mubis.es/media/releases/455/248/thor-blu-ray-3d-original.jpg\" />";
 const char *two_d="<meta property=\"og:title\" content=\"Avatar Blu-ray\" />"
  "<meta property=\"og:image\" content=\"https://www.mubis.es/media/releases/99/1/avatar-blu-ray-original.jpg\" />";
 const char *untrusted="<meta property=\"og:title\" content=\"Avatar Blu-ray 3D\" />"
  "<meta property=\"og:image\" content=\"https://example.com/movies/covers/3d.jpg\" />";
 CHECK(movie_edition_slug("Avatar",slug,sizeof(slug)) && !strcmp(slug,"avatar"));
 CHECK(movie_edition_slug("Cómo entrenar a tu dragón 2",slug,sizeof(slug)) &&
       !strcmp(slug,"como-entrenar-a-tu-dragon-2"));
 CHECK(movie_edition_slug("Thor: El mundo oscuro",slug,sizeof(slug)) &&
       !strcmp(slug,"thor-el-mundo-oscuro"));
 CHECK(movie_edition_mubis_html(avatar,poster,sizeof(poster)) &&
       !strcmp(poster,"https://www.mubis.es/media/releases/194/28525/avatar-blu-ray-3d-original.jpg"));
 CHECK(movie_edition_mubis_html(thor,poster,sizeof(poster)));
 CHECK(!movie_edition_mubis_html(two_d,poster,sizeof(poster)));
 CHECK(movie_edition_mubis_normal_html(two_d,poster,sizeof(poster)));
 CHECK(!movie_edition_mubis_normal_html(avatar,poster,sizeof(poster)));
 CHECK(!movie_edition_mubis_html(untrusted,poster,sizeof(poster)));
 CHECK(!movie_edition_mubis_html("<meta property=\"og:title\" content=\"Avatar Blu-ray 3D\" />",poster,sizeof(poster)));
 CHECK(movie_edition_bluray_id("thor",2011,0)==82808);
 CHECK(movie_edition_bluray_id("shrek",2001,0)==30402);
 CHECK(movie_edition_bluray_id("cars",2006,0)==82250);
 CHECK(movie_edition_bluray_id("rio",2011,1)==33406);
 CHECK(movie_edition_bluray_id("rio",2011,0)==24347);
 CHECK(movie_edition_bluray_id("how to train your dragon 2",2014,1)==115682);
 CHECK(movie_edition_bluray_id("warcraft",2016,1)==164503);
 CHECK(!movie_edition_bluray_id("thor",2014,0));
 CHECK(!movie_edition_bluray_id("rando unrelated",2009,0));
 printf("Authentic edition matching tests: %d passed, %d failed\n",passed,failed);
 return failed?1:0;
}
