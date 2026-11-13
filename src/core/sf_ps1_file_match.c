/* Generic PS1 CUE BIN/IMG resolver. V15.28.
 * The old region-only normalization rejected ordinary Redump filenames:
 *   "(PS1) Disney Tarzan [PAL] [SCES-01519].BIN"
 *   "Grand Theft Auto 2 (Europe) (En,Fr,De,Es,It) (Rev 1).bin"
 * after users renamed their image to a shorter same-game filename.
 *
 * Never match based on a game whitelist or a fuzzy substring. Strip only
 * recognized, well-delimited release metadata and compare full normalized
 * titles. Disc/track numbers and contradictory IDs/revisions are protected.
 * A tie at the best evidence rank means ambiguity and is always rejected.
 */
#include "sf_ps1_file_match.h"
#include <string.h>
#include <strings.h>
#include <ctype.h>
#include <stdio.h>

static int disc_image_extension(const char *name)
{
    const char *dot=name?strrchr(name,'.'):NULL;
    return dot && (!strcasecmp(dot,".bin")||!strcasecmp(dot,".img"));
}
static void compact(const char *p,size_t n,char *out,size_t cap)
{
    size_t k=0,i;
    for(i=0;i<n && k+1<cap;i++){
        unsigned char c=(unsigned char)p[i];
        if(isalnum(c) || c>=128)out[k++]=(char)(c<128?tolower(c):c);
    }
    out[k]=0;
}
static int one_of(const char *word,const char *const *values,size_t count)
{
    size_t i;
    for(i=0;i<count;i++)if(!strcmp(word,values[i]))return 1;
    return 0;
}
static int region_tag(const char *word)
{
    static const char *const names[]={
        "usa","europe","japan","world","asia",
        "pal","ntsc","ntscu","ntscj","france","germany","spain",
        "italy","australia","korea","brazil","uk","us","eu","jp",
        "unitedstates","unitedkingdom","english","french","german",
        "spanish","italian","portuguese","russian","korean","japanese",
        "multi5","multi6","multi7","multilanguage",
        "ps1","psx","playstation","playstation1","sonyplaystation"
    };
    return one_of(word,names,sizeof(names)/sizeof(names[0]));
}
static int language_code(const char *code)
{
    static const char *const names[]={
        "en","fr","de","es","it","pt","nl","ru","ja","jp","ko",
        "zh","sv","no","da","fi","pl","cs","cz","hu","tr","el",
        "ar","he","th","id","ms","br"
    };
    return one_of(code,names,sizeof(names)/sizeof(names[0]));
}
static int language_tag(const char *text,size_t n)
{
    char token[24];size_t p=0;unsigned count=0;
    /* Tags such as "(En,Fr,De,Es,It)" and "[En+Fr+Es]".
       Require at least two explicit language codes. */
    while(p<n){
        size_t k=0;
        while(p<n && (text[p]==' ' || text[p]=='\t'))p++;
        while(p<n && isalpha((unsigned char)text[p])){
            if(k+1>=sizeof(token))return 0;
            token[k++]=(char)tolower((unsigned char)text[p++]);
        }
        if(!k)return 0;
        token[k]=0;
        if(!language_code(token))return 0;
        count++;
        while(p<n && (text[p]==' ' || text[p]=='\t'))p++;
        if(p==n)break;
        if(text[p]!=',' && text[p]!='+' && text[p]!=';')return 0;
        p++;
    }
    return count>=2;
}
static int serial_tag(const char *word,char *out,size_t cap)
{
    static const char *const prefixes[]={
        "sces","sles","scus","slus","scps","slps","slpm","sips",
        "sced","sled","scds","slka","papx","pbpx","sczs","slus"
    };
    size_t i,n=strlen(word),digits=0;
    if(n<8 || n>=cap)return 0;
    if(!one_of(word,prefixes,sizeof(prefixes)/sizeof(prefixes[0])))
    {
        /* Compare the 4-letter prefix only: SCES-01519 -> sces01519 */
        char prefix[5];
        memcpy(prefix,word,4);prefix[4]=0;
        if(!one_of(prefix,prefixes,sizeof(prefixes)/sizeof(prefixes[0])))
            return 0;
    }
    for(i=4;i<n;i++){
        if(!isdigit((unsigned char)word[i]))return 0;
        digits++;
    }
    if(digits<4 || digits>9)return 0;
    memcpy(out,word,n+1);
    return 1;
}
static int parse_marker_number(const char *word,const char *prefix,
                               unsigned *out)
{
    size_t n=strlen(prefix);unsigned value=0;
    const char *p=word+n;
    if(strncmp(word,prefix,n)!=0 || !*p)return 0;
    for(;*p;p++){
        if(!isdigit((unsigned char)*p))return 0;
        value=value*10+(unsigned)(*p-'0');
        if(value>99)return 0;
    }
    if(value==0)return 0;
    *out=value;
    return 1;
}
static int known_tag(const char *text,size_t n,SfPs1MatchKey *key)
{
    char word[SF_PS1_MATCH_NAME_MAX];
    char serial[SF_PS1_MATCH_TAG_MAX];
    unsigned num;
    compact(text,n,word,sizeof(word));
    if(!word[0])return 0;
    if(region_tag(word) || language_tag(text,n))return 1;
    if(serial_tag(word,serial,sizeof(serial))){
        strcpy(key->serial,serial);
        return 1;
    }
    if(parse_marker_number(word,"disc",&num) ||
       parse_marker_number(word,"disk",&num) ||
       parse_marker_number(word,"cd",&num)){
        key->has_disc=1;key->disc=num;return 1;
    }
    if(parse_marker_number(word,"track",&num)){
        key->has_track=1;key->track=num;return 1;
    }
    if(parse_marker_number(word,"rev",&num) ||
       parse_marker_number(word,"revision",&num)){
        snprintf(key->rev,sizeof(key->rev),"%u",num);
        return 1;
    }
    return 0;
}
static int normalize(const char *name,SfPs1MatchKey *key)
{
    const char *ext=strrchr(name,'.'),*p=name;
    size_t w=0;
    if(!disc_image_extension(name) || !ext || !key)return 0;
    memset(key,0,sizeof(*key));
    while(p<ext){
        unsigned char c=(unsigned char)*p;
        if(c=='(' || c=='['){
            const char close=c=='('?')':']';
            const char *end=memchr(p+1,close,(size_t)(ext-p-1));
            if(end && known_tag(p+1,(size_t)(end-p-1),key)){
                p=end+1;continue;
            }
        }
        /* Common possessive title spelling: "Disney's Tarzan" and
           "Disney Tarzan" refer to the same base title. */
        if(c=='\'' && p+2<ext && (p[1]=='s'||p[1]=='S') &&
           (p[2]==' '||p[2]=='-'||p[2]=='('||p[2]=='[')){
            p+=2;continue;
        }
        if(isalnum(c) || c>=128){
            if(w+2>=sizeof(key->title))return 0;
            key->title[w++]=(char)(c<128?tolower(c):c);
        }
        p++;
    }
    key->title[w]=0;
    /* Some users store an image by serial only: SCES_015.19.bin.
       In that case the filename is an identifier, not a game title. */
    {
        char serial[SF_PS1_MATCH_TAG_MAX];
        if(serial_tag(key->title,serial,sizeof(serial))){
            strcpy(key->serial,serial);
            key->title[0]=0;
            return 1;
        }
    }
    return w>0 || key->serial[0]!=0;
}
static int compatible(const SfPs1MatchKey *a,const SfPs1MatchKey *b)
{
    if(a->has_disc!=b->has_disc ||
       (a->has_disc && a->disc!=b->disc))return 0;
    if(a->has_track!=b->has_track ||
       (a->has_track && a->track!=b->track))return 0;
    if(a->serial[0] && b->serial[0] && strcmp(a->serial,b->serial))return 0;
    if(a->rev[0] && b->rev[0] && strcmp(a->rev,b->rev))return 0;
    return 1;
}
int sf_ps1_match_begin(SfPs1FileMatch *m,const char *expected)
{
    size_t n;
    if(!m || !expected || !(n=strlen(expected)) ||
       n>=sizeof(m->expected) || !disc_image_extension(expected))return -1;
    memset(m,0,sizeof(*m));
    memcpy(m->expected,expected,n+1);
    return normalize(expected,&m->expected_key)?0:-1;
}
void sf_ps1_match_consider(SfPs1FileMatch *m,const char *candidate)
{
    SfPs1MatchKey key;
    size_t n;unsigned score=0;
    if(!m || !candidate || !disc_image_extension(candidate) ||
       !(n=strlen(candidate)) || n>=SF_PS1_MATCH_NAME_MAX)return;
    m->scanned++;
    if(!normalize(candidate,&key))return;
    if(!compatible(&m->expected_key,&key))return;
    if(!strcasecmp(m->expected,candidate))score=10;
    else if(m->expected_key.title[0] && key.title[0] &&
            !strcmp(m->expected_key.title,key.title)){
        score=2;
        if(m->expected_key.serial[0] && key.serial[0])score++;
        if(m->expected_key.rev[0] && key.rev[0])score++;
    }else if(m->expected_key.serial[0] && key.serial[0] &&
             !strcmp(m->expected_key.serial,key.serial) &&
             (!m->expected_key.title[0] || !key.title[0])){
        score=1; /* serial-only file, exact matching PS1 ID */
    }
    if(!score || score<m->top_score)return;
    if(score>m->top_score){
        m->top_score=score;
        m->same_score=1;
        memcpy(m->winner,candidate,n+1);
    }else if(strcmp(m->winner,candidate)!=0){
        /* Two physical filenames at the same confidence: NEVER guess. */
        m->same_score++;
    }
}
int sf_ps1_match_finish(const SfPs1FileMatch *m,char *out,size_t cap)
{
    size_t n;
    if(!m || !out || !cap)return -1;
    out[0]=0;
    if(!m->top_score)return -1;
    if(m->same_score!=1)return -2;
    n=strlen(m->winner);
    if(n>=cap)return -1;
    memcpy(out,m->winner,n+1);
    return 0;
}
