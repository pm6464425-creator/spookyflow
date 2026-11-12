/* TMDB-powered movie artwork; external USB BDISO/COVERS only.
 * Requires the owner's TMDB v3 API key in BDISO/COVERS/TMDB_KEY.TXT.
 * Never saves movie artwork to the PS3's internal disk. */
#ifndef SF_BDISO_ART_H
#define SF_BDISO_ART_H
int sf_bdiso_ensure_art(const char *iso_path);
/* A manual library rescan retries failed online poster requests. */
void sf_bdiso_reset_attempts(void);
#endif
