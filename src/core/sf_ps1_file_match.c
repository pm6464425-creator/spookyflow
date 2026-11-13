/* PS1 CUE reference resolver: portable matching rules.
 * Applied only if the exact file referenced by CUE is absent.
 * Score 3: identical (case-insensitive); score 2: same full title after
 * stripping explicit geographic region tokens. Keep disc and track
 * numbers, game revisions and modifications intact to prevent mixing
 * mismatched audio. Multiple same-score matches -> ambiguous, reject.
 */
#include "sf_ps1_file_match.h"
#include <string.h>
#include <strings.h>
#include <ctype.h>

static int disc_image_extension(const char *name)
{
    const char *dot=strrchr(name,'.');
    return dot && (!strcasecmp(dot,".bin")||!strcasecmp(dot,".img"));
}
static int region_marker(const char *text,size_t n)
{
    static const char *const tags[]={
        "usa","europe","japan","world","asia",
        "pal","ntsc","ntsc-u","ntsc-j","ntsc u","ntsc j",
        "france","germany","spain","italy","australia",
        "korea","brazil","uk","us","eu","jp",
        "united states","united kingdom"
    };
    unsigned i;char val[64];size_t start=0,end=n;
    while(start<end && (text[start]==' '||text[start]=='\t'))start++;
    while(end>start && (text[end-1]==' '||text[end-1]=='\t'))end--;
    if(end-start>=sizeof(val))return 0;
    for(i=0;i<end-start;i++)
        val[i]=(char)tolower((unsigned char)text[start+i]);
    val[i]=0;
    for(i=0;i<sizeof(tags)/sizeof(tags[0]);i++)
        if(!strcmp(val,tags[i]))return 1;
    return 0;
}
static int normalized(const char *name,char out[SF_PS1_MATCH_NAME_MAX])
{
    const char *ext=strrchr(name,'.'),*p=name;size_t w=0;
    if(!disc_image_extension(name) || !ext)return 0;
    while(p<ext){
        unsigned char c=(unsigned char)*p;
        if(c=='(' || c=='['){
            char close=c=='('?')':']';
            const char *end=memchr(p+1,close,(size_t)(ext-p-1));
            if(end && region_marker(p+1,(size_t)(end-p-1))){
                p=end+1;
                continue;
            }
        }
        if(isalnum(c) || c>=128){
            if(w+2>=SF_PS1_MATCH_NAME_MAX)return 0;
            out[w++]=(char)(c<128?tolower(c):c);
        }
        p++;
    }
    out[w]=0;
    return w>0;
}
int sf_ps1_match_begin(SfPs1FileMatch *m,const char *expected)
{
    size_t n;
    if(!m || !expected || !(n=strlen(expected)) ||
       n>=sizeof(m->expected) || !disc_image_extension(expected))return -1;
    memset(m,0,sizeof(*m));
    memcpy(m->expected,expected,n+1);
    if(!normalized(expected,m->expected_norm))return -1;
    return 0;
}
void sf_ps1_match_consider(SfPs1FileMatch *m,const char *candidate)
{
    size_t n;unsigned score=0;
    char candidate_norm[SF_PS1_MATCH_NAME_MAX];
    if(!m || !candidate || !disc_image_extension(candidate) ||
       !(n=strlen(candidate)) || n>=SF_PS1_MATCH_NAME_MAX)return;
    m->scanned++;
    if(!strcasecmp(m->expected,candidate))score=3;
    else if(normalized(candidate,candidate_norm) &&
            !strcmp(m->expected_norm,candidate_norm))score=2;
    if(!score || score<m->top_score)return;
    if(score>m->top_score){
        m->top_score=score;
        m->same_score=1;
        memcpy(m->winner,candidate,n+1);
    }else if(strcasecmp(m->winner,candidate)!=0){
        /* Two different names with equal confidence. Refuse guessing. */
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
