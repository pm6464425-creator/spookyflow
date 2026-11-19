#include "arcade_native_select.h"
#include "arcade_engine.h"
#include <stdio.h>
#include <assert.h>
#include <string.h>
#define ROOT "/tmp/arcade_native_route_test/"
int main(void)
{
    char r[160];
    int engine=arcade_native_engine_select(ROOT "kof2002.zip",
             ARCADE_ENGINE_FBNEO,r,sizeof r);
    assert(engine==ARCADE_ENGINE_FBNEO_LEGACY);
    assert(strstr(r,"SELECT_FBNEO_LEGACY"));
    printf("PASS legacy BIOS + original KOF2002 CRC => isolated FBNeo legacy (%s)\n",r);
    /* Regression for user's real ZIPs: a 3-file clone backed by 14-file
     * kof2002.zip was previously rejected as "CRC_MISMATCH 3_OF_14". */
    engine=arcade_native_engine_select(ROOT "kf2k2mp.zip",
             ARCADE_ENGINE_FBNEO,r,sizeof r);
    assert(engine==ARCADE_ENGINE_FBNEO_LEGACY);
    assert(strstr(r,"NEOGEO_CLON_CRC_OK_PADRE=kof2002"));
    assert(strstr(r,"HEREDADOS=11/14"));
    puts("PASS kf2k2mp clone: 3 own + 11 inherited parent chips => legacy FBNeo");
    engine=arcade_native_engine_select(ROOT "kf2k2pla.zip",
             ARCADE_ENGINE_FBNEO,r,sizeof r);
    assert(engine==ARCADE_ENGINE_FBNEO_LEGACY);
    assert(strstr(r,"NEOGEO_CLON_CRC_OK_PADRE=kof2002"));
    puts("PASS V15.74 previously missing alias: kf2k2pla ZIP 2 own chips + kof2002 parent => verified legacy FBNeo");
    /* Without parent, do NOT claim clone works. */
    assert(rename(ROOT "kof2002.zip",ROOT "kof2002_parent_backup.zip")==0);
    engine=arcade_native_engine_select(ROOT "kf2k2mp.zip",
             ARCADE_ENGINE_FBNEO,r,sizeof r);
    assert(engine==ARCADE_ENGINE_NONE);
    assert(strstr(r,"NEOGEO_CLON_FALTA_PADRE=kof2002"));
    assert(rename(ROOT "kof2002_parent_backup.zip",ROOT "kof2002.zip")==0);
    /* Parent exists, but one clone chip CRC differs => still reject. */
    assert(rename(ROOT "kf2k2mp.zip",ROOT "kf2k2mp_good_backup.zip")==0);
    assert(rename(ROOT "kf2k2mp_wrong.zip",ROOT "kf2k2mp.zip")==0);
    engine=arcade_native_engine_select(ROOT "kf2k2mp.zip",
             ARCADE_ENGINE_FBNEO,r,sizeof r);
    assert(engine==ARCADE_ENGINE_NONE);
    assert(strstr(r,"NEOGEO_CLON_CRC_INCOMPLETO"));
    assert(rename(ROOT "kf2k2mp.zip",ROOT "kf2k2mp_wrong.zip")==0);
    assert(rename(ROOT "kf2k2mp_good_backup.zip",ROOT "kf2k2mp.zip")==0);
    puts("PASS bad/missing clone parent/CRC rejected before any emulator starts");
    /* Distinct NeoGeo title, proving compatibility is no longer KOF-only. */
    engine=arcade_native_engine_select(ROOT "kof98.zip",
             ARCADE_ENGINE_FBNEO,r,sizeof r);
    assert(engine==ARCADE_ENGINE_FBNEO_LEGACY);
    assert(strstr(r,"SELECT_FBNEO_LEGACY"));
    printf("PASS legacy BIOS + KOF98 CRC => isolated FBNeo legacy (%s)\n",r);
    engine=arcade_native_engine_select(ROOT "not_in_map.zip",
             ARCADE_ENGINE_NONE,r,sizeof r);
    assert(engine==ARCADE_ENGINE_NONE);
    assert(strstr(r,"ROM_NO_INCLUIDA_EN_BASE_MOTORES_PS3"));
    engine=arcade_native_engine_select(ROOT "gng.7z",
             ARCADE_ENGINE_NONE,r,sizeof r);
    assert(engine==ARCADE_ENGINE_NONE);
    assert(strstr(r,"7Z_NO_ADMITIDO"));
    engine=arcade_native_engine_select(ROOT "gng.zip",
              ARCADE_ENGINE_FBNEO,r,sizeof r);
    assert(engine==ARCADE_ENGINE_FBNEO);
    engine=arcade_native_engine_select(ROOT "kof2002_missing.zip",
              ARCADE_ENGINE_FBNEO,r,sizeof r);
    assert(engine==ARCADE_ENGINE_FBNEO);
    engine=arcade_native_engine_select(ROOT "kof2002.zip",
              ARCADE_ENGINE_MAME125,r,sizeof r);
    assert(engine==ARCADE_ENGINE_MAME125);
    /* Modern BIOS must keep FBNeo for KOF2002; never choose MAME blindly. */
    assert(rename(ROOT "neogeo.zip",ROOT "neogeo_old_backup.zip")==0);
    assert(rename(ROOT "neogeo_modern.zip",ROOT "neogeo.zip")==0);
    engine=arcade_native_engine_select(ROOT "kof2002.zip",
              ARCADE_ENGINE_FBNEO,r,sizeof r);
    assert(engine==ARCADE_ENGINE_FBNEO);
    assert(strstr(r,"MODERN_CORRECT"));
    assert(rename(ROOT "neogeo.zip",ROOT "neogeo_modern.zip")==0);
    assert(rename(ROOT "neogeo_old_backup.zip",ROOT "neogeo.zip")==0);
    /* Simulate missing one declared chip with metadata-only ZIP fixture. */
    assert(rename(ROOT "kof2002.zip",ROOT "kof2002_full_backup.zip")==0);
    assert(rename(ROOT "kof2002_missing.zip",ROOT "kof2002.zip")==0);
    engine=arcade_native_engine_select(ROOT "kof2002.zip",
              ARCADE_ENGINE_FBNEO,r,sizeof r);
    assert(engine==ARCADE_ENGINE_NONE);
    assert(strstr(r,"CRC_MISMATCH"));
    assert(rename(ROOT "kof2002.zip",ROOT "kof2002_missing.zip")==0);
    assert(rename(ROOT "kof2002_full_backup.zip",ROOT "kof2002.zip")==0);
    puts("PASS modern BIOS=>FBNeo; missing chip=>no unsafe launch; unrelated arcade unchanged");
    return 0;
}
