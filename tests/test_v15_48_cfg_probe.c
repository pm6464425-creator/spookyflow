/* V15.48: read-only parser uses same lexer as the production FULL setter. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include "sf_snes_screen.h"
static void write_cfg(const char *path,const char *src) {
 FILE *f=fopen(path,"wb");assert(f);
 assert(fwrite(src,1,strlen(src),f)==strlen(src));
 assert(fclose(f)==0);
}
int main(int argc,char **argv) {
 char out[256]={0};
 const char *correct="PSNES : {\nEMULATION : { SCALING = \"FULL\"; };\n};\n";
 assert(argc==2);
 remove(argv[1]);
 assert(sf_snes_config_video_probe(argv[1],out,sizeof(out))==2);
 assert(strstr(out,"MISSING"));
 write_cfg(argv[1],correct);
 assert(sf_snes_config_video_probe(argv[1],out,sizeof(out))==0);
 assert(strstr(out,"expected_FULL=YES"));
 {
 FILE *f=fopen(argv[1],"rb");char b[256]={0};size_t n;assert(f);
 n=fread(b,1,sizeof(b)-1,f);assert(n==strlen(correct));
 fclose(f);assert(strcmp(b,correct)==0);
 }
 write_cfg(argv[1],"PSNES : {\nEMULATION : { /* SCALING=\"FULL\"; */ SCALING=\"FIT\"; };\n};\n");
 assert(sf_snes_config_video_probe(argv[1],out,sizeof(out))==1);
 assert(strstr(out,"scaling=FIT"));
 write_cfg(argv[1],"PSNES : { EMULATION : { FILTER=\"POINT\"; }; };\n");
 assert(sf_snes_config_video_probe(argv[1],out,sizeof(out))==2);
 write_cfg(argv[1],"PSNES : { OTHER : {SCALING=\"FULL\";}; };\n");
 assert(sf_snes_config_video_probe(argv[1],out,sizeof(out))==-5);
 remove(argv[1]);
 puts("PASS video-config probe detects missing, FULL, FIT, absent EMULATION and never rewrites files");
 return 0;
}
