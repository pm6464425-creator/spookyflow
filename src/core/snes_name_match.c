/* snes_name_match.c - conservative verified Nintendo SNES cover filename matcher.
 * Match official game titles, tolerate numbered packs, ROM tags, editions and
 * region differences. NEVER match arbitrary prefixes/substring or guessed URLs.
 */
#include "snes_name_match.h"
#include "util.h"
#include <stdio.h>
#include <string.h>

enum { KEY_MAX = 210, NAME_MAX = 256 };
typedef struct {
    char key[KEY_MAX];
    int region, rev, beta, demo, extra, hack, edition;
} Parts;

static int alp(unsigned char c) {return (c>='a'&&c<='z') || (c>='A'&&c<='Z');}
static int dig(unsigned char c) {return c>='0'&&c<='9';}
static char low(unsigned char c) {return (char)(c>='A'&&c<='Z'?c+32:c);}
static int eq(const char *a,const char *b) {return strcmp(a,b)==0;}

static int region_token(const char *t)
{
    if(eq(t,"usa")||eq(t,"us")||eq(t,"u"))return 1;
    if(eq(t,"europe")||eq(t,"eur")||eq(t,"eu")||eq(t,"e")||eq(t,"pal")||eq(t,"spain")||eq(t,"espana"))return 2;
    if(eq(t,"japan")||eq(t,"jpn")||eq(t,"jap")||eq(t,"j"))return 3;
    if(eq(t,"world")||eq(t,"asia")||eq(t,"w"))return 4;
    return 0;
}
static int metadata(const char *t)
{
    static const char *const tags[]={
        "snes","smc","sfc","zip","rom","supernintendo","nointro",
        "rev","revision","version","ver","v","beta","proto","prototype",
        "sample","demo","bad","dump","good","verified","baddump",
        "en","es","fr","de","it","pt","ja","jp","eng","esp","ger",
        "englis","english","spanish","espanol","latino","latina",
        "castellano","ntsc","pal","translated","traduccion","translation",
        "virtual","console","classic","mini","switch","online",
        "unl","unlicensed","aftermarket","checksum","intro","hack","patched",
        "patch","mod","hackrom","rerelease","reissue"
    };
    int i;
    for(i=0;i<(int)(sizeof(tags)/sizeof(tags[0]));i++)
        if(eq(t,tags[i]))return 1;
    return 0;
}
static void token(Parts *p,const char *t,int bracket)
{
    size_t n;
    if(!t[0])return;
    /* Region words are metadata only inside () or []; World is part of
       titles such as Super Mario World and must NEVER be dropped. */
    if(bracket && region_token(t)){
        if(!p->region)p->region=region_token(t);
        return;
    }
    if(eq(t,"rev")||eq(t,"revision"))p->rev=1;
    if(eq(t,"beta")||eq(t,"proto")||eq(t,"prototype")||eq(t,"sample"))p->beta=1;
    if(eq(t,"demo"))p->demo=1;
    if(eq(t,"virtual")||eq(t,"switch")||eq(t,"arcade")||eq(t,"classic"))p->extra=1;
    if(eq(t,"hack")||eq(t,"mod")||eq(t,"patch")||eq(t,"patched"))p->hack=1;
    if(eq(t,"the"))return; /* allow "The Legend" vs "Legend, The" without fuzzy guesses */
    if(bracket && metadata(t))return;
    /* Unknown parenthetical qualifiers such as (Caravan Event Ban) mean
     * SPECIAL editions, not the retail game. Never prefer one over the
     * standard (Japan) or (USA) release for a translation ROM. */
    if(bracket && !eq(t,"by") && !eq(t,"t") && !eq(t,"add") &&
       !eq(t,"i") && !eq(t,"v") && !dig((unsigned char)t[0]))
        p->edition++;
    /* No-Intro tags such as (Rev 1), (En,Fr), [!], [T-En] do not form title.
       Bracketed words are metadata except an explicit (AKA title), which is
       not automatically trusted because it can refer to unrelated editions. */
    if(bracket)return;
    n=strlen(t);
    if(!n || n>32)return;
    if(p->key[0]) {
        if(strlen(p->key)+1>=KEY_MAX)return;
        strcat(p->key," ");
    }
    if(strlen(p->key)+n+1<KEY_MAX)strcat(p->key,t);
}
static void parts(const char *path,Parts *p)
{
    const char *s,*q;
    char one[48];
    size_t n,i,size=0;
    int depth=0;
    memset(p,0,sizeof(*p));
    if(!path)return;
    s=strrchr(path,'/');
    s=s?s+1:path;
    q=strrchr(s,'\\');
    if(q)s=q+1;
    n=strlen(s);
    if(n>=4 && (sf_ends_with_i(s,".sfc")||sf_ends_with_i(s,".smc")||
                 sf_ends_with_i(s,".zip")||sf_ends_with_i(s,".png")))n-=4;
    /* Strip explicit translation-team / uncensored badges from the
     * provided ROM pack while keeping the game's actual name intact. */
    {
        const char *cut=strstr(s," - Sin Censura -");
        if(!cut)cut=strstr(s," -SirYoink -");
        if(cut && (size_t)(cut-s)<n)n=(size_t)(cut-s);
    }
    /* Many ROM packs prefix a library position like 001_, 002 - ... .
       Exclude only 3-digit counters; keep real game titles such as "7th Saga". */
    if(n>5 && dig((unsigned char)s[0])&&dig((unsigned char)s[1])&&
       dig((unsigned char)s[2]) && (s[3]==' '||s[3]=='_'||s[3]=='-'||s[3]=='.')) {
        s+=4;n-=4;
    }
    for(i=0;i<=n;i++){
        unsigned char c=i==n?0:(unsigned char)s[i];
        if(c=='('||c=='['||c=='{'){
            if(size) {one[size]=0;token(p,one,depth>0);size=0;}
            depth++;
            continue;
        }
        if(c==')'||c==']'||c=='}'){
            if(size){one[size]=0;token(p,one,depth>0);size=0;}
            if(depth>0)depth--;
            continue;
        }
        if(c==0xc3 && i+1<n){
            unsigned char f=(unsigned char)s[++i];
            if(f==0xa1||f==0x81||f==0xa0||f==0xa2)c='a';
            else if(f==0xa9||f==0x89||f==0xa8)c='e';
            else if(f==0xad||f==0x8d)c='i';
            else if(f==0xb3||f==0x93)c='o';
            else if(f==0xba||f==0x9a||f==0xbc)c='u';
            else if(f==0xb1||f==0x91)c='n';
            else c=' ';
        }
        if(alp(c)||dig(c)){
            if(size+1<sizeof(one))one[size++]=low(c);
        }else if(size){
            one[size]=0;token(p,one,depth>0);size=0;
        }
    }
}
static int aliases(const char *game,const char *cover)
{
    if(eq(game,cover))return 1;
    /* Verified official cover names that include a longer subtitle than
       their widely distributed ROM aliases. No arbitrary prefix matching. */
    if(eq(game,"super mario rpg")&&eq(cover,"super mario rpg legend of seven stars"))return 1;
    if(eq(game,"donkey kong country 2")&&eq(cover,"donkey kong country 2 diddy s kong quest"))return 1;
    if(eq(game,"donkey kong country 3")&&eq(cover,"donkey kong country 3 dixie kong s double trouble"))return 1;
    if(eq(game,"legend of zelda link to past")&&eq(cover,"legend of zelda a link to past"))return 1;
    if(eq(game,"yoshi s island")&&eq(cover,"super mario world 2 yoshi s island"))return 1;
    if(eq(game,"ancient magic bazoo mahou sekai")&&eq(cover,"ancient magic bazoe mahou sekai"))return 1;
    if(eq(game,"bio metal")&&eq(cover,"biometal"))return 1;
    if(eq(game,"black thorne")&&eq(cover,"blackthorne"))return 1;
    if(eq(game,"ilusion of time")&&eq(cover,"illusion of time"))return 1;
    if(eq(game,"dino city")&&eq(cover,"dinocity"))return 1;
    /* Every alias below points to a different spelling or official subtitle
     * of the SAME SNES game, verified against Libretro Named_Boxarts.
     * These are exact two-sided title identities, never substring guesses. */
    if(eq(game,"bastard ankoku no hakai shin")&&eq(cover,"bastard ankoku no hakaishin"))return 1;
    if(eq(game,"do re mi fantasy milon no dokidoki daibouken")&&
       eq(cover,"doremi fantasy milon no dokidoki daibouken"))return 1;
    if(eq(game,"captain tsubasa iii challenge of kaiser")&&
       eq(cover,"captain tsubasa iii koutei no chousen"))return 1;
    if(eq(game,"captain tsubasa v hasha no shougou canpione")&&
       eq(cover,"captain tsubasa v hasha no shougou campione"))return 1;
    if(eq(game,"carmen sandiego where in time")&&
       eq(cover,"where in time is carmen sandiego"))return 1;
    if(eq(game,"sky blazer")&&eq(cover,"skyblazer"))return 1;
    if(eq(game,"secret of stars")&&eq(cover,"tecmo secret of stars"))return 1;
    if(eq(game,"final fantasy magic quest")&&eq(cover,"final fantasy mystic quest"))return 1;
    if(eq(game,"mega man vii")&&eq(cover,"mega man 7"))return 1;
    if(eq(game,"gourmet squadron barayarou")&&eq(cover,"gourmet sentai barayarou"))return 1;
    if(eq(game,"super nova darius force")&&
       (eq(cover,"super nova")||eq(cover,"darius force")))return 1;
    if(eq(game,"smurfs 2")&&eq(cover,"smurfs travel world"))return 1;
    if(eq(game,"fifa 96")&&eq(cover,"fifa soccer 96"))return 1;
    if(eq(game,"dragon ball z super butoden")&&
       eq(cover,"dragon ball z super butouden"))return 1;
    if(eq(game,"dragon ball z super butouden 3 ultime menace")&&
       (eq(cover,"dragon ball z ultime menace")||
        eq(cover,"dragon ball z super butouden 3")))return 1;
    if(eq(game,"sim city 2000")&&
       eq(cover,"simcity 2000 ultimate city simulator"))return 1;
    if(eq(game,"spawn")&&eq(cover,"todd mcfarlane s spawn video game"))return 1;
    if(eq(game,"street fighter 2 new challengers")&&
       eq(cover,"super street fighter ii new challengers"))return 1;
    if(eq(game,"adventures of tintin prisoners of sun")&&
       eq(cover,"tintin prisoners of sun"))return 1;
    if(eq(game,"super shell monsters story")&&
       eq(cover,"daikaijuu monogatari"))return 1;
    return 0;
}
static int ranking(const Parts *g,const Parts *c)
{
    int score=100;
    if(!g->key[0]||!aliases(g->key,c->key))return -1;
    if(g->demo!=c->demo)return -1;
    if(g->hack!=c->hack)return -1; /* cannot replace a custom hack with the vanilla game */
    /* Prefer authentic standard editions to promotion/demo/reissue
     * cover variants, even if the ROM has an informal translation tag. */
    score -= c->edition * 30;
    if(!g->beta && c->beta)score-=30;
    if(!g->rev && c->rev)score-=15;
    if(!g->extra && c->extra)score-=20;
    if(g->region) {
        if(g->region==c->region)score+=100;
        else if(g->region==1&&c->region==2)score+=20;
        else if(g->region==2&&c->region==1)score+=20;
        else if(c->region==4)score+=10;
    } else {
        if(c->region==1)score+=80;
        else if(c->region==2)score+=60;
        else if(c->region==4)score+=45;
        else if(c->region==3)score+=25;
    }
    return score;
}
/* V15.35: enumerate multiple real verified box-art files for the SAME
 * title. Prefer retail / same-region, but try other official regions if the
 * first image is missing or portrait. Never return unrelated games. */
