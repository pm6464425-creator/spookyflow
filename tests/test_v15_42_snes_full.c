/* V15.42 native pSNES 16:9 regression.
 * Run under host ASan/UBSan with the same C config modifier as PS3. */
#include "sf_snes_screen.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#define CHECK(x) do { if(!(x)) { fprintf(stderr,"FAIL V15.42:%d %s\n",__LINE__,#x); exit(2); } ++checks; } while(0)
static int checks;
static void write_str(const char *p,const char *text)
{
    FILE *f=fopen(p,"wb");
    CHECK(f!=NULL);
    CHECK(fwrite(text,1,strlen(text),f)==strlen(text));
    CHECK(fclose(f)==0);
}
static char *read_file(const char *p)
{
    FILE *f=fopen(p,"rb");
    long n;
    char *s;
    CHECK(f!=NULL);
    CHECK(fseek(f,0,SEEK_END)==0);
    n=ftell(f);
    CHECK(n>=0 && n<256*1024);
    CHECK(fseek(f,0,SEEK_SET)==0);
    s=(char*)malloc((size_t)n+1);
    CHECK(s!=NULL);
    CHECK(fread(s,1,(size_t)n,f)==(size_t)n);
    CHECK(fclose(f)==0);
    s[n]='\0';
    return s;
}
int main(int argc,char **argv)
{
    char global[500],game[500],missing[500],broken[500];
    char *s,*snapshot;
    const char *initial =
        "PSNES : {\n  VERSION = 702;\n"
        "  UI_OPTIONS : { SKIN_ASPECT = \"16/9\"; };\n"
        "  EMULATION : {\n    SCALING = \"FIT\";\n"
        "    SCALING_MODE = \"ASPECT\";\n"
        "    FILTER = \"LINEAR\";\n    SHOW_FPS = \"OFF\";\n  };\n"
        "  GAMEPAD : { JOY_UP = \"1\"; };\n};\n";
    const char *pergame =
        "PSNES_GAME : {\n  EMULATION : { SCALING = \"2X\";\n"
        "  FILTER = \"POINT\";  }; \n  GAMEPAD : { JOY_A=\"2\"; }; };";
    const char *comments =
        "PSNES : {\n  # EMULATION : { SCALING = \"FIT\"; }\n"
        "  LOG : { MSG = \"SCALING = \\\"FIT\\\"\"; };\n"
        "  /* EMULATION : { SCALING = \"FIT\"; } */\n"
        "  EMULATION : { FILTER = \"LINEAR\"; }; \n};\n";
    const char *malformed =
        "PSNES : { EMULATION : { SCALING = maybe; FILTER = \"LINEAR\"; }; };\n";
    if(argc!=2) return 3;
    CHECK(strlen(argv[1]) < 380);
    snprintf(global,sizeof(global),"%s/global.cfg",argv[1]);
    snprintf(game,sizeof(game),"%s/X-Kaliber 2097 (ESP).cfg",argv[1]);
    snprintf(missing,sizeof(missing),"%s/nonexistent.cfg",argv[1]);
    snprintf(broken,sizeof(broken),"%s/broken.cfg",argv[1]);

    write_str(global,initial);
    CHECK(sf_snes_config_set_full(global,1)==0);
    s=read_file(global);
    CHECK(strstr(s,"SCALING = \"FULL\";")!=NULL);
    CHECK(strstr(s,"SCALING_MODE = \"ASPECT\";")!=NULL);
    CHECK(strstr(s,"FILTER = \"LINEAR\";")!=NULL);
    CHECK(strstr(s,"GAMEPAD : { JOY_UP = \"1\"; }")!=NULL);
    CHECK(strstr(s,"SKIN_ASPECT = \"16/9\";")!=NULL);
    snapshot=s;
    CHECK(sf_snes_config_set_full(global,1)==0);
    s=read_file(global);
    CHECK(strcmp(s,snapshot)==0); /* idempotent */
    free(snapshot);free(s);

    write_str(game,pergame);
    CHECK(sf_snes_config_set_full(game,0)==0);
    s=read_file(game);
    CHECK(strstr(s,"SCALING = \"FULL\";")!=NULL);
    CHECK(strstr(s,"FILTER = \"POINT\";")!=NULL);
    CHECK(strstr(s,"JOY_A=\"2\"")!=NULL);
    free(s);

    CHECK(sf_snes_config_set_full(missing,0)==0);
    CHECK(access(missing,F_OK)!=0);
    CHECK(sf_snes_config_set_full(missing,1)==0);
    s=read_file(missing);
    CHECK(strstr(s,"PSNES : {")!=NULL);
    CHECK(strstr(s,"SCALING = \"FULL\";")!=NULL);
    free(s);

    write_str(global,comments);
    CHECK(sf_snes_config_set_full(global,1)==0);
    s=read_file(global);
    CHECK(strstr(s,"EMULATION : { FILTER = \"LINEAR\"; \n    SCALING = \"FULL\";};")!=NULL);
    CHECK(strstr(s,"# EMULATION : { SCALING = \"FIT\"; }")!=NULL);
    free(s);

    write_str(broken,malformed);
    CHECK(sf_snes_config_set_full(broken,1)<0);
    s=read_file(broken);
    CHECK(strcmp(s,malformed)==0);
    free(s);

    /* Explicit unsupported file cannot clobber user's saved options. */
    CHECK(sf_snes_config_set_full(NULL,1)<0);
    CHECK(remove(global)==0);
    CHECK(remove(game)==0);
    CHECK(remove(missing)==0);
    CHECK(remove(broken)==0);
    printf("PASS V15.42 native pSNES 16:9 FULL config: %d checks\n",checks);
    return 0;
}
