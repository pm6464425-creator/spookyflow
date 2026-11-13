#ifndef SF_PS1_FILE_MATCH_H
#define SF_PS1_FILE_MATCH_H
#include <stddef.h>
/* Generic safe CUE FILE reference matching, not per-game exceptions.
 * Match 1: actual file exists; handled by caller.
 * Match 2: same filename ignoring case.
 * Match 3: unique same title after removing known release metadata.
 * Match 4: exact matching PS1 serial when filename is serial-only.
 * Disc and track numbers MUST agree. Conflicting explicit serial/revision
 * values are rejected. Any equally ranked candidates -> ambiguous.
 * Works only in the same directory, never guesses among different games. */
#define SF_PS1_MATCH_NAME_MAX 512
#define SF_PS1_MATCH_TAG_MAX 48
typedef struct {
    char title[SF_PS1_MATCH_NAME_MAX];
    char serial[SF_PS1_MATCH_TAG_MAX];
    char rev[SF_PS1_MATCH_TAG_MAX];
    unsigned disc;
    unsigned track;
    unsigned has_disc;
    unsigned has_track;
} SfPs1MatchKey;
typedef struct {
    char expected[SF_PS1_MATCH_NAME_MAX];
    SfPs1MatchKey expected_key;
    char winner[SF_PS1_MATCH_NAME_MAX];
    unsigned top_score;
    unsigned same_score;
    unsigned scanned;
} SfPs1FileMatch;
int sf_ps1_match_begin(SfPs1FileMatch *m,const char *expected_basename);
void sf_ps1_match_consider(SfPs1FileMatch *m,const char *candidate_basename);
int sf_ps1_match_finish(const SfPs1FileMatch *m,char *out,size_t cap);
#endif
