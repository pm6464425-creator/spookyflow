#ifndef SF_PS1_FILE_MATCH_H
#define SF_PS1_FILE_MATCH_H
#include <stddef.h>
/* Portable, deterministic resolution of a BIN reference in a PS1 .CUE.
 * A missing referenced file can only be replaced by one UNIQUE same-title
 * file in the SAME directory. Never guesses among ambiguous regions/discs.
 *
 * Return values:
 *  0: matching candidate returned by sf_ps1_match_finish
 * -1: none found
 * -2: ambiguous (multiple equally valid filenames)
 */
#define SF_PS1_MATCH_NAME_MAX 512
typedef struct {
    char expected[SF_PS1_MATCH_NAME_MAX];
    char expected_norm[SF_PS1_MATCH_NAME_MAX];
    char winner[SF_PS1_MATCH_NAME_MAX];
    unsigned top_score;
    unsigned same_score;
    unsigned scanned;
} SfPs1FileMatch;
int sf_ps1_match_begin(SfPs1FileMatch *m,const char *expected_basename);
void sf_ps1_match_consider(SfPs1FileMatch *m,const char *candidate_basename);
int sf_ps1_match_finish(const SfPs1FileMatch *m,char *out,size_t cap);
#endif
