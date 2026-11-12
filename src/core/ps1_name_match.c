/* ps1_name_match.c - conservative title/region matcher for PS1 covers.
 *
 * This reads only the catalogue of REAL PNG names bundled at package build
 * time from the public Libretro Sony - PlayStation Named_Boxarts directory.
 * A local file "Disney Tarzan Latino.cue" maps to "Disney Tarzan (Spain).png"
 * without inventing remote URLs or overwriting the user's disc images.
 * The catalogue is not used for PS2, PS3 or BDISO.
 */
#include "ps1_name_match.h"
#include "util.h"
#include <stdio.h>
#include <string.h>

#define KMAX 160
#define NAMEMAX 256

typedef struct {
    char key[KMAX];
    int region, disc, demo, rerelease, language_es;
} NameParts;

static int letter(char c) { return (c>='a'&&c<='z') || (c>='A'&&c<='Z'); }
static int digit(char c) { return c>='0' && c<='9'; }
static char low(char c) { return c>='A'&&c<='Z'? (char)(c+32):c; }
static int eq(const char *a,const char *b) {return strcmp(a,b)==0;}
static void append(char *dst,size_t cap,const char *s)
{
    size_t at=strlen(dst),n=strlen(s);
    if(at+n+1<cap){memcpy(dst+at,s,n+1);}
}
/* Latin-American custom dump names routinely append "Latino", "Spanish",
 * translation tags or track numbers. Those are NOT part of the game's
 * official PlayStation title and should never be searched literally. */
