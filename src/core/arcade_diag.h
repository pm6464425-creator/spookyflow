/* SINGLE MAME/FBNeo report, entirely separate from the original consoles. */
#ifndef SF_ARCADE_DIAG_H
#define SF_ARCADE_DIAG_H
#ifndef SF_ARCADE_DIAG_PATH
#define SF_ARCADE_DIAG_PATH "/dev_hdd0/tmp/spookyflow/diagnostico_mame.txt"
#endif
#ifndef SF_ARCADE_ROM_MANIFEST
#define SF_ARCADE_ROM_MANIFEST "/dev_hdd0/game/SPKF00001/USRDIR/ARCADE/fbneo_ps3_crc_manifest.tsv"
#endif
/* Rewrite one TXT on EACH X press. Return 0 for success; -1 on IO failure.
 * The function never renames, extracts, alters, deletes or copies a ROM.
 * Uses authentic FBNeo native PS3 core revision fccba95f2 to audit ZIPs.
 */
int arcade_diag_begin(const char *rom_path,int core,
                      const char *core_file,const char *reload_file,
                      const char *config_file);
void arcade_diag_event(const char *stage,int rc,const char *detail);
#endif
