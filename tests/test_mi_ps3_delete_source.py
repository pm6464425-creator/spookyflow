"""MI PS3: source-only regression gates. No console file writes."""
from pathlib import Path
import subprocess
import re

def check(ok,msg):
    assert ok,msg
    print("PASS",msg)

base="origin/fix/v15-74-arcade-crc-detection-33-clones"
def old(path):
    return subprocess.check_output(["git","show",f"{base}:{path}"],text=True)
def now(path):
    return Path(path).read_text()

menu=now("src/core/menu.c")
header=now("src/core/menu.h")
app=now("src/core/app.c")
app_h=now("src/core/app.h")
main=now("src/ps3/main.c")
ui=now("src/core/ui.c")
backend=now("src/ps3/sf_delete_game.c")
policy=now("src/core/sf_delete_policy.c")

check("ROW_G_DELETE" in header and "MENU_FX_DELETE" in header,"menu and action enums")
check('case ROW_G_DELETE: return "Eliminar";' in menu,"exact Spanish label")
check("tmp[n++] = ROW_G_DELETE;" in menu,"all tabs have delete row")
check(menu.index("tmp[n++] = ROW_G_DELETE;") < menu.index("} else if (panel == PANEL_FLOW)"),"Triangle menu placement")
check("k == ROW_G_DELETE || menu_is_group" in menu,"X activates action")
check("dir == 0 ? MENU_FX_DELETE" in menu,"not triggered by left/right")
check("int (*delete_game)(int platform" in app_h,"host-isolated callback")
check("int delete_confirm;" in app_h,"confirmation state")
check("a->delete_confirm=1;" in app,"no immediate delete from the row")
check("if(in->pressed&SFB_CIRCLE)" in app,"Circle cancels confirmation")
check("else if(in->pressed&SFB_CROSS)" in app,"second X required")
check("a->host.delete_game(a->panel_plat,path" in app,"real selected game path")
check("g->flags&GAME_DEMO" in app,"built-in demo protected")
check("g->ndisc>1" in app and "g->ndisc<=1" in app,"multidisc protected")
check("a->usb_auto_rescan=1;" in app,"existing background rescan, no unrelated NTFS refresh")
check("rc==0" in app and "No se pudo eliminar" in app,"success/failure distinguished")
check("sf_delete_path_allowed(platform,path,is_dir)" in backend,"strict path policy enforced")
check("delete_cue_bundle(path)" in backend,"CUE and same-directory tracks")
check("delete_tree(path,0,&n)" in backend,"PS3 folder deletion")
check("delete_split_iso(path)" in backend,"split ISO handled together")
check("sysFsRmdir(dir)" in backend,"directory roots removed only after contents")
check("SF_DEL_TREE_LIMIT" in backend,"corrupt tree bounded")
check('strcmp(root,"BDISO")' in policy,"BDISO root restricted")
check('strcmp(root,"ARCADE")' in policy,"Arcade root restricted")
check('sf_del_comp_eq(segment,len,"COVERS")' in policy,"cover folders protected")
check('"/dev_hdd0/"' in policy and '"/dev_usb"' in policy,"known storage roots only")
check("draw_delete_confirm(a);" in ui,"modal on top of unchanged flow")
check("host.delete_game = sf_delete_game;" in main,"runtime callback wired")
check(now("sfo.xml").count("MI PS3")==1,"installed title preserved")
check('TITLE     := MI PS3' in now("Makefile"),"Makefile title preserved")

original_main=old("src/ps3/main.c")
expected_main=original_main.replace('#include "ps3_platform.h"',
                                    '#include "ps3_platform.h"\n#include "sf_delete_game.h"').replace(
    '    host.remove_file = ps3_fs_remove_file;',
    '    host.remove_file = ps3_fs_remove_file;\n    host.delete_game = sf_delete_game; /* isolated user-confirmed menu action */')
check(main==expected_main,"all existing platform launch code byte-for-byte unchanged")

protected=[
    "src/core/scanner.c","src/core/library.c",
    "src/core/coverart.c","src/core/cover_cache.c",
    "src/ps3/ps3_fs.c","src/ps3/sf_ps2_hen.c",
    "src/ps3/sf_cobra_mount.c",
]
for path in protected:
    check(now(path)==old(path),f"protected unchanged: {path}")

changed=subprocess.check_output(["git","diff","--name-only",base,"HEAD"],text=True).splitlines()
allowed={
    ".github/workflows/mi-ps3-delete.yml",
    "src/core/sf_delete_policy.h","src/core/sf_delete_policy.c",
    "src/ps3/sf_delete_game.h","src/ps3/sf_delete_game.c",
    "src/core/app.h","src/core/app.c","src/core/menu.h",
    "src/core/menu.c","src/core/ui.c","src/ps3/main.c",
    "tests/test_mi_ps3_delete_policy.c",
    "tests/test_mi_ps3_delete_backend.c",
    "tests/mi_ps3_fs_stub/sys/file.h",
    "tests/test_mi_ps3_delete_source.py",
    "tests/verify_compiled_pkg.py","tools/build_ps1_boxart_catalog.py","tools/build_snes_boxart_catalog.py","Makefile","sfo.xml",
}
check(set(changed).issubset(allowed),"only approved files changed")
print("PASS: MI PS3 deletion source safety audit complete")
