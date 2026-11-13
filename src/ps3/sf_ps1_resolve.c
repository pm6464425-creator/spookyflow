/* PS3 filesystem adapter for PS1 CUE filename resolution.
 * V15.26: Metal Slug X.cue -> "Metal Slug X (Europe).bin" returned ENOENT.
 * Search for a same-title BIN in that folder without modifying the CUE.
 * Ambiguous matches are explicitly rejected to protect multi-disc/track
 * collections and avoid mounting the wrong game.
 */
#include "sf_ps1_resolve.h"
#include "sf_ps1_file_match.h"
#include "log.h"
#include <stdio.h>
#include <string.h>
#include <strings.h>
#include <dirent.h>
#include <sys/stat.h>
#include <errno.h>

#define SF_PS1_RESOLVER_PATH 1024
/* Static: PS3 PPU main thread's stack is small. */
static SfPs1FileMatch s_name_match;
static char s_dirname[SF_PS1_RESOLVER_PATH];
static char s_candidate[SF_PS1_MATCH_NAME_MAX];
static char s_result[SF_PS1_RESOLVER_PATH];

static int copy_file_path(const char *src,char *dst,size_t cap)
{
    size_t n=strlen(src);
    if(n>=cap)return -1;
    memcpy(dst,src,n+1);return 0;
}
int sf_ps1_resolve_existing(const char *expected,char *out,size_t cap)
{
    struct stat st;
    const char *slash;
    size_t dirlen;
    DIR *dir;
    struct dirent *entry;
    unsigned scanned;
    int rc;
    if(!expected || !out || !cap || expected[0]!='/')return -3;
    out[0]=0;
    if(stat(expected,&st)==0){
        if(!S_ISREG(st.st_mode))return -3;
        return copy_file_path(expected,out,cap);
    }
    if(errno!=ENOENT){
        log_printf("PS1_DIAG STAGE=CUE_REFERENCE RESULT=STAT_ERROR "
                   "ERRNO=%d EXPECTED=%s",errno,expected);
        return -3;
    }
    slash=strrchr(expected,'/');
    if(!slash || !slash[1])return -3;
    dirlen=(size_t)(slash-expected+1);
    if(dirlen>=sizeof(s_dirname) ||
       sf_ps1_match_begin(&s_name_match,slash+1)!=0)return -3;
    memcpy(s_dirname,expected,dirlen);
    s_dirname[dirlen]=0;
    dir=opendir(s_dirname);
    if(!dir){
        log_printf("PS1_DIAG STAGE=CUE_DIR_SCAN RESULT=OPEN_FAILED "
                   "ERRNO=%d DIR=%s",errno,s_dirname);
        return -3;
    }
    while((entry=readdir(dir))!=NULL)
        sf_ps1_match_consider(&s_name_match,entry->d_name);
    closedir(dir);
    scanned=s_name_match.scanned;
    rc=sf_ps1_match_finish(&s_name_match,s_candidate,sizeof(s_candidate));
    if(rc!=0){
        log_printf("PS1_DIAG STAGE=CUE_REFERENCE RESULT=%s "
                   "EXPECTED=%s CANDIDATES=%u TOP_SCORE=%u TOP_TIES=%u",
                   rc==-2?"AMBIGUOUS":"NOT_FOUND",expected,scanned,
                   s_name_match.top_score,s_name_match.same_score);
        return rc;
    }
    if(snprintf(s_result,sizeof(s_result),"%s%s",s_dirname,s_candidate)
       >=(int)sizeof(s_result) || stat(s_result,&st)!=0 ||
       !S_ISREG(st.st_mode)){
        log_printf("PS1_DIAG STAGE=CUE_REFERENCE RESULT=NOT_A_REGULAR_FILE "
                   "CANDIDATE=%s",s_candidate);
        return -3;
    }
    log_printf("PS1_DIAG STAGE=CUE_REFERENCE RESULT=RECOVERED "
               "EXPECTED=%s FOUND=%s MATCH=%s CANDIDATES=%u",
               expected,s_result,
               s_name_match.top_score==3?"CASE_INSENSITIVE":"REGION_NORMALIZED",
               scanned);
    return copy_file_path(s_result,out,cap);
}
