/* Compiled host C tests exercising the SAME metadata matcher as the PS3. */
#include "movie_catalog.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
static int total,failed;
#define CHECK(c) do {total++;if(!(c)){failed++;printf("FAIL %s:%d: %s\n",__FILE__,__LINE__,#c);}}while(0)

static void test_movie(const char *name,const char *query,int year,int expect)
{
    char json[2100],poster[1024],title[256];
    int result;
    snprintf(json,sizeof(json),
      "{\"metas\":[{\"id\":\"tt1234\",\"type\":\"movie\","
      "\"name\":\"%s\",\"genres\":[\"Action\",\"Fantasy\"],"
      "\"behaviorHints\":{\"defaultVideoId\":\"tt1234\"},"
      "\"releaseInfo\":\"%d\",\"poster\":\"https://m.media-amazon.com/images/M/test.jpg\"},"
      "{\"name\":\"Thor: Ragnarok\",\"year\":2017,\"poster\":\"https://image.tmdb.org/t/p/w500/test.jpg\"}]}",
      name,year?year:2011);
    result=movie_catalog_pick(json,query,year,poster,sizeof(poster),title,sizeof(title));
    CHECK(result==expect);
    if(expect){CHECK(strcmp(title,name)==0);CHECK(strstr(poster,"m.media-amazon.com")!=NULL);}
}
int main(void)
{
    char poster[1024],title[256];
    /* Nested genre array is BEFORE the poster: strchr(']') prematurely
     * truncated this response in the released V6 and all films failed. */
    test_movie("Thor","Thor",2011,1);
    test_movie("Avatar","Avatar",2009,1);
    test_movie("Shrek","Shrek",2001,1);
    test_movie("Cars","Cars",2006,1);
    test_movie("Dredd","Dredd",2012,1);
    test_movie("Doctor Strange","Doctor Strange",2016,1);
    test_movie("I, Robot","Yo Robot",2004,1);
    test_movie("Final Destination 5","Final Destination V",2011,1);
    test_movie("Warcraft","Warcraft The Beginning",2016,1);
    test_movie("Mr. Peabody & Sherman","Mr Peabody Sherman",2014,1);
    test_movie("How to Train Your Dragon 2","How to Train Your Dragon II",2014,1);
    test_movie("Batman v Superman: Dawn of Justice","Batman v Superman Dawn of Justice",2016,1);
    test_movie("Terminator 2: Judgment Day","Terminator 2 el Juicio Final",1991,1);
    test_movie("Thor","Thor",2012,0);
    test_movie("Avengers","Thor",2011,0);
    CHECK(!movie_catalog_pick("{\"metas\":[{\"name\":\"Thor\","
        "\"poster\":\"https://evil.example/test.jpg\","
        "\"releaseInfo\":\"2011\"}]}", "Thor",2011,
        poster,sizeof(poster),title,sizeof(title)));
    CHECK(!movie_catalog_pick("{\"metas\":[{\"name\":\"Thor\","
        "\"poster\":\"https://m.media-amazon.com/x.jpg\""
        "\"genres\":[\"Action\"]}", "Thor",2011,
        poster,sizeof(poster),title,sizeof(title)));
    /* Results with nested arrays + quotes containing ] and } are valid. */
    CHECK(movie_catalog_pick("{\"metas\":[{\"name\":\"Thor\","
        "\"genres\":[\"Sci ] Fi\",\"Action }\"],"
        "\"behaviorHints\":{\"foo\":{\"a\":[1,2]}},"
        "\"poster\":\"https://image.tmdb.org/t/p/w500/thor.jpg\","
        "\"releaseInfo\":\"2011\"}]}", "Thor",2011,
        poster,sizeof(poster),title,sizeof(title)));
    printf("real-json-shaped movie catalog tests: %d passed, %d failed\n",
           total-failed,failed);
    return failed?1:0;
}