static int stopword(const char *s)
{
    static const char *const skip[]={
        "latino","latina","latam","espanol","espanola","spanish",
        "castellano","castellana","traducido","traduccion","trad",
        "multilang","multilanguage","multi5","multi","lang",
        "version","versiones","patched","patch","original",
        "psx","ps1","playstation","rom","redump","bin","cue",
        "usa","us","europe","eu","spain","espana","japan",
        "jap","ntsc","pal","eng","en","es","fr","ger",
        "france","germany","italy","uk","brasil","brazil",
        "region","world","rev","revision","rerelease",
        "cd","disc","disk","disco","track","pista"
    };
    int i;
    for(i=0;i<(int)(sizeof(skip)/sizeof(skip[0]));i++)
        if(eq(s,skip[i]))return 1;
    return 0;
}
static int region_code(const char *t)
{
    if(eq(t,"spain")||eq(t,"espana")||eq(t,"latino")||eq(t,"latina")||
       eq(t,"latam")||eq(t,"espanol")||eq(t,"espanola")||
       eq(t,"spanish")||eq(t,"castellano")||eq(t,"castellana"))return 3;
    if(eq(t,"usa")||eq(t,"us"))return 2;
    if(eq(t,"europe")||eq(t,"eu"))return 1;
    if(eq(t,"japan")||eq(t,"jap"))return 4;
    return 0;
}
static void process_token(NameParts *p,char *token,int *disc_next,int *track_next)
{
    int r;
    if(!token[0])return;
    r=region_code(token);
    if(r){p->region=r;if(r==3)p->language_es=1;}
    if(eq(token,"demo"))p->demo=1;
    if(eq(token,"rerelease")||eq(token,"reissue"))p->rerelease=1;
    if(eq(token,"disc")||eq(token,"disk")||eq(token,"disco")||eq(token,"cd")) {
        *disc_next=1;return;
    }
    if(eq(token,"track")||eq(token,"pista")) {*track_next=1;return;}
    if(*disc_next) {
        if(digit(token[0])&&token[1]=='\0')p->disc=token[0]-'0';
        *disc_next=0;
        return;
    }
    if(*track_next) {*track_next=0;return;}
    if(stopword(token))return;
    if(p->key[0])append(p->key,sizeof(p->key)," ");
    append(p->key,sizeof(p->key),token);
}
static void parts(const char *filename,NameParts *out)
{
    const char *base=filename,*e;
    char token[64];
    size_t i,finish,n=0;
    int disc_next=0,track_next=0;
    memset(out,0,sizeof(*out));
    if(!filename)return;
    for(e=filename;*e;e++)if(*e=='/'||*e=='\\')base=e+1;
    finish=strlen(base);
    if(finish>4 &&
       (!sf_stricmp(base+finish-4,".cue")||
        !sf_stricmp(base+finish-4,".bin")||
        !sf_stricmp(base+finish-4,".png")||
        !sf_stricmp(base+finish-4,".iso")||
        !sf_stricmp(base+finish-4,".chd")||
        !sf_stricmp(base+finish-4,".pbp")))finish-=4;
    for(i=0;i<=finish;i++) {
        unsigned char c=i==finish?0:(unsigned char)base[i];
        /* English possessive: Disney's == Disney, Lion's == Lion. */
        if(c==39 && i+1<finish && (base[i+1]=='s'||base[i+1]=='S') &&
           (i+2>=finish || !letter(base[i+2]))) {i++;continue;}
        /* Decode common accented Spanish vowels in a UTF-8 filename. */
        if(c==0xc3 && i+1<finish) {
            unsigned char next=(unsigned char)base[++i];
            if(next==0xa1||next==0x81||next==0xa0||next==0xa2)c='a';
            else if(next==0xa9||next==0x89||next==0xa8)c='e';
            else if(next==0xad||next==0x8d)c='i';
            else if(next==0xb3||next==0x93)c='o';
            else if(next==0xba||next==0x9a||next==0xbc)c='u';
            else if(next==0xb1||next==0x91)c='n';
            else c=' ';
        }
        if(letter((char)c)||digit((char)c)) {
            if(n+1<sizeof(token))token[n++]=low((char)c);
        } else if(n) {
            token[n]='\0';
            process_token(out,token,&disc_next,&track_next);
            n=0;
        }
    }
}
static int valid_exact_key(const NameParts *game,const NameParts *cover)
{
    if(!game->key[0] || !cover->key[0])return 0;
    if(eq(game->key,cover->key))return 1;
    /* The official franchise sometimes adds "Disney's" although a
     * localized dump is named only "Tarzan Latino". Only allow removing
     * a WHOLE Disney token, not arbitrary substring/fuzzy matching. */
    if(!strncmp(game->key,"disney ",7) && eq(game->key+7,cover->key))return 1;
    if(!strncmp(cover->key,"disney ",7) && eq(cover->key+7,game->key))return 1;
    return 0;
}
static int score(const NameParts *game,const NameParts *cover)
{
    int result=100;
    if(!valid_exact_key(game,cover))return -1;
    if(cover->demo!=game->demo)return -1;
    if(game->disc && cover->disc && game->disc!=cover->disc)return -1;
    if(game->disc && cover->disc==game->disc)result+=15;
    if(!cover->disc && !game->disc)result+=4;
    if(!cover->rerelease)result+=5;
    if(game->region) {
        if(game->region==cover->region)result+=50;
        else if(game->region==3 && cover->region==2)result+=30;
        else if(game->region==3 && cover->region==1)result+=20;
        else if(game->region==2 && cover->region==1)result+=20;
    }else {
        if(cover->region==2)result+=40;
        else if(cover->region==1)result+=30;
        else if(cover->region==3)result+=20;
    }
    return result;
}
int ps1_name_catalog_match(const char *game_path,const char *index,size_t len,
                           char *image_filename,size_t cap)
{
    NameParts game,candidate;
    size_t at=0,sz;
    char line[NAMEMAX];
    int best=-1;
    if(!image_filename||cap<8)return 0;
    image_filename[0]='\0';
    if(!index||!len||!game_path)return 0;
    parts(game_path,&game);
    if(!game.key[0])return 0;
    while(at<len){
        int s;
        size_t end=at;
        while(end<len && index[end]!='\n' && index[end]!='\r')end++;
        sz=end-at;
        if(sz>4&&sz<NAMEMAX){
            memcpy(line,index+at,sz);line[sz]='\0';
            if(sf_ends_with_i(line,".png")) {
                parts(line,&candidate);
                s=score(&game,&candidate);
                if(s>best && strlen(line)+1<=cap){
                    best=s;
                    strcpy(image_filename,line);
                }
            }
        }
        at=end;
        while(at<len&&(index[at]=='\n'||index[at]=='\r'))at++;
    }
    return best>=0;
}
int ps1_name_catalog_url(const char *image_filename,char *url,size_t cap)
{
    static const char prefix[]=
        "https://thumbnails.libretro.com/Sony%20-%20PlayStation/Named_Boxarts/";
    static const char hex[]="0123456789ABCDEF";
    size_t i,pos;
    if(!url||!cap)return 0;
    url[0]='\0';
    if(!image_filename||!sf_ends_with_i(image_filename,".png")||
       strchr(image_filename,'/')||strchr(image_filename,'\\'))return 0;
    if(sizeof(prefix)>=cap)return 0;
    memcpy(url,prefix,sizeof(prefix));
    pos=sizeof(prefix)-1;
    for(i=0;image_filename[i];i++) {
        unsigned char c=(unsigned char)image_filename[i];
        if(letter((char)c)||digit((char)c)||c=='-'||c=='_'||c=='.'||
           c=='('||c==')') {
            if(pos+2>cap)return 0;
            url[pos++]=(char)c;
        }else{
            if(pos+4>cap)return 0;
            url[pos++]='%';url[pos++]=hex[c>>4];url[pos++]=hex[c&15];
        }
    }
    url[pos]=0;
    return 1;
}
