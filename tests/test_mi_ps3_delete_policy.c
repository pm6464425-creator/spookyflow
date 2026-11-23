/* MI PS3 safety regressions: independent checks before any console unlink. */
#include <assert.h>
#include <stdio.h>
#include "layout.h"
#include "sf_delete_policy.h"
#define OK(t,p,d) assert(sf_delete_path_allowed((t),(p),(d))==SF_DEL_OK)
#define NO(t,p,d) assert(sf_delete_path_allowed((t),(p),(d))!=SF_DEL_OK)
int main(void)
{
    /* Six platform roots and all directly supported extensions */
    OK(PLAT_PS1,"/dev_hdd0/PSXISO/Castlevania.cue",0);
    OK(PLAT_PS1,"/dev_hdd0/PSXISO/Castlevania.bin",0);
    OK(PLAT_PS1,"/dev_hdd0/PSXGAMES/Game.iso",0);
    OK(PLAT_PS2,"/dev_hdd0/PS2ISO/Game.iso",0);
    OK(PLAT_PS2,"/dev_hdd0/PS2ISO/Game.iso.0",0);
    OK(PLAT_PS3,"/dev_hdd0/PS3ISO/Game.iso",0);
    OK(PLAT_PS3,"/dev_hdd0/GAMES/BLES12345",1);
    OK(PLAT_PS3,"/dev_hdd0/GAMEZ/BLUS12345",1);
    OK(PLAT_BDISO,"/dev_hdd0/BDISO/Movie.iso",0);
    OK(PLAT_SNES,"/dev_hdd0/SNES/Cooly Skunk (ESP).sfc",0);
    OK(PLAT_SNES,"/dev_hdd0/SNES/Classic Kong.smc",0);
    OK(PLAT_SNES,"/dev_hdd0/SNES/Super Bomberman.zip",0);
    OK(PLAT_ARCADE,"/dev_hdd0/ARCADE/kof2002.zip",0);
    OK(PLAT_ARCADE,"/dev_hdd0/ARCADE/Metal Slug X.7z",0);
    OK(PLAT_BDISO,"/dev_usb000/BDISO/Movie.iso",0);
    OK(PLAT_SNES,"/dev_usb001/SNES/Game.sfc",0);
    OK(PLAT_PS3,"/dev_usb007/GAMES/BCES11111",1);
    OK(PLAT_PS1,"/dev_hdd0/PSXISO/Dir/Resident Evil.cue",0);
    /* Any path outside the known game roots is forbidden */
    NO(PLAT_PS3,"/dev_hdd0/game/SPKF00001/USRDIR/EBOOT.BIN",0);
    NO(PLAT_PS3,"/dev_hdd0/game/SPKF00001",1);
    NO(PLAT_PS3,"/dev_hdd0/GAMES",1);
    NO(PLAT_PS3,"/dev_hdd0/GAMES/",1);
    NO(PLAT_PS3,"/dev_hdd0/GAMES/Folder/Subdir",1);
    NO(PLAT_PS3,"/dev_hdd0/GAMES/Folder.iso",0);
    NO(PLAT_SNES,"/dev_hdd0/tmp/spookyflow/Downloaded.sfc",0);
    NO(PLAT_BDISO,"/dev_hdd0/tmp/wmtmp/Movie.iso.ntfs[BDISO]",0);
    NO(PLAT_ARCADE,"/dev_hdd0/ARCADE/COVERS/kof2002.zip",0);
    NO(PLAT_SNES,"/dev_hdd0/SNES/COVERS/File.sfc",0);
    NO(PLAT_SNES,"/dev_hdd0/SNES/../GAMES/Game.sfc",0);
    NO(PLAT_SNES,"/dev_hdd0/SNES/./File.sfc",0);
    NO(PLAT_SNES,"/dev_hdd0/SNES/Folder//File.sfc",0);
    NO(PLAT_SNES,"/dev_hdd0/SNES/Evil\\File.sfc",0);
    NO(PLAT_SNES,"/dev_hdd0/SNES/Game.mp4",0);
    NO(PLAT_ARCADE,"/dev_hdd0/ARCADE/Game.iso",0);
    NO(PLAT_BDISO,"/dev_hdd0/BDISO/Movie.mkv",0);
    NO(PLAT_PS2,"/dev_hdd0/PSXISO/Game.iso",0);
    NO(PLAT_PS1,"/dev_hdd0/PS2ISO/Game.cue",0);
    NO(PLAT_SNES,"/dev_usb008/SNES/Game.sfc",0);
    NO(PLAT_SNES,"/dev_flash/SNES/Game.sfc",0);
    NO(PLAT_SNES,"/dev_hdd0/SNES/",0);
    NO(PLAT_SNES,"/dev_hdd0/SNES/Game.sfc",1);
    NO(PLAT_PS3,"/dev_hdd0/GAMES/BCES.sfc",1);
    NO(PLAT_PS3,"/dev_hdd0/GAMES/./BCES123",1);
    NO(PLAT_PS3,"/dev_hdd0/GAMES/../game",1);
    NO(PLAT_PS3,"/dev_hdd0/GAMES/BCES123/../../../game",1);
    NO(PLAT_PS3,"/dev_hdd0/GAMES/BCES123/",1);
    NO(PLAT_PS3,"/dev_hdd0/GAMES/.hidden/BCES123",1);
    NO(PLAT_PS3,"/dev_hdd0/GAMES/BCES123\n",1);
    NO(PLAT_PS3,"/dev_hdd0/PS3ISO/Game.iso.9",0);
    printf("PASS: 48 independent authorized/forbidden deletion-path checks\n");
    return 0;
}