int snes_name_catalog_matches(const char *rom_path,const char *index,size_t len,
                              char out[][256],int max)
{
    Parts game,cand;
    char line[NAME_MAX];
    int scores[8],found=0,k;
    size_t at=0,end,sz;
    if(!out || max<1 || max>8 || !rom_path || !index || !len)return 0;
    for(k=0;k<max;k++){out[k][0]=0;scores[k]=-1;}
    parts(rom_path,&game);
    if(!game.key[0])return 0;
    while(at<len){
        int sc,pos;
        end=at;
        while(end<len&&index[end]!='\n'&&index[end]!='\r')end++;
        sz=end-at;
        if(sz>4&&sz<sizeof(line)){
            memcpy(line,index+at,sz);line[sz]=0;
            if(sf_ends_with_i(line,".png")){
                parts(line,&cand);
                sc=ranking(&game,&cand);
                /* Stable score then lexicographic filename order. Do not
                 * let a special "(Caravan...)" title replace a retail one. */
                for(pos=0;sc>=0 && pos<max;pos++)
                    if(sc>scores[pos] || (sc==scores[pos] &&
                       strcmp(line,out[pos])<0))break;
                /* Invalid title score -1 must NEVER enter a region list:
                 * otherwise an unrelated game's box can overwrite this ROM. */
                if(sc>=0 && pos<max){
                    int j;
                    for(j=max-1;j>pos;j--){
                        scores[j]=scores[j-1];
                        memcpy(out[j],out[j-1],256);
                    }
                    scores[pos]=sc;
                    sf_strlcpy(out[pos],line,256);
                    if(found<max)found++;
                }
            }
        }
        at=end;
        while(at<len&&(index[at]=='\n'||index[at]=='\r'))at++;
    }
    return found;
}
int snes_name_catalog_match(const char *rom_path,const char *index,size_t len,
                            char *filename,size_t cap)
{
    char picked[1][256];
    if(!filename || !cap)return 0;
    filename[0]=0;
    if(snes_name_catalog_matches(rom_path,index,len,picked,1)!=1)return 0;
    if(strlen(picked[0])+1>cap)return 0;
    sf_strlcpy(filename,picked[0],cap);
    return 1;
}
int snes_name_catalog_url_kind(const char *image_filename,int kind,char *url,size_t cap)
{
    static const char *const prefixes[SNES_ART_KINDS] = {
        "https://thumbnails.libretro.com/Nintendo%20-%20Super%20Nintendo%20Entertainment%20System/Named_Boxarts/",
        "https://thumbnails.libretro.com/Nintendo%20-%20Super%20Nintendo%20Entertainment%20System/Named_Titles/",
        "https://thumbnails.libretro.com/Nintendo%20-%20Super%20Nintendo%20Entertainment%20System/Named_Logos/",
        "https://thumbnails.libretro.com/Nintendo%20-%20Super%20Nintendo%20Entertainment%20System/Named_Snaps/"
    };
    const char *prefix_url;
    size_t plen;
    static const char hex[]="0123456789ABCDEF";
    size_t i,pos;
    if(!url||!cap)return 0;
    url[0]=0;
    if(kind<0||kind>=SNES_ART_KINDS)return 0;
    prefix_url=prefixes[kind];
    plen=strlen(prefix_url);
    if(cap<=plen)return 0;
    if(!image_filename||!sf_ends_with_i(image_filename,".png")||
       strchr(image_filename,'/')||strchr(image_filename,'\\'))return 0;
    memcpy(url,prefix_url,plen+1);pos=plen;
    for(i=0;image_filename[i];i++){
        unsigned char c=(unsigned char)image_filename[i];
        if(alp(c)||dig(c)||c=='-'||c=='_'||c=='.'||c=='('||c==')'){
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

int snes_name_catalog_url(const char *image_filename,char *url,size_t cap)
{
    return snes_name_catalog_url_kind(image_filename,SNES_ART_BOX,url,cap);
}
