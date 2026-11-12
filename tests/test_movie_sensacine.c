#include "movie_sensacine.h"
#include <stdio.h>
#include <string.h>
static int passed,failed;
#define C(x) do {if(x)passed++;else{failed++;printf("FAIL line=%d %s\n",__LINE__,#x);}}while(0)
int main(void)
{
 char poster[1100],original[1100];
 const char *avatar=
  "<div class=\"card entity-card entity-card-list\"><figure>"
  "<img class=\"thumbnail-img\" src=\"data:image/gif;base64,AAA\" "
  "data-src=\"https://es.web.img3.acsta.net/c_310_420/medias/nmedia/18/92/13/82/20182449.jpg\" "
  "alt=\"Cartel de Avatar\" width=\"310\" height=\"420\" />"
  "</figure></div>"
  "<div><img data-src=\"https://es.web.img3.acsta.net/c_310_420/pictures/sequel.jpg\" "
  "alt=\"Cartel de Avatar: El sentido del agua\" /></div>";
 const char *ronin="<img data-src=\"https://es.web.img3.acsta.net/c_310_420/pictures/47ronin.jpg\" "
  "alt=\"Cartel de La leyenda del samurái - 47 Ronin\" />";
 const char *injected="<img data-src=\"https://evil.example.com/c_310_420/x.jpg\" "
  "alt=\"Cartel de Avatar\" />";
 const char *fake="<img data-src=\"https://es.web.img3.acsta.net/c_310_420/box.jpg\" "
  "alt=\"Funda Blu-ray Avatar 3D\" />";
 const char *only_sequel="<img data-src=\"https://es.web.img3.acsta.net/c_310_420/sequel.jpg\" "
  "alt=\"Cartel de Avatar: El sentido del agua\" />";
 C(movie_sensacine_pick(avatar,"Avatar",poster,sizeof(poster)));
 C(!strcmp(poster,"https://es.web.img3.acsta.net/c_310_420/medias/nmedia/18/92/13/82/20182449.jpg"));
 C(movie_sensacine_fullsize(poster,original,sizeof(original)));
 C(!strcmp(original,"https://es.web.img3.acsta.net/medias/nmedia/18/92/13/82/20182449.jpg"));
 C(movie_sensacine_pick(ronin,"47 Ronin",poster,sizeof(poster)));
 C(!strcmp(poster,"https://es.web.img3.acsta.net/c_310_420/pictures/47ronin.jpg"));
 C(movie_sensacine_pick(ronin,"47 RONIN",poster,sizeof(poster)));
 C(movie_sensacine_pick(ronin,"47 ronin",poster,sizeof(poster)));
 C(!movie_sensacine_pick(injected,"Avatar",poster,sizeof(poster)));
 C(!movie_sensacine_pick(fake,"Avatar",poster,sizeof(poster)));
 C(!movie_sensacine_pick(only_sequel,"Avatar",poster,sizeof(poster)));
 C(!movie_sensacine_pick(avatar,"Thor",poster,sizeof(poster)));
 C(!movie_sensacine_fullsize("https://evil.example.com/c_310_420/a.jpg",original,sizeof(original)));
 C(!movie_sensacine_pick(avatar,"Avatar",poster,4));
 printf("SensaCine flat movie poster parser: %d passed, %d failed\n",passed,failed);
 return failed?1:0;
}
