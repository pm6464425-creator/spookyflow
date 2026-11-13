/* V15.26 - PS1 .CUE points to missing filename (Metal Slug X).
 * Resolve only one unambiguous same-title source, never alter source. */
#include "sf_ps1_file_match.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#define CHECK(expr) do{if(!(expr)){fprintf(stderr,"PS1 CUE name resolver FAILED line %d: %s\n",__LINE__,#expr);exit(1);}checks++;}while(0)
static int checks;
static int match(const char *expected,const char *const *names,size_t count,
                 char *out,size_t cap)
{
    SfPs1FileMatch m;
    if(sf_ps1_match_begin(&m,expected)!=0)return -9;
    for(size_t i=0;i<count;i++)sf_ps1_match_consider(&m,names[i]);
    return sf_ps1_match_finish(&m,out,cap);
}
int main(void)
{
    char result[SF_PS1_MATCH_NAME_MAX];
    {
        const char *names[]={"Metal Slug X.bin","Other Game.bin"};
        CHECK(match("Metal Slug X (Europe).bin",names,2,result,sizeof(result))==0);
        CHECK(!strcmp(result,"Metal Slug X.bin"));
    }
    {
        const char *names[]={"METAL SLUG X (EUROPE).BIN","Metal Slug XX.bin"};
        CHECK(match("Metal Slug X (Europe).bin",names,2,result,sizeof(result))==0);
        CHECK(!strcmp(result,names[0]));
    }
    {
        const char *names[]={"Metal Slug X (Europe).bin","Metal Slug X (USA).bin"};
        CHECK(match("Metal Slug X (Europe).bin",names,2,result,sizeof(result))==0);
        CHECK(!strcmp(result,names[0])); /* exact takes precedence */
    }
    {
        const char *names[]={"Metal Slug X (USA).bin","Metal Slug X (Japan).bin"};
        CHECK(match("Metal Slug X (Europe).bin",names,2,result,sizeof(result))==-2);
    }
    {
        const char *names[]={"Metal Slug XX.bin"};
        CHECK(match("Metal Slug X (Europe).bin",names,1,result,sizeof(result))==-1);
    }
    {
        const char *names[]={"Tekken (Europe) (Track 02).bin",
                             "Tekken (USA) (Track 01).bin"};
        CHECK(match("Tekken (Europe) (Track 01).bin",names,2,result,sizeof(result))==0);
        CHECK(!strcmp(result,"Tekken (USA) (Track 01).bin"));
    }
    {
        const char *names[]={"Tekken (USA) (Track 02).bin"};
        CHECK(match("Tekken (Europe) (Track 01).bin",names,1,result,sizeof(result))==-1);
    }
    {
        const char *names[]={"Game (Disc 2) (USA).bin"};
        CHECK(match("Game (Disc 1) (Europe).bin",names,1,result,sizeof(result))==-1);
    }
    {
        const char *names[]={"Game (Revision 2) (Europe).bin"};
        CHECK(match("Game (Revision 1) (Europe).bin",names,1,result,sizeof(result))==-1);
    }
    {
        const char *names[]={"Metal Slug X.iso","Metal Slug X.cue","readme.txt"};
        CHECK(match("Metal Slug X.bin",names,3,result,sizeof(result))==-1);
    }
    {
        const char *names[]={"Metal Slug X (USA).img"};
        CHECK(match("Metal Slug X (Europe).bin",names,1,result,sizeof(result))==0);
        CHECK(!strcmp(result,names[0]));
    }
    {
        SfPs1FileMatch m;
        CHECK(sf_ps1_match_begin(&m,"Metal Slug X (Europe).bin")==0);
        sf_ps1_match_consider(&m,"Another PS1 Game.bin");
        CHECK(sf_ps1_match_finish(&m,result,sizeof(result))==-1);
    }
    printf("PS1 CUE safe basename matcher %d tests PASS\n",checks);
    return 0;
}
