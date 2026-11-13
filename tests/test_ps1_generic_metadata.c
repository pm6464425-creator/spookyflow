/* General PS1 metadata title resolver regression. NO per-game allowlist:
 * tests use real diagnostic patterns to exercise CUE metadata rules,
 * and unrelated/ambiguous/revision/track cases to prevent false mounts. */
#include "sf_ps1_file_match.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#define T(x) do { if(!(x)) { fprintf(stderr,"PS1 GENERIC NAME MATCH FAIL line %d: %s\n",__LINE__,#x);exit(1);}count++;}while(0)
static int count;
static int match(const char *expected,const char *const *names,size_t n,char *out)
{
    SfPs1FileMatch m;
    if(sf_ps1_match_begin(&m,expected)!=0)return -9;
    for(size_t i=0;i<n;i++)sf_ps1_match_consider(&m,names[i]);
    return sf_ps1_match_finish(&m,out,SF_PS1_MATCH_NAME_MAX);
}
#define M(expected, result, ...) do {const char *const choices[]={__VA_ARGS__};char o[SF_PS1_MATCH_NAME_MAX];T(match(expected,choices,sizeof(choices)/sizeof(choices[0]),o)==(result));}while(0)
#define WIN(expected, win, ...) do {const char *const choices[]={__VA_ARGS__};char o[SF_PS1_MATCH_NAME_MAX];T(match(expected,choices,sizeof(choices)/sizeof(choices[0]),o)==0);T(!strcmp(o,win));}while(0)
int main(void)
{
    /* Examples of metadata from the user's PS3 diagnostics. */
    WIN("(PS1) Disney Tarzan [PAL] [SCES-01519].BIN","Disney Tarzan.bin",
        "Disney Tarzan.bin","Disney Aladdin.bin");
    WIN("(PS1) Disney Tarzan [PAL] [SCES-01519].BIN",
        "Disney Tarzan (Europe) (En,Fr,De,Es,It).BIN",
        "Disney Tarzan (Europe) (En,Fr,De,Es,It).BIN","Tarzan 2.bin");
    WIN("(PS1) Disney Tarzan [PAL] [SCES-01519].BIN",
        "Disney's Tarzan (Europe).bin","Disney's Tarzan (Europe).bin");
    WIN("Grand Theft Auto 2 (Europe) (En,Fr,De,Es,It) (Rev 1).bin",
        "Grand Theft Auto 2.bin","Grand Theft Auto 2.bin","Grand Theft Auto 1.bin");
    WIN("Grand Theft Auto 2 (Europe) (En,Fr,De,Es,It) (Rev 1).bin",
        "Grand Theft Auto 2 (USA).bin","Grand Theft Auto 2 (USA).bin");
    WIN("Grand Theft Auto 2 (Europe) (En,Fr,De,Es,It) (Rev 1).bin",
        "Grand Theft Auto 2 (Rev 1).bin",
        "Grand Theft Auto 2 (Rev 1).bin","Grand Theft Auto 2.bin");
    M("Grand Theft Auto 2 (Europe) (Rev 1).bin",-1,
        "Grand Theft Auto 2 (Rev 2).bin","Grand Theft Auto 3.bin");
    M("Grand Theft Auto 2 (Europe) (Rev 1).bin",-2,
        "Grand Theft Auto 2 (USA).bin","Grand Theft Auto 2 (Japan).bin");
    WIN("(PS1) Disney Tarzan [PAL] [SCES-01519].BIN",
        "SCES_015.19.bin","SCES_015.19.bin");
    M("(PS1) Disney Tarzan [PAL] [SCES-01519].BIN",-1,
        "SLES_015.19.bin","Other Game.bin");
    M("(PS1) Disney Tarzan [PAL] [SCES-01519].BIN",-1,
        "Disney Tarzan [SCUS-94163].BIN");
    M("(PS1) Disney Tarzan [PAL] [SCES-01519].BIN",-2,
        "Disney Tarzan (USA).bin","Disney Tarzan (Japan).bin");
    /* Never mix tracks, discs, revisions or games in an ambiguous folder. */
    WIN("Example (Europe) (Track 01).bin",
        "Example (Japan) (Track 01).bin",
        "Example (Japan) (Track 01).bin","Example (USA) (Track 02).bin");
    M("Example (Track 01).bin",-1,"Example.bin");
    M("Example.bin",-1,"Example (Track 01).bin");
    M("Example (Disc 1).bin",-1,"Example (Disc 2).bin");
    M("Example (Disc 1).bin",-1,"Example.bin");
    M("Example (Rev 1).bin",-1,"Example (Rev 2).bin");
    M("Example (Europe).bin",-1,"Example 2.bin");
    M("Final Fantasy 7 (Disc 1).bin",-1,"Final Fantasy 7 (Disc 2).bin");
    M("Example (Europe).bin",-1,"Example (USA) (Track 01).bin");
    M("Example (Europe).bin",-1,"Example.iso","Example.cue","example.txt");
    WIN("Example (Europe).bin","EXAMPLE (EUROPE).BIN",
        "EXAMPLE (EUROPE).BIN","Example (Japan).bin");
    WIN("Disney Tarzan (Europe).bin","Disney Tarzan (Europe).bin",
        "Disney Tarzan (Europe).bin","Disney Tarzan (Japan).bin");
    WIN("(PS1) Game [PAL] [SCES-01234].BIN","Game (Europe).img",
        "Game (Europe).img");
    M("(PS1) Game [PAL] [SCES-01234].BIN",-1,"Another Game.bin");
    M("Example (Europe).bin",-1,"Random.bin");
    printf("PS1 generic CUE title / metadata resolver %d tests PASS\n",count);
    return 0;
}
